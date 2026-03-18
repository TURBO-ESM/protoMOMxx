#include "MOM_parser_utilities.h"
#include "MOM_string_functions.h"
#include <charconv>
#include <stdexcept>
#include <optional>

// -----------------------------------------------------------------------------
// Internal helper functions for parsing. (Not part of the public API.)
// -----------------------------------------------------------------------------

using MOM::string_utils::lowercase;
using MOM::string_utils::trim;
using MOM::string_utils::split_unquoted;

namespace mom_parser_utilities {

namespace {

/// @brief Parse a quoted string, ensuring it is well-formed and stripping the quotes.
/// @param s The input string view containing the quoted string.
/// @param line_no The line number for error reporting.
/// @param path The file path for error reporting.
/// @return The parsed string with quotes removed.
std::string parse_quoted_string(std::string_view s, std::size_t line_no, const std::string &path) {
   s = trim(s);

  if (s.size() < 2) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": expected quoted string");
  }

  char q = s.front();
  if ((q != '"' && q != '\'') || s.back() != q) {
      throw std::runtime_error(path + ":" + std::to_string(line_no) + ": malformed quoted string");
  }

  // Extract the content between the quotes
  return std::string(s.substr(1, s.size() - 2));
}

/// @brief Try to parse a string as a boolean value.
/// @param s The input string view to parse.
/// @return The parsed (optional) boolean value if parsing was successful, std::nullopt otherwise.
std::optional<bool> try_parse_bool(std::string_view s) {
  const auto sl = lowercase(trim(s));

  if (sl == ".true." || sl == "true" || sl == "t") {
    return true;
  }
  if (sl == ".false." || sl == "false" || sl == "f") {
    return false;
  }
  return std::nullopt;
}

/// @brief Try to parse a string as an integer using std::from_chars.
/// @param s The input string view to parse.
/// @return The parsed (optional) integer value if parsing was successful, std::nullopt otherwise.
std::optional<int> try_parse_int(std::string_view s) {
    s = trim(s);
    if (s.empty())
        return std::nullopt;

    int value{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);

    if (ec != std::errc{} || ptr != s.data() + s.size())
        return std::nullopt;

    return value;
}

/// @brief Try to parse a string as a double using std::stod, ensuring the entire string is consumed.
/// @param s The input string view to parse.
/// @return The parsed double value if successful, std::nullopt otherwise.
std::optional<double> try_parse_double(std::string_view s) {
    s = trim(s);
    if (s.empty()) return std::nullopt;

    // Using std::stod for portability and proper handling of NaN/Inf, 
    // since std::from_chars for double doesn't seem to be fully supported in all compilers yet.
    try {
        std::size_t pos = 0;
        std::string tmp(s);  // std::stod requires std::string
        double value = std::stod(tmp, &pos);
        if (pos != tmp.size()) return std::nullopt;
        return value;
    } catch (const std::invalid_argument&) {
        return std::nullopt; // Not a number
    } catch (const std::out_of_range&) {
        return std::nullopt; // Too large/small
    }
}

/// @brief Parse a scalar value from a string, trying bool, int, double, and quoted string (in that order).
/// @param s The input string view to parse.
/// @param line_no The line number for error reporting.
/// @param path The file path for error reporting.
/// @return The parsed value as a ParamValue variant.
ParamValue parse_scalar(std::string_view s, std::size_t line_no, const std::string &path) {
  s = trim(s);
  if (s.empty()) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": empty value");
  }

  // Quoted string
  if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')) {
    return parse_quoted_string(s, line_no, path);
  }

  // Bool
  const auto b= try_parse_bool(s);
  if (b.has_value())
    return b.value();

  // Int
  const auto i = try_parse_int(s);
  if (i.has_value())
    return i.value();

  // Double
  const auto d = try_parse_double(s);
  if (d.has_value())
    return d.value();

  // Bare string: reject spaces
  if (s.find_first_of(" \t\r\n") != std::string_view::npos) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": unquoted string contains whitespace: '" +
                             std::string(s) + "'");
  }
  return std::string(s);
}

/// @brief Convert a vector of ParamValue scalars to a vector of a specific type T
/// @tparam T The target type to convert to (must be one of the scalar types supported by ParamValue).
/// @param scalars The input vector of ParamValue scalars to convert.
/// @return A vector of type T containing the converted values.
template <typename T>
std::vector<T> convert_vector(const std::vector<ParamValue>& scalars) {
    std::vector<T> out;
    out.reserve(scalars.size());
    for (const auto &v : scalars) out.push_back(std::get<T>(v));
    return out;
}


} // anonymous namespace

// -----------------------------------------------------------------------------
// Public mom_parser_utilities API definitions
// -----------------------------------------------------------------------------

ParamValue get_value(std::string_view raw, std::size_t line_no, const std::string &path) {
  raw = trim(raw);
  if (raw.empty()) {
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": missing value");
  }

  // List?
  const auto parts = split_unquoted(raw);
  if (parts.size() == 1) {
    return parse_scalar(parts[0], line_no, path);
  }

  // Parse each as scalar, then enforce homogeneous type and return vector<T>.
  std::vector<ParamValue> scalars;
  scalars.reserve(parts.size());
  for (const auto &p : parts) {
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
  case 1: return convert_vector<bool>(scalars);
  case 2: return convert_vector<int>(scalars);
  case 3: return convert_vector<double>(scalars);
  case 4: return convert_vector<std::string>(scalars);
  default:
    throw std::runtime_error(path + ":" + std::to_string(line_no) + ": internal error parsing list");
  }
}

} // namespace mom_parser_utilities