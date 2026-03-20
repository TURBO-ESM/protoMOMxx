/**
 * @file MOM_parser_utils.h
 * @brief Common parsing utilities for MOM runtime parameter and namelist parsers.
 *
 * This header contains shared parsing functions used by both MOM_file_parser and MOM_nml_parser
 * to avoid code duplication.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace MOM::parser_utils {

/// @brief A type alias for the supported parameter value types.
/// std::monostate is the first alternative, used to represent "not found" / unset values.
using ParamValue = std::variant<std::monostate, bool, int, double, std::string, std::vector<bool>,
                                std::vector<int>, std::vector<double>, std::vector<std::string>>;

inline const ParamValue NotFound{}; /// A sentinel value representing a missing parameter

/// @brief Parse a value which can be either a scalar or a comma-separated list of scalars.
/// @param raw The input string view containing the raw value to parse.
/// @param line_no The line number for error reporting.
/// @param path The file path for error reporting.
/// @return The parsed value as a ParamValue variant, which may be a scalar or a vector of scalars.
ParamValue get_value(std::string_view raw, std::size_t line_no, const std::string &path);

/// @brief Strip comments from a line (Fortran-style ! only).
/// @param line The input string view containing the line to strip comments from.
/// @return A string_view with comments removed.
std::string_view strip_comments(std::string_view line);

} // namespace MOM::parser_utils
