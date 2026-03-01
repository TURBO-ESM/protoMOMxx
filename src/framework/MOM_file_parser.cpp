#include "MOM_file_parser.h"
#include "MOM_string_functions.h"

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

/// @brief Find the first occurrence of a character that is not inside quotes. Handles both single and double quotes.
/// @param s The string to search through.
/// @param ch The character to find.
/// @return The index of the first unquoted occurrence of ch, or std::string_view::npos if not found.
static std::size_t find_unquoted(std::string_view s, char ch) {
  char in_quote = 0; // 0, '\'' or '"'
  for (std::size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (in_quote) {
      if (c == in_quote) in_quote = 0;
    } else {
      if (c == '"' || c == '\'') in_quote = c;
      else if (c == ch) return i;
    }
  }
  return std::string_view::npos;
}

/// @brief Split a string by commas that are not inside quotes. Handles both single and double quotes.
/// @param s The string to split.
/// @return A vector of string_view parts.
static std::vector<std::string_view> split_commas_top_level(std::string_view s)
{
  std::vector<std::string_view> parts;
  std::size_t start = 0;

  while (start < s.size()) {
    std::string_view tail = s.substr(start);
    std::size_t comma_rel = find_unquoted(tail, ',');
    if (comma_rel == std::string_view::npos) break;

    parts.push_back(tail.substr(0, comma_rel));
    start += comma_rel + 1;
  }

  parts.push_back(s.substr(start));
  return parts;
}

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

/// @brief Parse a quoted string, ensuring it is well-formed and stripping the quotes.
/// @param s The input string view containing the quoted string.
/// @param line_no The line number for error reporting.
/// @param path The file path for error reporting.
/// @return The parsed string with quotes removed.
static std::string parse_quoted_string(std::string_view s, std::size_t line_no, const std::string& path) {
  s = trim(s);
  if (s.size() < 2) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": expected quoted string");
  }
  char q = s.front();
  if (!((q == '"') || (q == '\'')) || s.back() != q) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": malformed quoted string");
  }

  std::string out;
  out.reserve(s.size() - 2);

  for (std::size_t i = 1; i + 1 < s.size(); ++i) {
    out.push_back(s[i]);
  }
  return out;
}

/// @brief Try to parse a string as a boolean value.
///          Recognizes "true", "t", "1" as true and "false", "f", "0" as false (case-insensitive).
/// @param s The input string view to parse.
/// @param out Reference to a boolean variable where the parsed value will be stored if parsing is successful. 
/// @return true if parsing was successful and out was set, false otherwise.
static bool try_parse_bool(std::string_view s, bool& out) {
  auto sl = lowercase(trim(s));
  if (sl == "true" || sl == "t")  { out = true; return true; }
  if (sl == "false"|| sl == "f")  { out = false; return true; }
  return false;
}

/// @brief Try to parse a string as a 64-bit integer using std::from_chars.
/// @param s The input string view to parse.
/// @param out Reference to a std::int64_t variable where the parsed value will be stored if parsing is successful.
/// @return true if parsing was successful and out was set, false otherwise. 
static bool try_parse_int64(std::string_view s, std::int64_t& out) {
  s = trim(s);
  if (s.empty()) return false;
  // from_chars doesn't accept leading '+' reliably across old libs; handle explicitly:
  if (s.front() == '+') s.remove_prefix(1);
  std::int64_t v{};
  auto* b = s.data();
  auto* e = s.data() + s.size();
  auto res = std::from_chars(b, e, v, 10);
  if (res.ec != std::errc{} || res.ptr != e) return false;
  out = v;
  return true;
}

/// @brief Try to parse a string as a double using std::stod, ensuring the entire string is consumed.
/// @param s The input string view to parse.
/// @param out Reference to a double variable where the parsed value will be stored if parsing is successful.
/// @return true if parsing was successful and out was set, false otherwise.
static bool try_parse_double(std::string_view s, double& out) {
  std::string tmp(trim(s));
  if (tmp.empty()) return false;
  try {
    std::size_t pos = 0;
    double v = std::stod(tmp, &pos);
    if (pos != tmp.size()) return false;
    out = v;
    return true;
  } catch (const std::invalid_argument&) {
    return false;
  } catch (const std::out_of_range&) {
    return false;
  }
}

/// @brief Parse a scalar value from a string, trying bool, int64, double, and quoted string (in that order).
///        If the string does not match any of these types, it is treated as an unquoted string (whitespace disallowed).
/// @param s The input string view to parse.
/// @param line_no The line number for error reporting.
/// @param path The file path for error reporting.
/// @return The parsed value as a ParamValue variant.
static ParamValue parse_scalar(std::string_view s, std::size_t line_no, const std::string& path) {
  s = trim(s);
  if (s.empty()) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": empty value");
  }

  // Quoted string
  if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')) {
    return parse_quoted_string(s, line_no, path);
  }

  // Bool
  bool b{};
  if (try_parse_bool(s, b)) return b;

  // Int
  std::int64_t i{};
  if (try_parse_int64(s, i)) return i;

  // Double
  double d{};
  if (try_parse_double(s, d)) return d;

  // Bare string: reject spaces
  if (s.find_first_of(" \t\r\n") != std::string_view::npos) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": unquoted string contains whitespace: '" + std::string(s) + "'");
  }
  return std::string(s);
}

/// @brief Parse a value which can be either a scalar or a comma-separated list of scalars.
///        Enforces that all items in a list are of the same type.
/// @param raw The input string view containing the raw value to parse.
/// @param line_no The line number for error reporting.
/// @param path The file path for error reporting.
/// @return The parsed value as a ParamValue variant, which may be a scalar or a vector of scalars.
static ParamValue get_value(std::string_view raw, std::size_t line_no, const std::string& path) {
  raw = trim(raw);
  if (raw.empty()) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": missing value");
  }

  // List?
  auto parts = split_commas_top_level(raw);
  if (parts.size() == 1) {
    return parse_scalar(parts[0], line_no, path);
  }

  // Parse each as scalar, then enforce homogeneous type and return vector<T>.
  std::vector<ParamValue> scalars;
  scalars.reserve(parts.size());
  for (auto p : parts) scalars.push_back(parse_scalar(p, line_no, path));

  const std::size_t idx = scalars.front().index();
  for (const auto& v : scalars) {
    if (v.index() != idx) {
      throw std::runtime_error(
        path + ":" + std::to_string(line_no) +
        ": mixed-type list is not allowed (e.g. int + string)."
      );
    }
  }

  switch (idx) {
    case 0: { // bool
      std::vector<bool> out;
      out.reserve(scalars.size());
      for (auto& v : scalars) out.push_back(std::get<bool>(v));
      return out;
    }
    case 1: { // int64
      std::vector<std::int64_t> out;
      out.reserve(scalars.size());
      for (auto& v : scalars) out.push_back(std::get<std::int64_t>(v));
      return out;
    }
    case 2: { // double
      std::vector<double> out;
      out.reserve(scalars.size());
      for (auto& v : scalars) out.push_back(std::get<double>(v));
      return out;
    }
    case 3: { // string
      std::vector<std::string> out;
      out.reserve(scalars.size());
      for (auto& v : scalars) out.push_back(std::get<std::string>(v));
      return out;
    }
    default:
      // Should never happen because parse_scalar only returns scalar types.
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": internal error parsing list");
  }
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
