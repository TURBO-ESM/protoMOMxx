/**
 * @file MOM_nml_parser.h
 * @brief Lightweight parser for Fortran namelist files.
 *
 * @details
 * Provides parsing of Fortran namelist files with support for comments and
 * detailed error reporting. Supported value types:
 * - Scalars: bool, int, double, string
 * - Homogeneous comma-separated lists of the above types
 *
 * Fortran Namelist syntax:
 * @code
 *   &NAMELIST_NAME
 *     VARIABLE = value
 *     LOGICAL_VAR = .true.
 *     LOGICAL_VAR2 = .false.
 *     INTEGER_VAR = 42
 *     REAL_VAR = 3.14
 *     STRING_VAR = 'Hello World'
 *     ARRAY_VAR = 1, 2, 3, 4
 *     STRING_ARRAY = 'abc', 'def', 'ghi'
 *   /
 *
 *   &ANOTHER_NAMELIST
 *     VAR1 = 100
 *   /
 * @endcode
 *
 * Comments in namelist files:
 * - Line comments start with '!'
 *
 * Example usage:
 * @code
 *   NamelistParams params("path/to/namelist.nml");
 *   bool value;
 *   params.get("LOGICAL_VAR", value, "NAMELIST_NAME");
 * @endcode
 */

#pragma once

#include "MOM_param_table.h"
#include "MOM_parser_utils.h"
#include <string>
#include <vector>

namespace MOM {

using parser_utils::ParamValue;

/// @brief The NamelistParams class provides an interface for parsing Fortran
/// namelist files and retrieving values.
class NamelistParams {
public:
  /// @brief Construct a NamelistParams object by parsing the specified namelist
  /// file.
  /// @param path The path to the namelist file to parse.
  explicit NamelistParams(const std::string &path);

  /// @brief Get a parameter value from a namelist
  /// @param key The parameter key
  /// @param namelist The namelist name
  /// @return The parameter value as a ParamValue
  /// @throws std::out_of_range if the namelist or key does not exist
  const ParamValue &get_variant(const std::string &key,
                                const std::string &namelist = "") const;

  /// @brief Get a parameter value as a specific type.
  /// @tparam T The expected type of the parameter value.
  /// @param key The parameter key
  /// @param value An output reference to store the retrieved parameter value.
  /// @param namelist The namelist name (empty string for global scope)
  /// @throws std::runtime_error if the parameter is not of type T
  template <typename T>
  void get(const std::string &key, T &value,
           const std::string &namelist = "") const;

  /// @brief Check if a parameter exists
  /// @param key The parameter key
  /// @param namelist The namelist name
  /// @return true if the parameter exists, false otherwise
  bool has_param(const std::string &key,
                 const std::string &namelist = "") const;

  /// @brief Get all namelists
  /// @return A vector of namelist names
  std::vector<std::string> get_namelists() const;

  /// @brief Get the total number of parameters across all namelists
  /// @return The total number of parameters across all namelists
  size_t get_num_parameters() const;

private:
  std::string path_;
  ParamTable table_{true}; // case-insensitive
};

} // namespace MOM