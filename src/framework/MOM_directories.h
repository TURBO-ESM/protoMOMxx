/**
 * @file MOM_directories.h
 * @brief Container for paths and parameter file names.
 *
 * @details
 * The namelist parameters indicate which directories to use for certain types of
 * input and output, and which files to look in for the full parsable input parameter file(s).
 */
#pragma once
#include <string>
#include <vector>

namespace MOM{

/// @brief Container for paths and parameter file names.
///
/// Reads the namelist parameters that indicate which directories to use for certain types of
/// input and output, and which files to look in for the full parsable input parameter file(s).
class Directories {
public:
  /// @brief Construct a Directories object by reading the namelist parameters.
  /// @param ensemble_num Optional ensembler number to apply to directory and file names.
  explicit Directories(int ensemble_num = -1);

  /// @return The directory to read restart and input files from.
  const std::string& restart_input_dir() const;
  /// @return The directory to write restart files to.
  const std::string& restart_output_dir() const;
  /// @return The directory to write model output to.
  const std::string& output_directory() const;
  /// @return The input filename indicating how the run segment should be started.
  const std::string& input_filename() const;
  /// @return The list of parameter files to read for model initialization (e.g., MOM_input, MOM_override, etc.).
  const std::vector<std::string>& parameter_filenames() const;

private:

  std::string restart_input_dir_;  ///< Directory to read restart and input files.
  std::string restart_output_dir_; ///< Directory to write restart files.
  std::string output_directory_;   ///< Directory to write model output.
  std::string input_filename_;  ///< Indicates input files or how the run segment should be started.
  std::vector<std::string> parameter_filenames_; ///< List of parameter files to read for model initialization.
};

} // namespace MOM
