/**
 * @file MOM_document.h
 * @brief Documentation generator for MOM runtime parameters.
 *
 * @details
 * Provides hooks for documenting runtime parameter values, defaults, and
 * descriptions to a set of output files at various levels of detail:
 *
 *   - `.all`       — every non-layout, non-debugging parameter
 *   - `.short`     — only parameters that differ from their defaults
 *   - `.layout`    — layout parameters
 *   - `.debugging` — debugging parameters
 *
 * This is a C++ reimplementation of the Fortran MOM_document module.
 */

#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

/// @brief Supported parameter value types for documentation.
using DocValue = std::variant<bool, std::int64_t, double, std::string, std::vector<bool>, std::vector<std::int64_t>,
                              std::vector<double>, std::vector<std::string>>;

/// @brief Options controlling how a parameter is documented.
struct DocParamOptions {
  bool layout_param = false;    ///< If true, write to the layout doc file.
  bool debugging_param = false; ///< If true, write to the debugging doc file.
  bool like_default = false;    ///< If true, treat the value as if it equals the default.
  std::string module = "";      ///< The module this parameter belongs to.
};

/// @brief A structure that controls documentation output: file destinations,
///        formatting, and duplicate tracking.
class DocFileWriter {
public:
  /// @brief Initialize the documentation system.
  /// @param doc_file_base Base path/name for documentation files (e.g. "MOM_parameter_doc").
  /// @param complete  If true, write the .all file documenting every parameter.
  /// @param minimal   If true, write the .short file documenting non-default parameters.
  /// @param layout    If true, write the .layout file.
  /// @param debugging If true, write the .debugging file.
  DocFileWriter(const std::string &doc_file_base, bool complete = true, bool minimal = true, bool layout = true,
                bool debugging = true);

  ~DocFileWriter();

  // Non-copyable, movable
  DocFileWriter(const DocFileWriter &) = delete;
  DocFileWriter &operator=(const DocFileWriter &) = delete;
  DocFileWriter(DocFileWriter &&) = default;
  DocFileWriter &operator=(DocFileWriter &&) = default;

  // ---- Parameter documentation ----

  /// @brief Document a boolean parameter.
  void doc_param(const std::string &varname, const std::string &desc, const std::string &units, bool val,
                 std::optional<bool> default_val = std::nullopt, const DocParamOptions &opts = {});

  /// @brief Document an integer parameter.
  void doc_param(const std::string &varname, const std::string &desc, const std::string &units, std::int64_t val,
                 std::optional<std::int64_t> default_val = std::nullopt, const DocParamOptions &opts = {});

  /// @brief Document a real (double) parameter.
  void doc_param(const std::string &varname, const std::string &desc, const std::string &units, double val,
                 std::optional<double> default_val = std::nullopt, const DocParamOptions &opts = {});

  /// @brief Document a string parameter.
  void doc_param(const std::string &varname, const std::string &desc, const std::string &units, const std::string &val,
                 std::optional<std::string> default_val = std::nullopt, const DocParamOptions &opts = {});

  /// @brief Document a vector-of-bool parameter.
  void doc_param(const std::string &varname, const std::string &desc, const std::string &units,
                 const std::vector<bool> &vals, std::optional<std::vector<bool>> default_val = std::nullopt,
                 const DocParamOptions &opts = {});

  /// @brief Document a vector-of-int parameter.
  void doc_param(const std::string &varname, const std::string &desc, const std::string &units,
                 const std::vector<std::int64_t> &vals,
                 std::optional<std::vector<std::int64_t>> default_val = std::nullopt, const DocParamOptions &opts = {});

  /// @brief Document a vector-of-double parameter.
  void doc_param(const std::string &varname, const std::string &desc, const std::string &units,
                 const std::vector<double> &vals, std::optional<std::vector<double>> default_val = std::nullopt,
                 const DocParamOptions &opts = {});

  /// @brief Document a vector-of-string parameter.
  void doc_param(const std::string &varname, const std::string &desc, const std::string &units,
                 const std::vector<std::string> &vals,
                 std::optional<std::vector<std::string>> default_val = std::nullopt, const DocParamOptions &opts = {});

  // ---- Module / block documentation ----

  /// @brief Document a module header.
  void doc_module(const std::string &modname, const std::string &desc, bool layout_mod = false,
                  bool debugging_mod = false, bool all_default = false);

  /// @brief Close a module (write closing block).
  void close_module();

  /// @brief Open a parameter block (adds blockName% to the current prefix).
  void doc_openBlock(const std::string &blockName, const std::string &desc = "");

  /// @brief Close a parameter block (removes blockName% from the current prefix).
  void doc_closeBlock(const std::string &blockName);

  // ---- Accessors (mainly for testing) ----

  /// @brief Return the number of documented parameters so far.
  size_t num_documented() const { return documented_.size(); }

  /// @brief Return the current block prefix.
  const std::string &block_prefix() const { return block_prefix_; }

  /// @brief Return whether any files are open for writing.
  bool files_are_open() const { return files_are_open_; }

  /// @brief Internal: Handle module transitions (open new module if needed).
  void handle_module_transition(const std::string &new_module);

private:
  // ---- Internal helpers ----

  /// @brief Lazily open the documentation files.
  void open_files();

  /// @brief Format "VARNAME = value   !   [units]"
  std::string define_string(const std::string &varname, const std::string &valstring, const std::string &units) const;

  /// @brief Format "VARNAME = False   !   [units]" (for false booleans)
  std::string undef_string(const std::string &varname, const std::string &units) const;

  /// @brief Write a message line + wrapped description to the appropriate files.
  void write_message_and_desc(const std::string &mesg, const std::string &desc, bool value_was_default = false,
                              bool layout_param = false, bool debugging_param = false);

  /// @brief Check if this parameter has already been documented.  Returns true
  ///        if it was (and warns on conflicting values).
  bool mesg_has_been_documented(const std::string &varname, const std::string &mesg);

  /// @brief Format a double as a compact string (mirroring Fortran real_string).
  static std::string real_string(double val);

  /// @brief Format an integer as a string.
  static std::string int_string(std::int64_t val);

  // ---- Data members ----

  std::string doc_file_base_; ///< Base name for doc files
  bool complete_ = true;      ///< Write .all file?
  bool minimal_ = true;       ///< Write .short file?
  bool layout_ = true;        ///< Write .layout file?
  bool debugging_ = true;     ///< Write .debugging file?

  std::ofstream file_all_;       ///< Stream for .all
  std::ofstream file_short_;     ///< Stream for .short
  std::ofstream file_layout_;    ///< Stream for .layout
  std::ofstream file_debugging_; ///< Stream for .debugging
  bool files_are_open_ = false;

  int comment_column_ = 32; ///< Column at which comments start
  int max_line_len_ = 112;  ///< Max line length for descriptions

  std::string block_prefix_;   ///< Current block prefix (e.g. "KPP%")
  std::string current_module_; ///< Current module name for closing blocks

  /// @brief Previously documented messages, keyed by (prefix+varname) -> mesg.
  std::unordered_map<std::string, std::string> documented_;
};
