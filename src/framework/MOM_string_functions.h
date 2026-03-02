#pragma once

#include <algorithm>
#include <string>
#include <vector>

// trim whitespace from both ends of a string_view
inline std::string_view trim(std::string_view s) {
  const auto first = s.find_first_not_of(" \t\n\r");
  if (first == std::string_view::npos)
    return {};

  const auto last = s.find_last_not_of(" \t\n\r");
  return s.substr(first, last - first + 1);
}

inline std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> result;
  std::size_t start = 0;
  std::size_t pos = 0;

  while ((pos = s.find(delim, start)) != std::string::npos) {
    result.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }

  result.push_back(s.substr(start));
  return result;
}

inline std::string lowercase(std::string_view sv) {
  std::string out(sv);
  for (char &c : out)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

// Check if a string is a valid identifier (starts with letter or _, followed by letters, digits, or _).
inline bool is_valid_identifier(std::string_view s) {
  s = trim(s);
  if (s.empty())
    return false;

  auto it = s.begin();
  unsigned char first = static_cast<unsigned char>(*it);
  if (!(std::isalpha(first) || first == '_'))
    return false;

  for (++it; it != s.end(); ++it) {
    unsigned char c = static_cast<unsigned char>(*it);
    if (!(std::isalnum(c) || c == '_'))
      return false;
  }

  return true;
}