#include "MOM_nml_parser.h"
#include "MOM_parser_utils.h"
#include "MOM_string_utils.h"

namespace MOM {

using string_utils::find_unquoted;
using string_utils::lowercase;
using string_utils::trim;
using string_utils::is_valid_identifier;
using parser_utils::get_value;
using parser_utils::strip_comments;

NamelistParams::NamelistParams(const std::string &path) : path_(path) {
  namespace fs = std::filesystem;

  if (!fs::exists(path_)) {
    throw std::runtime_error("Namelist file not found: " + path_);
  }
  if (!fs::is_regular_file(path_)) {
    throw std::runtime_error("Namelist path is not a regular file: " + path_);
  }

  std::ifstream infile(path_);
  if (!infile) {
    throw std::runtime_error("Unable to open namelist file at " + path_);
  }

  std::size_t line_no = 0;

  auto assign_param = [&](const std::string &namelist, const std::string &key, ParamValue value) {
    auto &nml_table = table_[namelist];
    auto it = nml_table.find(key);

    if (it == nml_table.end()) {
      nml_table.emplace(key, std::move(value));
      return;
    }

    if (it->second != value) {
      throw std::runtime_error(
        path_ + ":" + std::to_string(line_no) +
        ": duplicate assignment for '" + (namelist.empty() ? key : namelist + "%" + key) +
        "' with a different value"
      );
    }
  };

  std::string curr_namelist;
  std::string raw_line;
  std::string accumulated_line;

// Helper: find the end of the current assignment in a line, which may contain multiple comma-separated assignments.
auto find_rhs_end = [&](std::string_view rhs_full) {
  char in_quote = 0;

  for (size_t pos = 0; pos < rhs_full.size(); ++pos) {
    char c = rhs_full[pos];

    if (in_quote) {
      if (c == in_quote) in_quote = 0;
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
};

// Helper: process one or more assignments in a line (comma-separated), and add them to the table.
auto process_assignments = [&](std::string_view sv) {
  while (!sv.empty()) {
    auto eq = find_unquoted(sv, '=');
    if (eq == std::string_view::npos) return;

    auto lhs = trim(sv.substr(0, eq));
    if (lhs.empty()) {
      throw std::runtime_error(
        path_ + ":" + std::to_string(line_no) + ": empty variable name"
      );
    }

    auto rhs_full = sv.substr(eq + 1);
    auto rhs_end  = find_rhs_end(rhs_full);

    auto rhs = trim(rhs_full.substr(0, rhs_end));

    assign_param(
      curr_namelist,
      lowercase(std::string(lhs)),
      get_value(rhs, line_no, path_)
    );

    if (rhs_end < rhs_full.size()) {
      sv = trim(rhs_full.substr(rhs_end + 1));
    } else {
      break;
    }
  }
};

  // Helper: flush the current accumulated assignment(s) into the table.
  // Called before closing '/' and before starting a new 'key = ...' line.
  auto flush_accumulated = [&]() {
    if (accumulated_line.empty()) return;
    process_assignments(trim(accumulated_line));
    accumulated_line.clear();
  };

  while (std::getline(infile, raw_line)) {
    ++line_no;

    std::string_view sv = trim(strip_comments(raw_line));
    if (sv.empty()) continue;

    // Check for namelist start: &namelist_name [rest...]
    if (sv.front() == '&') {
      if (!curr_namelist.empty()) {
        throw std::runtime_error(
          path_ + ":" + std::to_string(line_no) +
          ": nested namelists not allowed (missing '/' for namelist '" + curr_namelist + "')"
        );
      }

      auto after_amp = sv.substr(1);
      // The namelist name is the first whitespace-delimited token.
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
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": empty namelist name");
      }

      curr_namelist = lowercase(nml_name);
      accumulated_line.clear();

      // If there is content after the namelist name on the same line, continue processing it.
      if (rest.empty()) continue;
      sv = rest;
    }


    // Check for unquoted '/' anywhere in the line (Fortran allows the closing
    // delimiter on the same line as the last value, e.g.  value = 'x' / ).
    auto slash_pos = find_unquoted(sv, '/');
    if (slash_pos != std::string_view::npos) {
      if (curr_namelist.empty()) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": unexpected '/' outside of a namelist");
      }
      // Any content before the slash is a continuation of the last assignment.
      std::string_view before = trim(sv.substr(0, slash_pos));
      if (!before.empty()) accumulated_line += " " + std::string(before);
      flush_accumulated();
      curr_namelist.clear();
      continue;
    }

    if (curr_namelist.empty()) {
      throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": content outside of namelist block");
    }

    // If this line starts a new assignment, flush any previously deferred
    // assignment (one that ended with a trailing comma). This makes a bare
    // newline a valid parameter separator, matching Fortran namelist rules.
    if (find_unquoted(sv, '=') != std::string_view::npos) flush_accumulated();

    // Accumulate line content (handles multi-line continuation).
    accumulated_line += " " + std::string(sv);

    // Try to process immediately when the assignment looks complete.
    // A trailing comma means more values may follow on the next line, so defer.
    std::string_view acc_sv = trim(accumulated_line);
    auto eq = find_unquoted(acc_sv, '=');

    if (eq != std::string_view::npos) {
      auto rhs = trim(acc_sv.substr(eq + 1));

      // Trailing comma: defer; let the next line decide.
      bool looks_complete = (rhs.empty() || rhs.back() != ',');

      // Unbalanced quotes: also incomplete.
      if (looks_complete) {
        char in_quote = 0;
        for (char c : rhs) {
          if (in_quote) {
            if (c == in_quote)
              in_quote = 0;
          } else if (c == '"' || c == '\'') {
            in_quote = c;
          }
        }
        looks_complete = (in_quote == 0);
      }

      if (looks_complete) {
        process_assignments(acc_sv);
        accumulated_line.clear();
      }
    }
  }

  if (!curr_namelist.empty()) {
    throw std::runtime_error(path_ + ":EOF: unterminated namelist '" + curr_namelist + "' (missing '/')");
  }
}

template <typename T>
void NamelistParams::get(const std::string &key, T &value, const std::string &namelist) const {
  const auto &val = get_variant(key, namelist);
  if (std::holds_alternative<T>(val)) {
    value = std::get<T>(val);
    return;
  }
  throw std::runtime_error("Parameter " + namelist + ":" + key + " is not of the requested type");
}

const ParamValue &NamelistParams::get_variant(const std::string &key, const std::string &namelist) const {
  const std::string lower_nml = lowercase(namelist);
  const std::string lower_key = lowercase(key);

  auto nml_it = table_.find(lower_nml);
  if (nml_it == table_.end()) throw std::out_of_range("Namelist not found: " + namelist);

  auto key_it = nml_it->second.find(lower_key);
  if (key_it == nml_it->second.end()) throw std::out_of_range("Key not found in namelist " + namelist + ": " + key);

  return key_it->second;
}

bool NamelistParams::has_param(const std::string &key, const std::string &namelist) const {
  auto nml_it = table_.find(lowercase(namelist));
  return nml_it != table_.end() && nml_it->second.find(lowercase(key)) != nml_it->second.end();
}

std::vector<std::string> NamelistParams::get_namelists() const {
  std::vector<std::string> namelists;
  namelists.reserve(table_.size());
  for (const auto& [name, _] : table_) namelists.push_back(name);
  return namelists;
}

size_t NamelistParams::get_num_parameters() const {
  size_t count = 0;
  for (const auto& [_, params] : table_) count += params.size();
  return count;
}

// Explicit template instantiations
template void NamelistParams::get<bool>(const std::string &, bool &, const std::string &) const;
template void NamelistParams::get<int>(const std::string &, int &, const std::string &) const;
template void NamelistParams::get<double>(const std::string &, double &, const std::string &) const;
template void NamelistParams::get<std::string>(const std::string &, std::string &, const std::string &) const;
template void NamelistParams::get<std::vector<bool>>(const std::string &, std::vector<bool> &, const std::string &) const;
template void NamelistParams::get<std::vector<int>>(const std::string &, std::vector<int> &, const std::string &) const;
template void NamelistParams::get<std::vector<double>>(const std::string &, std::vector<double> &, const std::string &) const;
template void NamelistParams::get<std::vector<std::string>>(const std::string &, std::vector<std::string> &, const std::string &) const;

} // namespace MOM
