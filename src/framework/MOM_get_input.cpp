/**
 * @file MOM_get_input.cpp
 * @brief Implementation of MOM input bootstrapping functionality.
 */

#include "MOM_get_input.h"
#include "MOM_nml_parser.h"
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace MOM_get_input {

Directories get_directories(const fs::path &namelist_file, int ensemble_num) {
  NamelistParams nml(namelist_file);
  Directories dirs;

  auto get_str = [&](const char *key) -> std::string {
    return nml.has_param(key, "MOM_INPUT_NML") ? nml.get_as<std::string>(key, "MOM_INPUT_NML") : std::string{};
  };

  dirs.output_directory = MOM_io::ensembler(get_str("OUTPUT_DIRECTORY"), ensemble_num);
  dirs.restart_input_dir = MOM_io::ensembler(get_str("RESTART_INPUT_DIR"), ensemble_num);
  dirs.restart_output_dir = MOM_io::ensembler(get_str("RESTART_OUTPUT_DIR"), ensemble_num);
  dirs.input_filename = MOM_io::ensembler(get_str("INPUT_FILENAME"), ensemble_num);

  return dirs;
}

std::vector<std::string> get_parameter_filenames(const fs::path &namelist_file, int ensemble_num) {
  NamelistParams nml(namelist_file);
  std::vector<std::string> parameter_filenames;

  if (nml.has_param("PARAMETER_FILENAME", "MOM_INPUT_NML")) {
    const auto &val = nml.get("PARAMETER_FILENAME", "MOM_INPUT_NML");
    if (std::holds_alternative<std::vector<std::string>>(val)) {
      for (const auto &fname : std::get<std::vector<std::string>>(val)) {
        if (!fname.empty())
          parameter_filenames.push_back(MOM_io::ensembler(fs::path(fname), ensemble_num).string());
      }
    } else if (std::holds_alternative<std::string>(val)) {
      const auto &fname = std::get<std::string>(val);
      if (!fname.empty())
        parameter_filenames.push_back(MOM_io::ensembler(fs::path(fname), ensemble_num).string());
    }
  }

  if (parameter_filenames.empty()) {
    throw std::runtime_error("At least one valid PARAMETER_FILENAME entry is required in "
                             "MOM_input_nml in: " +
                             namelist_file.string());
  }

  return parameter_filenames;
}

} // namespace MOM_get_input