#include "MOM_parser_utilities.h"
#include "MOM_string_functions.h"
#include <charconv>
#include <stdexcept>

// -----------------------------------------------------------------------------
// Internal helper functions for parsing. (Not part of the public API.)
// -----------------------------------------------------------------------------

using MOM::string_utils::lowercase;
using MOM::string_utils::trim;
using MOM::string_utils::split_unquoted;

/// @brief Parse a quoted string, ensuring it is well-formed and stripping the quotes.
/// @param s The input string view containing the quoted string.
/// @param line_no The line number for error reporting.
/// @param path The file path for error reporting.
/// @return The parsed string with quotes removed.
static std::string parse_quoted_string(std::string_view s, std::size_t line_no, const std::string &path) {
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
/// @param s The input string view to parse.
/// @param out Reference to a boolean variable where the parsed value will be stored if parsing is successful.
/// @return true if parsing was successful and out was set, false otherwise.
static bool try_parse_bool(std::string_view s, bool &out) {
  auto sl = lowercase(trim(s));

  if (sl == ".true." || sl == "true" || sl == "t") {
    out = true;
    return true;
  }
  if (sl == ".false." || sl == "false" || sl == "f") {
    out = false;
    return true;
  }
  return false;
}

/// @brief Try to parse a string as a 64-bit integer using std::from_chars.
/// @param s The input string view to parse.
/// @param out Reference to a int variable where the parsed value will be stored if parsing is successful.
/// @return true if parsing was successful and out was set, false otherwise.
static bool try_parse_int(std::string_view s, int &out) {
  s = trim(s);
  if (s.empty())
    return false;
  // from_chars doesn't accept leading '+' reliably across old libs; handle explicitly:
  if (s.front() == '+')
    s.remove_prefix(1);
  int v{};
  auto *b = s.data();
  auto *e = s.data() + s.size();
  auto res = std::from_chars(b, e, v, 10);
  if (res.ec != std::errc{} || res.ptr != e)
    return false;
  out = v;
  return true;
}

/// @brief Try to parse a string as a double using std::stod, ensuring the entire string is consumed.
/// @param s The input string view to parse.
/// @param out Reference to a double variable where the parsed value will be stored if parsing is successful.
/// @return true if parsing was successful and out was set, false otherwise.
static bool try_parse_double(std::string_view s, double &out) {
  std::string tmp(trim(s));
  if (tmp.empty())
    return false;
  try {
    std::size_t pos = 0;
    double v = std::stod(tmp, &pos);
    if (pos != tmp.size())
      return false;
    out = v;
    return true;
  } catch (const std::invalid_argument &) {
    return false;
  } catch (const std::out_of_range &) {
    return false;
  }
}

/// @brief Parse a scalar value from a string, trying bool, int, double, and quoted string (in that order).
/// @param s The input string view to parse.
/// @param line_no The line number for error reporting.
/// @param path The file path for error reporting.
/// @return The parsed value as a ParamValue variant.
static mom_parser_utilities::ParamValue parse_scalar(std::string_view s, std::size_t line_no, const std::string &path) {
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
  if (try_parse_bool(s, b))
    return b;

  // Int
  int i{};
  if (try_parse_int(s, i))
    return i;

  // Double
  double d{};
  if (try_parse_double(s, d))
    return d;

  // Bare string: reject spaces
  if (s.find_first_of(" \t\r\n") != std::string_view::npos) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": unquoted string contains whitespace: '" +
                             std::string(s) + "'");
  }
  return std::string(s);
}

// -----------------------------------------------------------------------------
// Public mom_parser_utilities API definitions
// -----------------------------------------------------------------------------

namespace mom_parser_utilities {

ParamValue get_value(std::string_view raw, std::size_t line_no, const std::string &path) {
  raw = trim(raw);
  if (raw.empty()) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": missing value");
  }

  // List?
  auto parts = split_unquoted(raw);
  if (parts.size() == 1) {
    return parse_scalar(parts[0], line_no, path);
  }

  // Parse each as scalar, then enforce homogeneous type and return vector<T>.
  std::vector<ParamValue> scalars;
  scalars.reserve(parts.size());
  for (auto p : parts) {
    auto trimmed = trim(p);
    if (!trimmed.empty()) { // Skip empty entries from trailing commas
      scalars.push_back(parse_scalar(trimmed, line_no, path));
    }
  }

  if (scalars.empty()) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": empty list");
  }

  if (scalars.size() == 1) {
    return scalars[0];
  }

  const std::size_t idx = scalars.front().index();
  for (const auto &v : scalars) {
    if (v.index() != idx) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) +
                               ": mixed-type list is not allowed (e.g. int + string).");
    }
  }

  switch (idx) {
  case 1: { // bool
    std::vector<bool> out;
    out.reserve(scalars.size());
    for (auto &v : scalars)
      out.push_back(std::get<bool>(v));
    return out;
  }
  case 2: { // int
    std::vector<int> out;
    out.reserve(scalars.size());
    for (auto &v : scalars)
      out.push_back(std::get<int>(v));
    return out;
  }
  case 3: { // double
    std::vector<double> out;
    out.reserve(scalars.size());
    for (auto &v : scalars)
      out.push_back(std::get<double>(v));
    return out;
  }
  case 4: { // string
    std::vector<std::string> out;
    out.reserve(scalars.size());
    for (auto &v : scalars)
      out.push_back(std::get<std::string>(v));
    return out;
  }
  default:
    // Should never happen because parse_scalar only returns scalar types.
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": internal error parsing list");
  }
}

} // namespace mom_parser_utilities