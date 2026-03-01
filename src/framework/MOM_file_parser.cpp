#include "MOM_file_parser.h"

using mom_parser_utilities::find_unquoted;
using mom_parser_utilities::get_value;

namespace {

struct ParsedTarget {
  std::string module;
  std::string key;
};

/// @brief Parse key target syntax for assignment-like statements.
///
/// Supports either:
/// - key
/// - module%key (only when not inside an open module block)
///
/// @param lhs The raw left-hand-side key expression.
/// @param curr_module The currently open module name, if any.
/// @param line_no Current line number for diagnostics.
/// @param path File path for diagnostics.
/// @return ParsedTarget containing resolved module and key.
static ParsedTarget parse_target(std::string_view lhs,
                                 std::string_view curr_module,
                                 std::size_t line_no,
                                 const std::string& path) {
  lhs = trim(lhs);
  if (lhs.empty()) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": empty key");
  }

  ParsedTarget out;
  out.module = std::string(curr_module);

  auto pct = lhs.find('%');
  if (pct != std::string_view::npos) {
    if (!curr_module.empty()) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": cannot use module%key inside an open module: '" + std::string(lhs) + "'");
    }

    auto mod = trim(lhs.substr(0, pct));
    auto key = trim(lhs.substr(pct + 1));

    if (key.find('%') != std::string_view::npos) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": only one '%' allowed in module%key: '" + std::string(lhs) + "'");
    }

    if (!is_valid_identifier(mod) || !is_valid_identifier(key)) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": invalid module%key syntax: '" + std::string(lhs) + "'");
    }

    out.module = std::string(mod);
    out.key = std::string(key);
    return out;
  }

  if (!is_valid_identifier(lhs)) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": invalid key name: '" + std::string(lhs) + "'");
  }

  out.key = std::string(lhs);
  return out;
}

} // namespace

/// @brief Strip comments from a line in a quote-aware manner.
///
/// Handles:
/// - C-style block comments (`/* ... */`)
/// - Fortran-style line comments introduced by `!`
///
/// @param line The input line to process.
/// @param in_block Reference to a boolean indicating whether parsing is currently
///        inside a C-style block comment. This value is updated by the function
///        and must be preserved across successive calls.
/// @return The input line with comments removed.
static std::string strip_comments(std::string_view line, bool& in_block) {
  std::string out;
  out.reserve(line.size());

  bool in_sq = false;
  bool in_dq = false;

  for (std::size_t i = 0; i < line.size();) {
    char c = line[i];

    // Handle active block comment
    if (in_block) {
      if (c == '*' && (i + 1) < line.size() && line[i + 1] == '/') {
        in_block = false;
        i += 2;
      } else {
        ++i;
      }
      continue;
    }

    // Update quote state
    if (!in_dq && c == '\'') {
      in_sq = !in_sq;
      out.push_back(c);
      ++i;
      continue;
    }

    if (!in_sq && c == '"') {
      in_dq = !in_dq;
      out.push_back(c);
      ++i;
      continue;
    }

    // Start of block comment? (only if not in quotes)
    if (!in_sq && !in_dq &&
        c == '/' && (i + 1) < line.size() && line[i + 1] == '*') {
      in_block = true;
      i += 2;
      continue;
    }

    // Inline '!' comment? (only if not in quotes)
    if (!in_sq && !in_dq && c == '!') {
      break; // ignore rest of line
    }

    out.push_back(c);
    ++i;
  }

  return out;
}

RuntimeParams::RuntimeParams(const std::string& path) : path_(path) {
  namespace fs = std::filesystem;

  if (!fs::exists(path_)) {
    throw std::runtime_error("Runtime parameter file not found: " + path_);
  }
  if (!fs::is_regular_file(path_)) {
    throw std::runtime_error("Runtime parameter path is not a regular file: " + path_);
  }

  std::ifstream infile(path_);
  if (!infile) {
    throw std::runtime_error("Unable to open runtime parameter file at " + path_);
  }

  std::string curr_module;
  bool in_block_comment = false;

  std::string raw_line;
  std::size_t line_no = 0;

  auto assign_param = [&](const std::string& module,
                          const std::string& key,
                          ParamValue value,
                          bool is_override) {
    auto& mod_table = table_[module];
    auto it = mod_table.find(key);

    if (is_override) {
      if (it == mod_table.end()) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": #override requires an existing key: '" +
                                 (module.empty() ? key : module + "%" + key) + "'");
      }
      it->second = std::move(value);
      return;
    }

    if (it == mod_table.end()) {
      mod_table.emplace(key, std::move(value));
      return;
    }

    // Keep repeated assignments only when they agree exactly.
    if (it->second != value) {
      throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": duplicate assignment for '" +
                               (module.empty() ? key : module + "%" + key) +
                               "' with a different value; use #override to change it");
    }
  };

  while (std::getline(infile, raw_line)) {
    ++line_no;

    // Strip comments in a quote-aware way (handles /*...*/ across lines + ! inline).
    std::string line = strip_comments(raw_line, in_block_comment);
    std::string_view sv = trim(line);

    if (sv.empty()) continue;

    // Module open: name%
    if (sv.back() == '%' && sv.front() != '%') {
      auto name = trim(sv.substr(0, sv.size() - 1));
      if (!is_valid_identifier(name)) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": invalid module name: '" + std::string(name) + "'");
      }
      if (!curr_module.empty()) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": cannot open module '" + std::string(name) +
                                 "' inside already-open module '" + curr_module + "'");
      }
      curr_module = std::string(name);
      continue;
    }

    // Module close: %name
    if (sv.front() == '%' && sv.size() >= 2) {
      auto name = trim(sv.substr(1));
      if (!is_valid_identifier(name)) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": invalid module close: '%" + std::string(name) + "'");
      }
      if (curr_module != name) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": module mismatch: closing '" +
                                 std::string(name) + "' but open module is '" + curr_module + "'");
      }
      curr_module.clear();
      continue;
    }

    // Directive: #cmd args...
    // Supported directives: #define, #undef, #override
    if (sv.front() == '#') {
      auto rest = trim(sv.substr(1));
      if (rest.empty()) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": malformed directive: '" + std::string(sv) + "'");
      }
      auto cmd_end = rest.find_first_of(" \t");
      std::string cmd = lowercase(trim(cmd_end == std::string_view::npos ? rest : rest.substr(0, cmd_end)));
      auto args = trim(cmd_end == std::string_view::npos ? std::string_view{} : rest.substr(cmd_end + 1));
      if (args.empty()) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": directive '#" + std::string(cmd) + "' requires arguments");
      }
      
      auto lhs_end = args.find_first_of(" \t\r");
      auto lhs = trim(lhs_end == std::string_view::npos ? args : args.substr(0, lhs_end));
      if (lhs.empty()) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": directive '#" + std::string(cmd) + "' requires a key");
      }
      auto target = parse_target(lhs, curr_module, line_no, path_);

      auto rhs = trim(lhs_end == std::string_view::npos ? std::string_view{} : args.substr(lhs_end + 1));
      
      if (cmd == "define") {
        if (rhs.empty()) {
          // #define key  (no value) means set bool to true
          assign_param(target.module, target.key, ParamValue(true), false);
        } else {
          // #define key value means parse value as usual
          assign_param(target.module, target.key, get_value(rhs, line_no, path_), false);
        }
        continue;
      }

      if (cmd == "undef") {
        bool is_override = false;
        // if a value was already provided, it must be "true":
        auto it = table_[target.module].find(target.key);
        if (it != table_[target.module].end()) {
          const auto& existing_val = it->second;
          if (!std::holds_alternative<bool>(existing_val) || !std::get<bool>(existing_val)) {
            throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": #undef can only be applied to keys that are currently defined as true; use #override to change other values");
          }
          is_override = true;
        } 
        assign_param(target.module, target.key, ParamValue(false), is_override);
        continue;
      }

      if (cmd == "override") {
        if (rhs.empty()) {
          throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": #override requires a value");
        }
        if (rhs.front() != '=') {
          throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": expected '=' after key in #override directive");
        }
        rhs.remove_prefix(1); // remove '='
        assign_param(target.module, target.key, get_value(rhs, line_no, path_), true);
        continue;
      }

      throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": unknown directive '#" + std::string(cmd) + "'");
    }

    // key = value (find '=' not in quotes)
    auto eq = find_unquoted(sv, '=');
    if (eq == std::string_view::npos) {
      throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": expected 'key = value' but got: '" + std::string(sv) + "'");
    }

    auto lhs = trim(sv.substr(0, eq));
    auto rhs = trim(sv.substr(eq + 1));

    if (lhs.empty()) {
      throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": empty key in: '" + std::string(sv) + "'");
    }

    auto target = parse_target(lhs, curr_module, line_no, path_);
    assign_param(target.module, target.key, get_value(rhs, line_no, path_), false);
  }

  if (in_block_comment) {
    throw std::runtime_error(path_ + ":EOF: unterminated block comment (missing '*/')");
  }
  if (!curr_module.empty()) {
    throw std::runtime_error(path_ + ":EOF: unterminated module '" + curr_module + "' (missing '%" + curr_module + "')");
  }
}

template<typename T>
T RuntimeParams::get_as(const std::string& key, const std::string& module) const {
  const auto& val = get(key, module);
  if (std::holds_alternative<T>(val)) {
    return std::get<T>(val);
  }
  throw std::runtime_error("Parameter " + module + ":" + key + " is not of the requested type");
}



const ParamValue& RuntimeParams::get(const std::string& key, const std::string& module) const {
  auto mod_it = table_.find(module);
  if (mod_it == table_.end()) {
    throw std::out_of_range("Module not found: " + module);
  }
  
  auto key_it = mod_it->second.find(key);
  if (key_it == mod_it->second.end()) {
    throw std::out_of_range("Key not found in module " + module + ": " + key);
  }
  
  return key_it->second;
}

bool RuntimeParams::has_param(const std::string& key, 
                              const std::string& module) const {
  auto mod_it = table_.find(module);
  if (mod_it == table_.end()) {
    return false;
  }
  return mod_it->second.find(key) != mod_it->second.end();
}

std::vector<std::string> RuntimeParams::get_modules() const {
  std::vector<std::string> modules;
  modules.reserve(table_.size());
  for (const auto& pair : table_) {
    modules.push_back(pair.first);
  }
  return modules;
}

size_t RuntimeParams::get_num_parameters() const {
  size_t count = 0;
  for (const auto& mod_pair : table_) {
    count += mod_pair.second.size();
  }
  return count;
}

// Explicit template instantiations for get_as
template bool RuntimeParams::get_as<bool>(const std::string&, const std::string&) const;
template std::int64_t RuntimeParams::get_as<std::int64_t>(const std::string&, const std::string&) const;
template double RuntimeParams::get_as<double>(const std::string&, const std::string&) const;
template std::string RuntimeParams::get_as<std::string>(const std::string&, const std::string&) const;
template std::vector<bool> RuntimeParams::get_as<std::vector<bool>>(const std::string&, const std::string&) const;
template std::vector<std::int64_t> RuntimeParams::get_as<std::vector<std::int64_t>>(const std::string&, const std::string&) const;
template std::vector<double> RuntimeParams::get_as<std::vector<double>>(const std::string&, const std::string&) const;
template std::vector<std::string> RuntimeParams::get_as<std::vector<std::string>>(const std::string&, const std::string&) const;
