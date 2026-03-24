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

#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace MOM {

/// @brief Options controlling how a parameter is documented.
struct DocParamOptions {
  bool layout_param = false;    ///< If true, write to the layout doc file.
  bool debugging_param = false; ///< If true, write to the debugging doc file.
  bool like_default =
      false; ///< If true, treat the value as if it equals the default.
};

/// @brief Wrapper around std::ofstream that manages open/close with error
/// checking.
struct DocFile {
  std::ofstream stream; ///< The underlying output file stream.
  bool enabled = false; ///< Whether this file category is active.

  /// @brief Open the file and write a header. No-op if disabled or already
  /// open.
  /// @param path The file path to open.
  /// @param header A header string to write at the top of the file after
  /// opening.
  /// @throws std::runtime_error if the file cannot be opened.
  void open(const std::string &path, std::string_view header);

  /// @brief Close the file if open. Safe to call multiple times.
  void close() noexcept;

  /// @brief Return whether the underlying stream is open.
  /// @return True if the underlying stream is open, false otherwise.
  bool is_open() const { return stream.is_open(); }
};

/// @brief A structure that controls documentation output: file destinations,
///        formatting, and duplicate tracking.
class DocFileWriter {
public:
  /// @brief Initialize the documentation system.
  /// @param doc_file_base Base path/name for documentation files (e.g.
  /// "MOM_parameter_doc").
  /// @param complete  If true, write the .all file documenting every parameter.
  /// @param minimal   If true, write the .short file documenting non-default
  /// parameters.
  /// @param layout    If true, write the .layout file.
  /// @param debugging If true, write the .debugging file.
  DocFileWriter(const std::string &doc_file_base, bool complete = true,
                bool minimal = true, bool layout = true, bool debugging = true);

  /// @brief Safety-net destructor: closes all files silently.
  ///        Prefer calling close() explicitly to detect I/O errors.
  ~DocFileWriter() noexcept;

  /// @brief Finalize documentation: close the current module (if any) and close
  /// all files.
  ///
  /// Call this explicitly before destruction so that any I/O errors can be
  /// reported via exceptions. The destructor only closes streams silently.
  void close();

  /// @brief Copy constructor and assignment operator (deleted to prevent
  /// copying).
  DocFileWriter(const DocFileWriter &) = delete;
  /// @brief Copy constructor and assignment operator (deleted to prevent
  /// copying).
  DocFileWriter &operator=(const DocFileWriter &) = delete;
  /// @brief Move constructor and assignment operator (deleted).
  DocFileWriter(DocFileWriter &&) = delete;
  /// @brief Move assignment operator (deleted).
  DocFileWriter &operator=(DocFileWriter &&) = delete;

  // ---- Parameter documentation ----

  /// @brief Document a scalar parameter (bool, int, double, or std::string).
  /// @param varname The name of the parameter variable.
  /// @param desc A description of the parameter (written to doc file).
  /// @param units The units of the parameter (written to doc file).
  /// @param val The value of the parameter.
  /// @param default_val The default value of the parameter (optional; used to
  /// determine whether to write to the .short file).
  /// @param opts Additional options controlling documentation behavior (e.g.
  /// which files to write to).
  template <typename T>
  void doc_param(std::string_view varname, std::string_view desc,
                 std::string_view units, const T &val,
                 std::optional<T> default_val = std::nullopt,
                 const DocParamOptions &opts = {});

  /// @brief Document a vector parameter (vector of bool, int, double, or
  /// std::string).
  /// @param varname The name of the parameter variable.
  /// @param desc A description of the parameter (written to doc file).
  /// @param units The units of the parameter (written to doc file).
  /// @param vals The values of the parameter.
  /// @param default_val The default value of the parameter (optional; used to
  /// determine whether to write to the .short file).
  /// @param opts Additional options controlling documentation behavior (e.g.
  /// which files to write to).
  template <typename T>
  void doc_param(std::string_view varname, std::string_view desc,
                 std::string_view units, const std::vector<T> &vals,
                 std::optional<std::vector<T>> default_val = std::nullopt,
                 const DocParamOptions &opts = {});

  // ---- Module / block documentation ----

  /// @brief Document a module header.
  /// @param modname The name of the module.
  /// @param desc A description of the module (written to doc file).
  /// @param layout_mod If true, treat this as a layout module (write to .layout
  /// file).
  /// @param debugging_mod If true, treat this as a debugging module (write to
  /// .debugging file).
  /// @param all_default If true, treat all parameters in this module as if they
  /// equal their defaults (only write to .all file).
  void doc_module(std::string_view modname, std::string_view desc,
                  bool layout_mod = false, bool debugging_mod = false,
                  bool all_default = false);

  /// @brief Close a module (write closing block).
  void close_module();

  /// @brief Open a parameter block (sets the block prefix to blockName%).
  /// Nesting is not supported.
  /// @param blockName The name of the block to open.
  /// @param desc A description of the block (written to doc file).
  void open_block(std::string_view blockName, std::string_view desc = "");

  /// @brief Close the most recently opened parameter block.
  /// @throws std::logic_error if no block is currently open.
  void close_block();
  // ---- Accessors (mainly for testing) ----

  /// @brief Return the number of documented parameters so far.
  /// @return The number of documented parameters so far.
  size_t num_documented() const { return documented_.size(); }

  /// @brief Return the current block prefix.
  /// @return The current block prefix (e.g. "KPP%").
  const std::string &block_prefix() const { return block_prefix_; }

  /// @brief Return whether any files are open for writing.
  /// @return True if any documentation files are currently open for writing,
  /// false otherwise.
  bool files_are_open() const { return files_are_open_; }

private:
  // ---- Internal helpers ----

  /// @brief Lazily open the documentation files.
  void open_files();

  /// @brief Common preamble for doc_param overloads: open files.
  /// @return true if files are open and writing can proceed, false otherwise.
  bool prepare_doc();

  /// @brief Common postamble for doc_param overloads: dedup check and write.
  void finalize_doc(std::string_view varname, std::string_view desc,
                    std::string_view mesg, bool equals_default,
                    const DocParamOptions &opts);

  /// @brief Format "VARNAME = value   !   [units]"
  std::string define_string(std::string_view varname,
                            std::string_view valstring,
                            std::string_view units) const;

  /// @brief Format "VARNAME = False   !   [units]" (for false booleans)
  std::string undef_string(std::string_view varname,
                           std::string_view units) const;

  /// @brief Write a message line + wrapped description to the appropriate
  /// files.
  void write_message_and_desc(std::string_view mesg, std::string_view desc,
                              bool value_was_default = false,
                              bool layout_param = false,
                              bool debugging_param = false);

  /// @brief Check if this parameter has already been documented.  Returns true
  ///        if it was (and warns on conflicting values).
  bool mesg_has_been_documented(std::string_view varname,
                                std::string_view mesg);

public:
  /// @brief Format a double as a compact string (mirroring Fortran
  /// real_string).
  /// @param val The double value to format.
  /// @return A string representation of the double, using fixed or scientific
  /// notation as appropriate.
  static std::string real_string(double val);

private:
  // ---- Data members ----

  std::string doc_file_base_; ///< Base name for doc files

  DocFile file_all_;    ///< .all — every non-layout, non-debugging parameter
  DocFile file_short_;  ///< .short — only non-default parameters
  DocFile file_layout_; ///< .layout — layout parameters
  DocFile file_debugging_; ///< .debugging — debugging parameters
  bool files_are_open_ = false;
  bool open_attempted_ =
      false; ///< True after first open_files() call, to avoid retrying

  static constexpr int comment_column_ = 32; ///< Column at which comments start
  static constexpr int max_line_len_ =
      112; ///< Max line length for descriptions
  static constexpr int comment_prefix_len_ =
      2; ///< Length of "! " prefix in comment lines

  std::string block_prefix_; ///< Current block prefix (e.g. "KPP%")
  std::string
      current_block_name_; ///< Name of the currently open block (empty if none)
  std::string current_module_; ///< Current module name for closing blocks
  bool current_module_layout_ =
      false; ///< Whether the current module is a layout module
  bool current_module_debugging_ =
      false; ///< Whether the current module is a debugging module
  bool current_module_all_default_ =
      false; ///< Whether all params in the current module equal defaults

  /// @brief Previously documented messages, keyed by (prefix+varname) -> mesg.
  std::unordered_map<std::string, std::string> documented_;
};

} // namespace MOM