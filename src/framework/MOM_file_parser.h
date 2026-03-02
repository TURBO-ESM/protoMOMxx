/**
 * @file MOM_file_parser.h
 * @brief Lightweight parser for MOM runtime parameter files.
 *
 * @details
 * Provides robust parsing of MOM runtime parameter files with support for C-style block comments, 
 * Fortran-style line comments, quoted strings, and detailed error reporting. Supported value types:
 * - Scalars: bool, int64, double, string
 * - Homogeneous comma-separated lists of the above types
 *
 * MOM6 Runtime Parameter syntax:
 * @code
 *   #define VAR                  ! To set the logical VAR to true.
 *   VAR = True                   ! To set the logical VAR to true.
 *   #undef VAR                   ! To set the logical VAR to false.
 *   VAR = False                  ! To set the logical VAR to false.
 *   #define VAR 999              ! To set the real or integer VAR to 999.
 *   VAR = 999                    ! To set the real or integer VAR to 999.
 *   #override VAR = 888          ! To override a previously set value.
 *   VAR = 1.1, 2.2, 3.3          ! To set an array of real values.
 *   HALF = 0.5	                  ! Set the value of HALF to 0.5
 *   NAME = "abc"	                ! Set the string NAME to 'abc'
 *   VECTOR = 1.0,2.0	            ! Set the array VECTOR to [1.0, 2.0]
 *   NAMES = 'abc','xyz'	        ! Set the strings NAMES to 'abc','xyz'
 *   #override DO_THIS = False	  ! Set the Boolean to .FALSE., ignoring the above specification
 *   #override HALF = 0.25	      ! Set the value of HALF to 0.25, ignoring the above value
 *   #override HALF = 0.25	      ! Same as the above value of HALF to 0.25 so is accepted
 * @endcode
 *
 * Key-value pairs may be written as:
 * @code
 *   key = value
 *   ModuleName%key = value   ! when defined outside a module block
 * @endcode
 *
 * Example usage:
 * @code
 *   RuntimeParams params("path/to/params.txt");
 * @endcode
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include <iostream>
#include "MOM_string_functions.h"
#include "MOM_parser_utilities.h"

using mom_parser_utilities::ParamValue;

class RuntimeParams {
public:
  explicit RuntimeParams(const std::string& path);
  explicit RuntimeParams(const std::vector<std::string>& paths);  

  /// @brief Get a parameter value from a module
  /// @param key The parameter key
  /// @param module The module name
  /// @return The parameter value as a ParamValue
  /// @throws std::out_of_range if the module or key does not exist
  const ParamValue& get(const std::string& key, const std::string& module = "") const;

  template<typename T>
  /// @brief Get a parameter value as a specific type.
  /// @tparam T The expected type of the parameter value.
  /// @param key The parameter key
  /// @param module The module name (empty string for global scope)
  /// @return The parameter value as type T
  /// @throws std::runtime_error if the parameter is not of type T
  T get_as(const std::string& key, const std::string& module = "") const;

  /// @brief Check if a parameter exists
  /// @param key The parameter key
  /// @param module The module name
  /// @return true if the parameter exists, false otherwise
  bool has_param(const std::string& key, const std::string& module = "") const;

  /// @brief Get all modules
  /// @return A vector of module names
  std::vector<std::string> get_modules() const;

  size_t get_num_parameters() const;

private:

  std::string path_;
  std::unordered_map<std::string, std::unordered_map<std::string, ParamValue>> table_;
};
