/**
 * @file MOM_directories.cpp
 * @brief Implementation of the Directories class, which contains paths and parameter file names.
 */

#include "MOM_directories.h"
#include "MOM_logger.h"
#include "MOM_io.h"
#include "MOM_nml_parser.h"
#include <stdexcept>

namespace MOM{

Directories::Directories(int ensemble_num) {

  logger::debug("Reading directories and parameter file names from input.nml...");

  const std::string nml_filename = "input.nml";
  NamelistParams nml(nml_filename);

  const std::string nml_group = "MOM_INPUT_NML";

  auto get_MOM_input_nml_param = [&](const std::string& param_name) -> std::string {
    if (nml.has_param(param_name, nml_group)) {
      return nml.get<std::string>(param_name, nml_group);
    }
    return "";
  };

  output_directory_   = io::ensembler(get_MOM_input_nml_param("OUTPUT_DIRECTORY"), ensemble_num);
  restart_input_dir_  = io::ensembler(get_MOM_input_nml_param("RESTART_INPUT_DIR"), ensemble_num);
  restart_output_dir_ = io::ensembler(get_MOM_input_nml_param("RESTART_OUTPUT_DIR"), ensemble_num);
  input_filename_     = io::ensembler(get_MOM_input_nml_param("INPUT_FILENAME"), ensemble_num);

  // Read the parameter file names, which can be a single string or a list of strings like
  // MOM_input, MOM_override, etc. The ensembler number is applied to each file name if provided.
  if (nml.has_param("PARAMETER_FILENAME", nml_group)) {
    const auto& val = nml.get_variant("PARAMETER_FILENAME", nml_group);
    if (std::holds_alternative<std::vector<std::string>>(val)) {
      for (const auto& fname : std::get<std::vector<std::string>>(val)) {
        if (!fname.empty())
          parameter_filenames_.push_back(io::ensembler(fname, ensemble_num));
      }
    } else if (std::holds_alternative<std::string>(val)) {
      const auto& fname = std::get<std::string>(val);
      if (!fname.empty())
        parameter_filenames_.push_back(io::ensembler(fname, ensemble_num));
    }
  }

  if (parameter_filenames_.empty()) {
    throw std::runtime_error(
        "At least one valid PARAMETER_FILENAME entry is required in "
        "MOM_INPUT_NML in: " + nml_filename);
  }

  logger::debug("Parameter files: ");
  for (const auto& fname : parameter_filenames()) {
    logger::debug("  ", fname);
  }
  logger::debug("Output directory: ", output_directory());
}

// Accessors for the various directories and parameter file names.
const std::string& Directories::input_filename() const { return input_filename_; }
const std::string& Directories::restart_input_dir() const { return restart_input_dir_; }
const std::string& Directories::restart_output_dir() const { return restart_output_dir_; }
const std::string& Directories::output_directory() const { return output_directory_; }
const std::vector<std::string>& Directories::parameter_filenames() const { return parameter_filenames_; }

} // namespace MOM
