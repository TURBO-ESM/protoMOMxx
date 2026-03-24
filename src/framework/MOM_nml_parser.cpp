#include "MOM_nml_parser.h"
#include "MOM_parser_utils.h"
#include "MOM_string_utils.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <variant>

namespace MOM {

using parser_utils::get_value;
using parser_utils::strip_comments;
using string_utils::find_unquoted;
using string_utils::is_valid_identifier;
using string_utils::lowercase;
using string_utils::quotes_balanced;
using string_utils::trim;

namespace {

/// @brief Find the end of the current assignment's RHS in a line that may
/// contain multiple comma-separated assignments (e.g. `A=1, B=2`).
/// Returns the position of the separating comma, or rhs_full.size() if none.
size_t find_rhs_end(std::string_view rhs_full) {
  char in_quote = 0;

  for (size_t pos = 0; pos < rhs_full.size(); ++pos) {
    char c = rhs_full[pos];

    if (in_quote) {
      if (c == in_quote)
        in_quote = 0;
      continue;
    }

    if (c == '"' || c == '\'') {
      in_quote = c;
      continue;
    }

    if (c == ',') {
      auto after = trim(rhs_full.substr(pos + 1));

      if (auto neq = find_unquoted(after, '='); neq != std::string_view::npos) {
        auto name = trim(after.substr(0, neq));
        if (is_valid_identifier(name)) {
          return pos;
        }
      }
    }
  }

  return rhs_full.size();
}

/// @brief Process one or more assignments in a line and insert them into the
/// table.
void process_assignments(std::string_view sv, const std::string &curr_namelist,
                         std::size_t line_no, const std::string &path,
                         ParamTable &table) {
  while (!sv.empty()) {
    auto eq = find_unquoted(sv, '=');
    if (eq == std::string_view::npos)
      return;

    auto lhs = trim(sv.substr(0, eq));
    if (lhs.empty()) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) +
                               ": empty variable name");
    }

    auto rhs_full = sv.substr(eq + 1);
    auto rhs_end = find_rhs_end(rhs_full);
    auto rhs = trim(rhs_full.substr(0, rhs_end));

    try {
      table.insert(lowercase(std::string(lhs)), curr_namelist,
                   get_value(rhs, line_no, path));
    } catch (const std::runtime_error &e) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": " +
                               e.what());
    }

    if (rhs_end < rhs_full.size()) {
      sv = trim(rhs_full.substr(rhs_end + 1));
    } else {
      break;
    }
  }
}

/// @brief Parse a Fortran namelist file and add its contents to the provided
/// table.
/// @param path The file path of the namelist file to parse.
/// @param table The ParamTable to which parsed parameters will be added.
void add_data_from_file(const std::string &path, ParamTable &table) {
  namespace fs = std::filesystem;

  if (!fs::exists(path)) {
    throw std::runtime_error("Namelist file not found: " + path);
  }
  if (!fs::is_regular_file(path)) {
    throw std::runtime_error("Namelist path is not a regular file: " + path);
  }

  std::ifstream infile(path);
  if (!infile) {
    throw std::runtime_error("Unable to open namelist file at " + path);
  }

  std::size_t line_no = 0;
  std::string curr_namelist;
  std::string raw_line;
  std::string accumulated_line;

  auto flush_accumulated = [&]() {
    if (accumulated_line.empty())
      return;
    process_assignments(trim(accumulated_line), curr_namelist, line_no, path,
                        table);
    accumulated_line.clear();
  };

  while (std::getline(infile, raw_line)) {
    ++line_no;

    std::string_view sv = trim(strip_comments(raw_line));
    if (sv.empty())
      continue;

    // Check for namelist start: &namelist_name [rest...]
    if (sv.front() == '&') {
      if (!curr_namelist.empty()) {
        throw std::runtime_error(
            path + ":" + std::to_string(line_no) +
            ": nested namelists not allowed (missing '/' for namelist '" +
            curr_namelist + "')");
      }

      auto after_amp = sv.substr(1);
      auto name_end = after_amp.find_first_of(" \t");
      std::string_view nml_name;
      std::string_view rest;
      if (name_end == std::string_view::npos) {
        nml_name = after_amp;
      } else {
        nml_name = after_amp.substr(0, name_end);
        rest = trim(after_amp.substr(name_end));
      }

      nml_name = trim(nml_name);
      if (nml_name.empty()) {
        throw std::runtime_error(path + ":" + std::to_string(line_no) +
                                 ": empty namelist name");
      }

      curr_namelist = lowercase(nml_name);
      accumulated_line.clear();

      if (rest.empty())
        continue;
      sv = rest;
    }

    // Check for unquoted '/' anywhere in the line (Fortran allows the closing
    // delimiter on the same line as the last value, e.g.  value = 'x' / ).
    auto slash_pos = find_unquoted(sv, '/');
    if (slash_pos != std::string_view::npos) {
      if (curr_namelist.empty()) {
        throw std::runtime_error(path + ":" + std::to_string(line_no) +
                                 ": unexpected '/' outside of a namelist");
      }
      std::string_view before = trim(sv.substr(0, slash_pos));
      if (!before.empty())
        accumulated_line += " " + std::string(before);
      flush_accumulated();
      curr_namelist.clear();
      continue;
    }

    if (curr_namelist.empty()) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) +
                               ": content outside of namelist block");
    }

    // If this line starts a new assignment, flush any previously deferred
    // assignment (one that ended with a trailing comma).
    if (find_unquoted(sv, '=') != std::string_view::npos)
      flush_accumulated();

    accumulated_line += " " + std::string(sv);

    // Try to process immediately when the assignment looks complete.
    std::string_view acc_sv = trim(accumulated_line);
    auto eq = find_unquoted(acc_sv, '=');

    if (eq != std::string_view::npos) {
      auto rhs = trim(acc_sv.substr(eq + 1));

      bool looks_complete = (rhs.empty() || rhs.back() != ',');

      if (looks_complete)
        looks_complete = quotes_balanced(rhs);

      if (looks_complete) {
        process_assignments(acc_sv, curr_namelist, line_no, path, table);
        accumulated_line.clear();
      }
    }
  }

  if (!curr_namelist.empty()) {
    throw std::runtime_error(path + ":EOF: unterminated namelist '" +
                             curr_namelist + "' (missing '/')");
  }
}

} // anonymous namespace

NamelistParams::NamelistParams(const std::string &path) : path_(path) {
  add_data_from_file(path_, table_);
}

template <typename T>
void NamelistParams::get(const std::string &key, T &value,
                         const std::string &namelist) const {
  const auto &val = get_variant(key, namelist);
  if (std::holds_alternative<T>(val)) {
    value = std::get<T>(val);
    return;
  }
  throw std::runtime_error("Parameter " +
                           (namelist.empty() ? key : namelist + ":" + key) +
                           " is not of the requested type");
}

const ParamValue &
NamelistParams::get_variant(const std::string &key,
                            const std::string &namelist) const {
  return table_.get_variant(key, namelist);
}

bool NamelistParams::has_param(const std::string &key,
                               const std::string &namelist) const {
  return table_.has_param(key, namelist);
}

std::vector<std::string> NamelistParams::get_namelists() const {
  return table_.get_groups();
}

size_t NamelistParams::get_num_parameters() const {
  return table_.get_num_parameters();
}

// Explicit template instantiations
template void NamelistParams::get<bool>(const std::string &, bool &,
                                        const std::string &) const;
template void NamelistParams::get<int>(const std::string &, int &,
                                       const std::string &) const;
template void NamelistParams::get<double>(const std::string &, double &,
                                          const std::string &) const;
template void NamelistParams::get<std::string>(const std::string &,
                                               std::string &,
                                               const std::string &) const;
template void NamelistParams::get<std::vector<bool>>(const std::string &,
                                                     std::vector<bool> &,
                                                     const std::string &) const;
template void NamelistParams::get<std::vector<int>>(const std::string &,
                                                    std::vector<int> &,
                                                    const std::string &) const;
template void NamelistParams::get<std::vector<double>>(
    const std::string &, std::vector<double> &, const std::string &) const;
template void NamelistParams::get<std::vector<std::string>>(
    const std::string &, std::vector<std::string> &, const std::string &) const;

} // namespace MOM
