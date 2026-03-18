#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace MOM{

namespace string_utils {

/// @brief Trim leading and trailing whitespace from a string_view.
/// @param s The input string_view to trim.
/// @return A string_view with leading and trailing whitespace removed.
inline std::string_view trim(std::string_view s) {
  const auto first = s.find_first_not_of(" \t\n\r");
  if (first == std::string_view::npos)
    return {};

  const auto last = s.find_last_not_of(" \t\n\r");
  return s.substr(first, last - first + 1);
}

/// @brief Convert a string_view to lowercase.
/// @param sv The input string_view to convert.
/// @return A new string with all characters converted to lowercase.
inline std::string lowercase(std::string_view sv) {
  std::string out(sv);
  for (char &c : out)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

/// @brief Check if a string_view is a valid identifier
///
/// A valid identifier starts with a letter or underscore, followed by letters, digits, or underscores.
/// @param s The string_view to check.
/// @return true if the string_view is a valid identifier, false otherwise.
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

/// @brief Find the first occurrence of a character in a string_view, ignoring characters inside quotes.
/// @param s The string_view to search.
/// @param ch The character to find.
/// @return The index of the first occurrence of the character, or std::string_view::npos if not found.
inline std::size_t find_unquoted(std::string_view s, char ch) {
  char in_quote = 0; // 0, '\'' or '"'
  for (std::size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (in_quote) {
      if (c == in_quote)
        in_quote = 0;
    } else {
      if (c == '"' || c == '\'')
        in_quote = c;
      else if (c == ch)
        return i;
    }
  }
  return std::string_view::npos;
}


/// @brief Split a string by a delimiter.
/// @param s The string to split.
/// @param delim The delimiter to split by.
/// @return A vector of strings.
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

/// @brief Split a string by a delimiter, but only at the top level (i.e. ignoring delimiters inside quotes).
/// @param s The string to split.
/// @param delimiter The delimiter to split by. Default is comma.
/// @return A vector of string_view parts.
inline std::vector<std::string_view> split_unquoted(std::string_view s, const char delimiter = ',') {
  std::vector<std::string_view> parts;
  std::size_t start = 0;

  while (start < s.size()) {
    std::string_view tail = s.substr(start);
    std::size_t delimiter_rel = find_unquoted(tail, delimiter);
    if (delimiter_rel == std::string_view::npos)
      break;

    parts.push_back(tail.substr(0, delimiter_rel));
    start += delimiter_rel + 1;
  }

  parts.push_back(s.substr(start));
  return parts;
}


} // namespace string_utils
 
} // namespace MOM
