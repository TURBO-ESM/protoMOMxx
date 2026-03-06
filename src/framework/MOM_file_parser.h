/**
 * @file MOM_file_parser.h
 * @brief Lightweight parser for MOM runtime parameter files.
 *
 * @details
 * Provides robust parsing of MOM runtime parameter files with support for C-style block comments,
 * Fortran-style line comments, quoted strings, and detailed error reporting. Supported value types:
 * - Scalars: bool, int, double, string
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

#include "MOM_document.h"
#include "MOM_get_input.h"
#include "MOM_parser_utilities.h"
#include "MOM_string_functions.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

using mom_parser_utilities::ParamValue;

/// @brief Options for getting a parameter value.
///
/// @details Controls default value fallback, module attribution, documentation
/// output, and error handling behavior for RuntimeParams::get().
struct ParamGetOptions {
  std::optional<ParamValue> default_value; ///< The default value of the parameter
  std::string module = "";        ///< The name of the calling module
  std::string desc = "";          ///< A description of this variable;
                                  ///< If empty, this parameter is not written to a doc file.
  std::string units = "";         ///< The units of this parameter
  bool fail_if_missing = false;   ///< If true, get() will throw std::out_of_range if the parameter is missing.
  bool do_not_read = false;       ///< If true, do not read a value for this parameter
                                  ///< although it may be logged.
  bool do_not_log = false;        ///< If true, do not log this parameter to a doc file.
  bool layout_param = false;      ///< If true, this parameter is logged in the layout parameter file.
  bool debugging_param = false;   ///< If true, this parameter is logged in the debugging parameter file.
};

/// @brief The RuntimeParams class provides an interface for parsing MOM runtime parameter files and retrieving values.
/// 
/// Supported value types include scalars (bool, int, double, string) and homogeneous comma-separated lists of the
/// above types. It supports C-style block comments, Fortran-style line comments, quoted strings, and error reporting.
/// Example usage:
/// @code
///   RuntimeParams params("path/to/params.txt");
///   bool REENTRANT_X = false;
///   params.get("REENTRANT_X", REENTRANT_X,
///              {.default_value = true,
///               .desc = "If true, the domain is zonally reentrant",
///               .units = "nondim"});
/// @endcode
class RuntimeParams {
public:
  /// @brief Construct a RuntimeParams object by parsing the specified parameter file.
  /// @param path The path to a single parameter file to parse.
  explicit RuntimeParams(const std::string &path);
  /// @brief Construct a RuntimeParams object by parsing multiple parameter files in order.
  /// @param paths A vector of paths to parameter files to parse in order (later files override earlier ones).
  explicit RuntimeParams(const std::vector<std::string> &paths);

  /// @brief Get a parameter value with the specified key and type, applying the provided options.
  /// @tparam T The expected type of the parameter value.
  /// @param key The parameter key to look up.
  /// @param value An output reference to store the retrieved parameter value.
  /// @param options A ParamGetOptions struct controlling default fallback, documentation, and error handling behavior.
  template <typename T>
  void get(const std::string &key, T &value, const ParamGetOptions &options = ParamGetOptions{}) const;

  /// @brief Get a parameter value from a module
  /// @param key The parameter key
  /// @param module The module name
  /// @return The parameter value as a ParamValue
  /// @throws std::out_of_range if the module or key does not exist
  const ParamValue &get_variant(const std::string &key, const std::string &module = "") const;

  /// @brief Check if a parameter exists
  /// @param key The parameter key
  /// @param module The module name
  /// @return true if the parameter exists, false otherwise
  bool has_param(const std::string &key, const std::string &module = "") const;

  /// @brief Get all modules
  /// @return A vector of module names
  std::vector<std::string> get_modules() const;

  /// @brief Get the number of parameters in this RuntimeParams object (for testing)
  /// @return The total number of parameters across all modules.
  size_t get_num_parameters() const;

  /// @brief Set the documentation writer for this parameter file.
  /// @param doc Shared pointer to a DocFileWriter. Pass nullptr to disable documentation.
  void set_doc(std::shared_ptr<DocFileWriter> doc) { doc_ = std::move(doc); }

  /// @brief Get the documentation writer.
  /// @return Shared pointer to the DocFileWriter, or nullptr if documentation is disabled.
  std::shared_ptr<DocFileWriter> get_doc() const { return doc_; }

  /// @brief Document a module header.
  /// @param modname The name of the module.
  /// @param desc A description of the module (written to doc file).
  /// @param layout_mod If true, treat this as a layout module (write to .layout file).
  /// @param debugging_mod If true, treat this as a debugging module (write to .debugging file).
  /// @param all_default If true, treat all parameters in this module as if they equal their defaults (only write to .all file).
  void doc_module(const std::string &modname, const std::string &desc, bool layout_mod = false,
                  bool debugging_mod = false, bool all_default = false) {
    if (doc_)
      doc_->doc_module(modname, desc, layout_mod, debugging_mod, all_default);
  }

  /// @brief Close a module (write closing block).
  void close_module() {
    if (doc_)
      doc_->close_module();
  }

private:
  std::string path_;
  std::unordered_map<std::string, std::unordered_map<std::string, ParamValue>> table_;
  std::shared_ptr<DocFileWriter> doc_; ///< Optional documentation writer
};
