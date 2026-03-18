#include "MOM_nml_parser.h"
#include "MOM_parser_utilities.h"
#include "MOM_string_functions.h"

using MOM::string_utils::find_unquoted;
using MOM::string_utils::lowercase;
using MOM::string_utils::trim;
using mom_parser_utilities::get_value;
using mom_parser_utilities::strip_comments;

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

  std::string curr_namelist;
  std::string raw_line;
  std::size_t line_no = 0;
  std::string accumulated_line;
  bool in_namelist = false;

  auto assign_param = [&](const std::string &namelist, const std::string &key, ParamValue value) {
    auto &nml_table = table_[namelist];
    auto it = nml_table.find(key);

    if (it == nml_table.end()) {
      nml_table.emplace(key, std::move(value));
      return;
    }

    // Keep repeated assignments only when they agree exactly.
    if (it->second != value) {
      throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": duplicate assignment for '" +
                               (namelist.empty() ? key : namelist + "%" + key) + "' with a different value");
    }
  };

  while (std::getline(infile, raw_line)) {
    ++line_no;

    // Strip comments
    std::string_view sv = trim(strip_comments(raw_line));

    if (sv.empty())
      continue;

    // Check for namelist start: &namelist_name
    if (sv.front() == '&') {
      if (in_namelist) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) +
                                 ": nested namelists not allowed (missing '/' for namelist '" + curr_namelist + "')");
      }

      auto name = trim(sv.substr(1));
      if (name.empty()) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": empty namelist name");
      }

      std::string namelist_name(name);
      for (char &c : namelist_name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }

      curr_namelist = namelist_name;
      in_namelist = true;
      accumulated_line.clear();
      continue;
    }

    // Helper: flush the current accumulated assignment into the table.
    // Called before closing '/' and before starting a new 'key = ...' line.
    auto flush_accumulated = [&]() {
      if (accumulated_line.empty())
        return;
      std::string_view acc_sv = trim(accumulated_line);
      auto eq = find_unquoted(acc_sv, '=');
      if (eq != std::string_view::npos) {
        auto lhs = trim(acc_sv.substr(0, eq));
        auto rhs = trim(acc_sv.substr(eq + 1));
        if (lhs.empty()) {
          throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": empty variable name");
        }
        std::string key(lowercase(lhs));
        assign_param(curr_namelist, key, get_value(rhs, line_no, path_));
      }
      accumulated_line.clear();
    };

    // Check for unquoted '/' anywhere in the line (Fortran allows the closing
    // delimiter on the same line as the last value, e.g.  value = 'x' / ).
    std::string_view::size_type slash_pos = find_unquoted(sv, '/');
    if (slash_pos != std::string_view::npos) {
      if (!in_namelist) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": unexpected '/' outside of a namelist");
      }
      // Any content before the slash is a continuation of the last assignment.
      std::string_view before = trim(sv.substr(0, slash_pos));
      if (!before.empty())
        accumulated_line += " " + std::string(before);
      flush_accumulated();
      in_namelist = false;
      curr_namelist.clear();
      continue;
    }

    if (!in_namelist) {
      throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": content outside of namelist block");
    }

    // If this line starts a new assignment, flush any previously deferred
    // assignment (one that ended with a trailing comma). This makes a bare
    // newline a valid parameter separator, matching Fortran namelist rules.
    if (find_unquoted(sv, '=') != std::string_view::npos)
      flush_accumulated();

    // Accumulate line content (handles multi-line continuation).
    accumulated_line += " " + std::string(sv);

    // Try to process immediately when the assignment looks complete.
    // A trailing comma means more values may follow on the next line, so defer.
    std::string_view acc_sv = trim(accumulated_line);
    auto eq = find_unquoted(acc_sv, '=');

    if (eq != std::string_view::npos) {
      auto rhs = trim(acc_sv.substr(eq + 1));

      // Trailing comma → defer; let the next line decide.
      bool looks_complete = (rhs.empty() || rhs.back() != ',');

      // Unbalanced quotes → also incomplete.
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
        auto lhs = trim(acc_sv.substr(0, eq));
        if (lhs.empty()) {
          throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": empty variable name");
        }
        std::string key(lowercase(lhs));
        assign_param(curr_namelist, key, get_value(rhs, line_no, path_));
        accumulated_line.clear();
      }
    }
  }

  if (in_namelist) {
    throw std::runtime_error(path_ + ":EOF: unterminated namelist '" + curr_namelist + "' (missing '/')");
  }
}

template <typename T> T NamelistParams::get(const std::string &key, const std::string &namelist) const {
  const auto &val = get_variant(key, namelist);
  if (std::holds_alternative<T>(val)) {
    return std::get<T>(val);
  }
  throw std::runtime_error("Parameter " + namelist + ":" + key + " is not of the requested type");
}

const ParamValue &NamelistParams::get_variant(const std::string &key, const std::string &namelist) const {
  // Keys and namelist names are stored lowercase; normalise the lookup too.
  const std::string lower_nml = lowercase(namelist);
  const std::string lower_key = lowercase(key);

  auto nml_it = table_.find(lower_nml);
  if (nml_it == table_.end()) {
    throw std::out_of_range("Namelist not found: " + namelist);
  }

  auto key_it = nml_it->second.find(lower_key);
  if (key_it == nml_it->second.end()) {
    throw std::out_of_range("Key not found in namelist " + namelist + ": " + key);
  }

  return key_it->second;
}

bool NamelistParams::has_param(const std::string &key, const std::string &namelist) const {
  auto nml_it = table_.find(lowercase(namelist));
  if (nml_it == table_.end()) {
    return false;
  }
  return nml_it->second.find(lowercase(key)) != nml_it->second.end();
}

std::vector<std::string> NamelistParams::get_namelists() const {
  std::vector<std::string> namelists;
  namelists.reserve(table_.size());
  for (const auto &pair : table_) {
    namelists.push_back(pair.first);
  }
  return namelists;
}

size_t NamelistParams::get_num_parameters() const {
  size_t count = 0;
  for (const auto &nml_pair : table_) {
    count += nml_pair.second.size();
  }
  return count;
}

// Explicit template instantiations for get
template bool NamelistParams::get<bool>(const std::string &, const std::string &) const;
template int NamelistParams::get<int>(const std::string &, const std::string &) const;
template double NamelistParams::get<double>(const std::string &, const std::string &) const;
template std::string NamelistParams::get<std::string>(const std::string &, const std::string &) const;
template std::vector<bool> NamelistParams::get<std::vector<bool>>(const std::string &, const std::string &) const;
template std::vector<int> NamelistParams::get<std::vector<int>>(const std::string &,
                                                                                     const std::string &) const;
template std::vector<double> NamelistParams::get<std::vector<double>>(const std::string &,
                                                                         const std::string &) const;
template std::vector<std::string> NamelistParams::get<std::vector<std::string>>(const std::string &,
                                                                                   const std::string &) const;
