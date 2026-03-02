/**
 * @file MOM_get_input.h
 * @brief Reads the only namelist needed to bootstrap the model.
 *
 * @details
 * The namelist parameters indicate which directories to use for certain types of
 * input and output, and which files to look in for the full parsable input parameter file(s).
 */
#pragma once
#include "MOM_io.h"
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace MOM_get_input {

/// @brief Container for paths and parameter file names.
struct Directories {
  std::filesystem::path restart_input_dir;   ///< Directory to read restart and input files.
  std::filesystem::path restart_output_dir;  ///< Directory to write restart files.
  std::filesystem::path output_directory;    ///< Directory to write model output.
  std::string           input_filename;      ///< Indicates input files or how the run segment should be started.
};

/// @brief Read the MOM_input_nml namelist to get directory paths and input file names.
/// @param namelist_file The path to the namelist file to read.
/// @param ensemble_num The ensemble number for ensembler processing (use -1 if not applicable).
/// @return A Directories struct containing the parsed paths and input filename.
Directories get_directories(
  const std::filesystem::path& namelist_file=std::filesystem::path("input.nml"),
  int ensemble_num=-1);

/// @brief Read the MOM_input_nml namelist to get the list of parameter file names.
/// @param namelist_file The path to the namelist file to read.
/// @param ensemble_num The ensemble number for ensembler processing (use -1 if not applicable).
/// @return A vector of parameter file names parsed from the namelist.
/// @throws std::runtime_error if no valid PARAMETER_FILENAME entry is found in the namelist.
std::vector<std::string> get_parameter_filenames(
  const std::filesystem::path& namelist_file=std::filesystem::path("input.nml"),
  int ensemble_num=-1);
}