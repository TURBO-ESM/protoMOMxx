/**
 * @file MOM_parser_utilities.h
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

namespace mom_parser_utilities {

/// @brief A type alias for the supported parameter value types.
using ParamValue = std::variant<bool, int, double, std::string, std::vector<bool>, std::vector<int>,
                                std::vector<double>, std::vector<std::string>>;
static const ParamValue NotFound{};

/// @brief Find the first occurrence of a character that is not inside quotes. Handles both single and double quotes.
/// @param s The string to search through.
/// @param ch The character to find.
/// @return The index of the first unquoted occurrence of ch, or std::string_view::npos if not found.
std::size_t find_unquoted(std::string_view s, char ch);

/// @brief Parse a value which can be either a scalar or a comma-separated list of scalars.
/// @param raw The input string view containing the raw value to parse.
/// @param line_no The line number for error reporting.
/// @param path The file path for error reporting.
/// @param fortran_style If true, uses Fortran-style boolean parsing (.true./.false.).
/// @return The parsed value as a ParamValue variant, which may be a scalar or a vector of scalars.
ParamValue get_value(std::string_view raw, std::size_t line_no, const std::string &path, bool fortran_style = false);

} // namespace mom_parser_utilities
