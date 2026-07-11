#pragma once
/// @file MOM_infra.h
/// @brief Startup/shutdown of the infrastructure layer (AMReX).

namespace MOM {

/// @brief Starts up the infrastructure layer (AMReX) when created, and shuts
/// it down when it goes out of scope.
///
/// The driver creates one Infra object at the beginning of the run, which
/// initializes AMReX. When the object goes out of scope at the end of the
/// run, its destructor finalizes AMReX automatically (RAII).
///
/// todo: accept a communicator argument
class Infra {
public:
  /// @brief Initialize the infrastructure layer (AMReX).
  /// @param argc Command-line argument count (AMReX may consume arguments).
  /// @param argv Command-line argument vector (AMReX may consume arguments).
  Infra(int &argc, char **&argv);

  /// @brief Finalize the infrastructure layer (AMReX).
  ~Infra();

  // Exactly one Infra object should exist per run. (A copy would finalize AMReX twice).
  // Multiple model instances are expected to share this single infrastructure runtime, 
  // since AMReX's runtime state (communicator, parameter table, memory arenas) is global.
  Infra(const Infra &) = delete;
  Infra &operator=(const Infra &) = delete;
  Infra(Infra &&) = delete;
  Infra &operator=(Infra &&) = delete;
};

} // namespace MOM
