#pragma once
/// @file MOM_infra.h
/// @brief Startup/shutdown of the infrastructure layer (MPI and AMReX), as a
///        thin wrapper over TIM::Runtime.

#include <mpi.h>

#include "core/tim_runtime.hpp"

namespace MOM {

/// @brief Owns the infrastructure runtime for a solo run: brings up MPI and
/// then AMReX (as a guest on the resulting communicator) when created, and
/// shuts them down in reverse order when it goes out of scope.
///
/// Infra is a thin wrapper over TIM::Runtime in owner mode (the analogue of
/// MOM6's MOM_infra_init, which is itself a thin wrapper over fms_init). All
/// of the initialization and shutdown machinery (MPI_Init/MPI_Finalize,
/// amrex::Initialize/Finalize, and the teardown ordering) lives in
/// TIM::Runtime. Infra keeps the familiar MOM_infra_init name and is the home
/// for future MOM-specific communicator policy: ensemble members, or a
/// coupler-supplied communicator adopted through TIM::Runtime's guest mode.
/// The driver creates one Infra at the start of the run. Its destructor tears
/// the runtime down automatically (RAII), and in correct order.
///
/// The communicator is MPI_COMM_WORLD for now; communicator partitioning
/// (e.g., for ensemble members or a coupler handing the ocean a communicator)
/// is a future extension of this class, expressed by selecting TIM::Runtime's
/// guest-mode constructor.
class Infra {
public:
  /// @brief Initialize the infrastructure layer via TIM::Runtime owner mode:
  /// MPI first, then AMReX as a guest on the owned communicator.
  /// @param argc Command-line argument count.
  /// @param argv Command-line argument vector.
  ///
  /// TIM::Runtime aborts (it does not throw) if MPI is already initialized:
  /// Infra owns the process, and a second owner indicates a driver bug.
  Infra(int &argc, char **&argv);

  /// @brief Finalize the infrastructure layer (AMReX, then MPI). The work is
  /// done by TIM::Runtime's RAII teardown.
  ~Infra() = default;

  /// @brief The top-level communicator the run executes on.
  /// @return The communicator TIM::Runtime owns (currently MPI_COMM_WORLD).
  MPI_Comm comm() const { return runtime_.comm(); }

  // Exactly one Infra object should exist per run. (A copy would tear down the
  // runtime twice.) Multiple model instances are expected to share this single
  // infrastructure runtime, since AMReX's runtime state (communicator,
  // parameter table, memory arenas) is global.
  Infra(const Infra &) = delete;
  Infra &operator=(const Infra &) = delete;
  Infra(Infra &&) = delete;
  Infra &operator=(Infra &&) = delete;

private:
  /// @brief The TIM infrastructure runtime, in owner mode. Its RAII lifetime
  /// drives MPI + AMReX startup and ordered teardown.
  TIM::Runtime runtime_;
};

} // namespace MOM
