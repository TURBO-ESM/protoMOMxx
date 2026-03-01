#include "MOM_nml_parser.h"
#include "MOM_string_functions.h"
#include "MOM_parser_utilities.h"

using mom_parser_utilities::find_unquoted;
using mom_parser_utilities::get_value;

namespace {

/// @brief Strip comments from a line (Fortran-style ! only).
static std::string strip_comments(std::string_view line) {
  std::string out;
  out.reserve(line.size());

  bool in_sq = false;
  bool in_dq = false;

  for (std::size_t i = 0; i < line.size(); ++i) {
    char c = line[i];

    // Update quote state
    if (!in_dq && c == '\'') {
      in_sq = !in_sq;
      out.push_back(c);
      continue;
    }

    if (!in_sq && c == '"') {
      in_dq = !in_dq;
      out.push_back(c);
      continue;
    }

    // Inline '!' comment? (only if not in quotes)
    if (!in_sq && !in_dq && c == '!') {
      break; // ignore rest of line
    }

    out.push_back(c);
  }

  return out;
}

} // namespace

NamelistParams::NamelistParams(const std::string& path) : path_(path) {
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

  auto assign_param = [&](const std::string& namelist,
                          const std::string& key,
                          ParamValue value) {
    auto& nml_table = table_[namelist];
    auto it = nml_table.find(key);

    if (it == nml_table.end()) {
      nml_table.emplace(key, std::move(value));
      return;
    }

    // Keep repeated assignments only when they agree exactly.
    if (it->second != value) {
      throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": duplicate assignment for '" +
                               (namelist.empty() ? key : namelist + "%" + key) +
                               "' with a different value");
    }
  };

  while (std::getline(infile, raw_line)) {
    ++line_no;

    // Strip comments
    std::string line = strip_comments(raw_line);
    std::string_view sv = trim(line);

    if (sv.empty()) continue;

    // Check for namelist start: &namelist_name
    if (sv.front() == '&') {
      if (in_namelist) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + 
                                 ": nested namelists not allowed (missing '/' for namelist '" + 
                                 curr_namelist + "')");
      }
      
      auto name = trim(sv.substr(1));
      if (name.empty()) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + ": empty namelist name");
      }
      
      // Convert to uppercase (Fortran convention)
      std::string namelist_name(name);
      for (char& c : namelist_name) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      }
      
      curr_namelist = namelist_name;
      in_namelist = true;
      accumulated_line.clear();
      continue;
    }

    // Check for namelist end: /
    if (sv.front() == '/' && sv.size() == 1) {
      if (!in_namelist) {
        throw std::runtime_error(path_ + ":" + std::to_string(line_no) + 
                                 ": unexpected '/' outside of a namelist");
      }
      
      // Process any accumulated line before closing
      if (!accumulated_line.empty()) {
        std::string_view acc_sv = trim(accumulated_line);
        auto eq = find_unquoted(acc_sv, '=');
        if (eq != std::string_view::npos) {
          auto lhs = trim(acc_sv.substr(0, eq));
          auto rhs = trim(acc_sv.substr(eq + 1));
          
          if (lhs.empty()) {
            throw std::runtime_error(path_ + ":" + std::to_string(line_no) + 
                                     ": empty variable name");
          }
          
          // Convert key to uppercase
          std::string key(lhs);
          for (char& c : key) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
          }
          
          assign_param(curr_namelist, key, get_value(rhs, line_no, path_, true));
        }
        accumulated_line.clear();
      }
      
      in_namelist = false;
      curr_namelist.clear();
      continue;
    }

    if (!in_namelist) {
      throw std::runtime_error(path_ + ":" + std::to_string(line_no) + 
                               ": content outside of namelist block");
    }

    // Accumulate line content (handles continuation)
    accumulated_line += " " + std::string(sv);
    
    // Check if this line contains a complete assignment
    std::string_view acc_sv = trim(accumulated_line);
    auto eq = find_unquoted(acc_sv, '=');
    
    if (eq != std::string_view::npos) {
      // Check if the value part ends (no unclosed quotes and no trailing comma suggesting continuation)
      auto rhs = trim(acc_sv.substr(eq + 1));
      
      // Simple heuristic: if line ends with comma, it might continue
      // But for now, process each assignment when we find '='
      bool looks_complete = true;
      if (!rhs.empty() && rhs.back() == ',') {
        // Could be continuation, but also could be end of array with trailing comma
        // For simplicity, treat as complete if it has balanced quotes
        char in_quote = 0;
        for (char c : rhs) {
          if (in_quote) {
            if (c == in_quote) in_quote = 0;
          } else if (c == '"' || c == '\'') {
            in_quote = c;
          }
        }
        looks_complete = (in_quote == 0);
      }
      
      if (looks_complete) {
        auto lhs = trim(acc_sv.substr(0, eq));
        
        if (lhs.empty()) {
          throw std::runtime_error(path_ + ":" + std::to_string(line_no) + 
                                   ": empty variable name");
        }
        
        // Convert key to uppercase
        std::string key(lhs);
        for (char& c : key) {
          c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        
        assign_param(curr_namelist, key, get_value(rhs, line_no, path_, true));
        accumulated_line.clear();
      }
    }
  }

  if (in_namelist) {
    throw std::runtime_error(path_ + ":EOF: unterminated namelist '" + curr_namelist + "' (missing '/')");
  }
}

template<typename T>
T NamelistParams::get_as(const std::string& key, const std::string& namelist) const {
  const auto& val = get(key, namelist);
  if (std::holds_alternative<T>(val)) {
    return std::get<T>(val);
  }
  throw std::runtime_error("Parameter " + namelist + ":" + key + " is not of the requested type");
}

const ParamValue& NamelistParams::get(const std::string& key, const std::string& namelist) const {
  auto nml_it = table_.find(namelist);
  if (nml_it == table_.end()) {
    throw std::out_of_range("Namelist not found: " + namelist);
  }
  
  auto key_it = nml_it->second.find(key);
  if (key_it == nml_it->second.end()) {
    throw std::out_of_range("Key not found in namelist " + namelist + ": " + key);
  }
  
  return key_it->second;
}

bool NamelistParams::has_param(const std::string& key, 
                               const std::string& namelist) const {
  auto nml_it = table_.find(namelist);
  if (nml_it == table_.end()) {
    return false;
  }
  return nml_it->second.find(key) != nml_it->second.end();
}

std::vector<std::string> NamelistParams::get_namelists() const {
  std::vector<std::string> namelists;
  namelists.reserve(table_.size());
  for (const auto& pair : table_) {
    namelists.push_back(pair.first);
  }
  return namelists;
}

size_t NamelistParams::get_num_parameters() const {
  size_t count = 0;
  for (const auto& nml_pair : table_) {
    count += nml_pair.second.size();
  }
  return count;
}

// Explicit template instantiations for get_as
template bool NamelistParams::get_as<bool>(const std::string&, const std::string&) const;
template std::int64_t NamelistParams::get_as<std::int64_t>(const std::string&, const std::string&) const;
template double NamelistParams::get_as<double>(const std::string&, const std::string&) const;
template std::string NamelistParams::get_as<std::string>(const std::string&, const std::string&) const;
template std::vector<bool> NamelistParams::get_as<std::vector<bool>>(const std::string&, const std::string&) const;
template std::vector<std::int64_t> NamelistParams::get_as<std::vector<std::int64_t>>(const std::string&, const std::string&) const;
template std::vector<double> NamelistParams::get_as<std::vector<double>>(const std::string&, const std::string&) const;
template std::vector<std::string> NamelistParams::get_as<std::vector<std::string>>(const std::string&, const std::string&) const;
