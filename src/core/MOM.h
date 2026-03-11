#pragma once
/// @file MOM.h
/// @brief Main header for the Modular Ocean Model (MOM) core.

namespace MOM {

/// @brief The Model class serves as the main interface for the Modular Ocean Model (MOM) core.
/// It encapsulates the runtime parameters and provides an entry point for initializing, running,
/// and finalizing the model.
class Model {
public:

  /// @brief Model constructor that initializes the model with the given ensemble number.
  /// @param ensemble_num The ensemble number for the model run; default is -1 (indicating no ensemble).
  explicit Model(int ensemble_num = -1);

private:
  const int ensemble_num_;
};

} // namespace MOM
