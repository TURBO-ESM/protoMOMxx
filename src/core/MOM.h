#pragma once
/// @file MOM.h
/// @brief Main header for the Modular Ocean Model (MOM) core.
#include "MOM_file_parser.h"

/// @brief The MOM class serves as the main interface for the Modular Ocean Model (MOM) core.
/// It encapsulates the runtime parameters and provides an entry point for initializing, running,
/// and finalizing the model.
class MOM {
public:
  /// @brief MOM constructor that initializes the model with the given ensemble number.
  /// @param ensemble_num The ensemble number for the model run; default is -1 (indicating no ensemble).
  explicit MOM(int ensemble_num = -1);

  /// @brief Unique pointer to the RuntimeParams object that holds the model's runtime parameters.
  std::unique_ptr<RuntimeParams> params;

private:
  bool verbosity_ = false;
};