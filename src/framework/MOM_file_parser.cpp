#include "MOM_file_parser.h"
#include "MOM_logger.h"
#include "MOM_string_utils.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace MOM {

using string_utils::find_unquoted;
using string_utils::trim;
using string_utils::lowercase;
using string_utils::is_valid_identifier;
using parser_utils::get_value;
using parser_utils::NotFound;

namespace {

struct ParsedTarget {
  std::string block;
  std::string key;
};

/// @brief Parse key target syntax for assignment-like statements.
///
/// Supports either:
/// - key
/// - block%key (only when not inside an open block)
///
/// @param lhs The raw left-hand-side key expression.
/// @param curr_block The currently open block name, if any.
/// @param line_no Current line number for diagnostics.
/// @param path File path for diagnostics.
/// @return ParsedTarget containing resolved block and key.
ParsedTarget parse_target(std::string_view lhs, std::string_view curr_block, std::size_t line_no,
                                 const std::string &path) {
  lhs = trim(lhs);
  if (lhs.empty()) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": empty key");
  }

  ParsedTarget out;
  out.block = std::string(curr_block);

  auto pct = lhs.find('%');
  if (pct != std::string_view::npos) {
    if (!curr_block.empty()) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) +
                               ": cannot use block%key inside an open block: '" + std::string(lhs) + "'");
    }

    auto blk = trim(lhs.substr(0, pct));
    auto key = trim(lhs.substr(pct + 1));

    if (key.find('%') != std::string_view::npos) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": only one '%' allowed in block%key: '" +
                               std::string(lhs) + "'");
    }

    if (!is_valid_identifier(blk) || !is_valid_identifier(key)) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": invalid block%key syntax: '" +
                               std::string(lhs) + "'");
    }

    out.block = std::string(blk);
    out.key = std::string(key);
    return out;
  }

  if (!is_valid_identifier(lhs)) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": invalid key name: '" + std::string(lhs) + "'");
  }

  out.key = std::string(lhs);
  return out;
}

/// @brief Strip block and line comments from a line in a quote-aware manner.
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
std::string strip_block_comments(std::string_view line, bool &in_block) {
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
    if (!in_sq && !in_dq && c == '/' && (i + 1) < line.size() && line[i + 1] == '*') {
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

/// @brief Parse a runtime parameter file and add its contents to the provided table.
/// @param path The file path of the runtime parameter file to parse.
/// @param table The ParamTable to which parsed parameters will be added.
void add_data_from_file(const std::string &path, ParamTable &table) {
  namespace fs = std::filesystem;

  logger::info("  Reading parameter file: ", path);

  if (!fs::exists(path)) {
    throw std::runtime_error("Runtime parameter file not found: " + path);
  }
  if (!fs::is_regular_file(path)) {
    throw std::runtime_error("Runtime parameter path is not a regular file: " + path);
  }

  std::ifstream infile(path);
  if (!infile) {
    throw std::runtime_error("Unable to open runtime parameter file at " + path);
  }

  std::string curr_block;
  bool in_block_comment = false;

  std::string raw_line;
  std::size_t line_no = 0;

  auto assign_param = [&](const std::string &block, const std::string &key, ParamValue value, bool is_override) {
    try {
      table.insert(key, block, std::move(value), is_override);
    } catch (const std::runtime_error &e) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": " + e.what());
    }
  };

  while (std::getline(infile, raw_line)) {
    ++line_no;

    // Strip block and line comments in a quote-aware way (handles /*...*/ across lines + ! inline).
    std::string line = strip_block_comments(raw_line, in_block_comment);
    std::string_view sv = trim(line);

    if (sv.empty())
      continue;

    // Block open: name%
    if (sv.back() == '%' && sv.front() != '%') {
      auto name = trim(sv.substr(0, sv.size() - 1));
      if (!is_valid_identifier(name)) {
        throw std::runtime_error(path + ":" + std::to_string(line_no) + ": invalid block name: '" + std::string(name) +
                                 "'");
      }
      if (!curr_block.empty()) {
        throw std::runtime_error(path + ":" + std::to_string(line_no) + ": cannot open block '" + std::string(name) +
                                 "' inside already-open block '" + curr_block + "'");
      }
      curr_block = std::string(name);
      continue;
    }

    // Block close: %name
    if (sv.front() == '%' && sv.size() >= 2) {
      auto name = trim(sv.substr(1));
      if (!is_valid_identifier(name)) {
        throw std::runtime_error(path + ":" + std::to_string(line_no) + ": invalid block close: '%" +
                                 std::string(name) + "'");
      }
      if (curr_block != name) {
        throw std::runtime_error(path + ":" + std::to_string(line_no) + ": block mismatch: closing '" +
                                 std::string(name) + "' but open block is '" + curr_block + "'");
      }
      curr_block.clear();
      continue;
    }

    // Directive: #cmd args...
    // Supported directives: #define, #undef, #override
    if (sv.front() == '#') {
      auto rest = trim(sv.substr(1));
      if (rest.empty()) {
        throw std::runtime_error(path + ":" + std::to_string(line_no) + ": malformed directive: '" + std::string(sv) +
                                 "'");
      }
      auto cmd_end = rest.find_first_of(" \t");
      std::string cmd = lowercase(trim(cmd_end == std::string_view::npos ? rest : rest.substr(0, cmd_end)));
      auto args = trim(cmd_end == std::string_view::npos ? std::string_view{} : rest.substr(cmd_end + 1));
      if (args.empty()) {
        throw std::runtime_error(path + ":" + std::to_string(line_no) + ": directive '#" + std::string(cmd) +
                                 "' requires arguments");
      }

      auto lhs_end = args.find_first_of(" \t\r");
      auto lhs = trim(lhs_end == std::string_view::npos ? args : args.substr(0, lhs_end));
      if (lhs.empty()) {
        throw std::runtime_error(path + ":" + std::to_string(line_no) + ": directive '#" + std::string(cmd) +
                                 "' requires a key");
      }
      auto target = parse_target(lhs, curr_block, line_no, path);

      auto rhs = trim(lhs_end == std::string_view::npos ? std::string_view{} : args.substr(lhs_end + 1));

      if (cmd == "define") {
        if (rhs.empty()) {
          // #define key  (no value) means set bool to true
          assign_param(target.block, target.key, ParamValue(true), false);
        } else {
          if (rhs.front() == '=') {
            throw std::runtime_error(path + ":" + std::to_string(line_no) +
                                     ": #define does not use '='; write '#define " + target.key + " <value>' or '" +
                                     target.key + " = <value>'");
          }
          // #define key value means parse value as usual
          assign_param(target.block, target.key, get_value(rhs, line_no, path), false);
        }
        continue;
      }

      if (cmd == "undef") {
        if (!rhs.empty()) {
          throw std::runtime_error(path + ":" + std::to_string(line_no) +
                                   ": #undef takes only a key name, but got trailing content: '" + std::string(rhs) +
                                   "'");
        }
        bool is_override = false;
        // if a value was already provided, it must be "true":
        if (table.has_param(target.key, target.block)) {
          const auto &existing_val = table.get_variant(target.key, target.block);
          if (!std::holds_alternative<bool>(existing_val) || !std::get<bool>(existing_val)) {
            throw std::runtime_error(path + ":" + std::to_string(line_no) +
                                     ": #undef can only be applied to keys that are currently defined as true; use "
                                     "#override to change other values");
          }
          is_override = true;
        }
        assign_param(target.block, target.key, ParamValue(false), is_override);
        continue;
      }

      if (cmd == "override") {
        if (rhs.empty()) {
          throw std::runtime_error(path + ":" + std::to_string(line_no) + ": #override requires a value");
        }
        if (rhs.front() != '=') {
          throw std::runtime_error(path + ":" + std::to_string(line_no) +
                                   ": expected '=' after key in #override directive");
        }
        rhs.remove_prefix(1); // remove '='
        assign_param(target.block, target.key, get_value(rhs, line_no, path), true);
        continue;
      }

      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": unknown directive '#" + std::string(cmd) +
                               "'");
    }

    // key = value (find '=' not in quotes)
    auto eq = find_unquoted(sv, '=');
    if (eq == std::string_view::npos) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": expected 'key = value' but got: '" +
                               std::string(sv) + "'");
    }

    auto lhs = trim(sv.substr(0, eq));
    auto rhs = trim(sv.substr(eq + 1));

    if (lhs.empty()) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": empty key in: '" + std::string(sv) + "'");
    }

    auto target = parse_target(lhs, curr_block, line_no, path);
    assign_param(target.block, target.key, get_value(rhs, line_no, path), false);
  }

  if (in_block_comment) {
    throw std::runtime_error(path + ":EOF: unterminated block comment (missing '*/')");
  }
  if (!curr_block.empty()) {
    throw std::runtime_error(path + ":EOF: unterminated block '" + curr_block + "' (missing '%" + curr_block + "')");
  }
}

} // anonymous namespace

RuntimeParams::RuntimeParams(const std::string &path, const std::string &doc_file_base) {
  if (!doc_file_base.empty()) {
    doc_ = std::make_unique<DocFileWriter>(doc_file_base);
  }
  add_data_from_file(path, table_);
}

RuntimeParams::RuntimeParams(const std::vector<std::string> &paths, const std::string &doc_file_base) {
  if (!doc_file_base.empty()) {
     doc_ = std::make_unique<DocFileWriter>(doc_file_base);
  }
  for (const auto &path : paths) {
    add_data_from_file(path, table_);
  }
}

bool RuntimeParams::has_param(const std::string &key) const {
  return table_.has_param(key, current_block_);
}

std::vector<std::string> RuntimeParams::get_blocks() const {
  return table_.get_groups();
}

size_t RuntimeParams::get_num_parameters() const {
  return table_.get_num_parameters();
}

const ParamValue &RuntimeParams::get_variant(const std::string &key) const {
  return table_.get_variant(key, current_block_);
}

void RuntimeParams::open_block(const std::string &blockName, const std::string &desc) {
  if (blockName.empty() || !string_utils::is_valid_identifier(blockName)) {
    throw std::invalid_argument("open_block: invalid block name \"" + blockName + "\"");
  }
  if (!current_block_.empty()) {
    throw std::logic_error("open_block(\"" + blockName + "\") called but block \"" + current_block_ + "\" is already open");
  }
  current_block_ = blockName;
  if (doc_) doc_->open_block(blockName, desc);
}

void RuntimeParams::close_block() {
  if (current_block_.empty()) throw std::logic_error("close_block() called but no block is open");
  current_block_.clear();
  if (doc_) doc_->close_block();
}

void RuntimeParams::doc_module(const std::string &modname, const std::string &desc, bool layout_mod,
    bool debugging_mod, bool all_default) {
  if (doc_) {
    doc_->doc_module(modname, desc, layout_mod, debugging_mod, all_default);
  } else {
    logger::warning("Attempted to document module '", modname,
                  "' but no documentation writer is attached. Provide a non-empty doc_file_base when constructing RuntimeParams to enable documentation.");
  }
}

void RuntimeParams::close_module() {
  if (doc_) {
    doc_->close_module();
  } else {
    logger::warning("Attempted to close module documentation but no documentation writer is attached. "
        "Provide a non-empty doc_file_base when constructing RuntimeParams to enable documentation.");
  }
}


template <typename T>
void RuntimeParams::get(const std::string &key, T &value, const ParamGetOptions &options) const {
  bool value_was_set = false;
  auto full_key = [&]() { return current_block_.empty() ? key : current_block_ + "%" + key; };

  if (!options.do_not_read) {
    try {
      const auto &val = get_variant(key);
      if (std::holds_alternative<T>(val)) {
        value = std::get<T>(val);
        value_was_set = true;
      } else {
        throw std::runtime_error("Parameter " + full_key() + " is not of the requested type");
      }
    } catch (const std::out_of_range &) {
      if (options.fail_if_missing) {
        throw std::out_of_range("Key not found" +
            (current_block_.empty() ? std::string{} : " in block " + current_block_) + ": " + key);
      }
    }
  }

  if (!value_was_set && options.default_value.has_value()) {
    if (!std::holds_alternative<T>(options.default_value.value())) {
      throw std::runtime_error("Default value for " + full_key() + " is not of the requested type");
    }
    value = std::get<T>(options.default_value.value());
    value_was_set = true;
  }

  if (!value_was_set && !options.do_not_read) {
    logger::warning("Parameter ", full_key(), " was not found and no default value was provided. "
                    "The variable will retain its previous value.");
  }

  // Document the parameter if a doc writer is attached and desc is provided
  if (value_was_set && doc_ && !options.do_not_log && !options.desc.empty()) {
    DocParamOptions doc_opts;
    doc_opts.layout_param = options.layout_param;
    doc_opts.debugging_param = options.debugging_param;

    // Convert ParamValue variant to the appropriate typed optional
    std::optional<T> typed_default;
    if (options.default_value.has_value() && std::holds_alternative<T>(options.default_value.value())) {
      typed_default = std::get<T>(options.default_value.value());
    }

    doc_->doc_param(key, options.desc, options.units, value, typed_default, doc_opts);
  }
}

// Explicit template instantiations for get
template void RuntimeParams::get<bool>(const std::string &, bool &, const ParamGetOptions &) const;
template void RuntimeParams::get<int>(const std::string &, int &, const ParamGetOptions &) const;
template void RuntimeParams::get<double>(const std::string &, double &, const ParamGetOptions &) const;
template void RuntimeParams::get<std::string>(const std::string &, std::string &,
                                              const ParamGetOptions &) const;
template void RuntimeParams::get<std::vector<bool>>(const std::string &, std::vector<bool> &,
                                                    const ParamGetOptions &) const;
template void RuntimeParams::get<std::vector<int>>(const std::string &, std::vector<int> &,
                                                            const ParamGetOptions &) const;
template void RuntimeParams::get<std::vector<double>>(const std::string &, std::vector<double> &,
                                                      const ParamGetOptions &) const;
template void RuntimeParams::get<std::vector<std::string>>(const std::string &, std::vector<std::string> &,
                                                           const ParamGetOptions &) const;

} // namespace MOM
