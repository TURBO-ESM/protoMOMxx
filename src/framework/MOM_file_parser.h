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
 *   BlockName%key = value   ! when defined outside a block
 * @endcode
 *
 * Example usage:
 * @code
 *   RuntimeParams params("path/to/params.txt");
 * @endcode
 */

#pragma once

#include "MOM_document.h"
#include "MOM_param_table.h"
#include "MOM_parser_utils.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MOM {

using parser_utils::ParamValue;

/// @brief Options for getting a parameter value.
///
/// @details Controls default value fallback, documentation output, and error
/// handling behavior for RuntimeParams::get(). The block context is set via
/// RuntimeParams::open_block()/close_block() rather than per-call options.
struct ParamGetOptions {
  std::optional<ParamValue> default_value; ///< The default value of the parameter
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
  /// @param doc_file_base The base name for the documentation file. If non-empty, a DocFileWriter will be created to write documentation for parsed parameters.
  explicit RuntimeParams(const std::string &path, const std::string &doc_file_base = "");

  /// @brief Construct a RuntimeParams object by parsing multiple parameter files in order.
  /// @param paths A vector of paths to parameter files to parse in order (later files override earlier ones).
  /// @param doc_file_base The base name for the documentation file. If non-empty, a DocFileWriter will be created to write documentation for parsed parameters.
  explicit RuntimeParams(const std::vector<std::string> &paths, const std::string &doc_file_base = "");

  /// @brief Get a parameter value with the specified key and type, applying the provided options.
  /// @tparam T The expected type of the parameter value.
  /// @param key The parameter key to look up.
  /// @param value An output reference to store the retrieved parameter value.
  /// @param options A ParamGetOptions struct controlling default fallback, documentation, and error handling behavior.
  template <typename T>
  void get(const std::string &key, T &value, const ParamGetOptions &options = ParamGetOptions{}) const;

  /// @brief Get a parameter value, using the current block context.
  /// @param key The parameter key
  /// @return The parameter value as a ParamValue
  /// @throws std::out_of_range if the key does not exist in the current block
  const ParamValue &get_variant(const std::string &key) const;

  /// @brief Check if a parameter exists in the current block context.
  /// @param key The parameter key
  /// @return true if the parameter exists, false otherwise
  bool has_param(const std::string &key) const;

  /// @brief Get all blocks
  /// @return A vector of block names
  std::vector<std::string> get_blocks() const;

  /// @brief Get the number of parameters in this RuntimeParams object (for testing)
  /// @return The total number of parameters across all blocks.
  size_t get_num_parameters() const;

  /// @brief Document a module header.
  /// @param modname The name of the module.
  /// @param desc A description of the module (written to doc file).
  /// @param layout_mod If true, treat this as a layout module (write to .layout file).
  /// @param debugging_mod If true, treat this as a debugging module (write to .debugging file).
  /// @param all_default If true, treat all parameters in this module as if they equal their defaults (only write to .all file).
  void doc_module(const std::string &modname, const std::string &desc, bool layout_mod = false,
                  bool debugging_mod = false, bool all_default = false);

  /// @brief Close a module (write closing block).
  void close_module();

  /// @brief Open a parameter block. Subsequent get() calls will look up keys
  /// in this block's group in the parameter table (e.g., open_block("KPP") causes
  /// get("N_SMOOTH", ...) to look up KPP%N_SMOOTH). Also writes the block
  /// marker to docs if a doc writer is attached.
  /// @param blockName The name of the block to open.
  /// @param desc A description of the block (written to doc file).
  void open_block(const std::string &blockName, const std::string &desc = "");

  /// @brief Close the currently open parameter block.
  /// @throws std::logic_error if no block is currently open.
  void close_block();

  /// @brief Get the current block name (empty string if no block is open).
  /// @return The current block name (empty string if no block is open).
  const std::string &current_block() const { return current_block_; }

private:
  ParamTable table_{false}; // case-sensitive
  std::string current_block_;  ///< Currently open block name for get() lookups
  std::unique_ptr<DocFileWriter> doc_; ///< Optional documentation writer
};

} // namespace MOM
