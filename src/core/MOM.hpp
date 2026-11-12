#pragma once
/// @file MOM.hpp
/// @brief Main header for the Modular Ocean Model (MOM) core.

#include <optional>

#include <AMReX_REAL.H>

#include "MOM_domain_infra.hpp"
#include "MOM_dynamics_unsplit_RK2.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_forcing_type.hpp"
#include "MOM_grid.hpp"
#include "MOM_porous_barrier.hpp"
#include "MOM_state.hpp"
#include "MOM_vertical_grid.hpp"

namespace MOM {

/// @brief The Model class is the main interface of the MOM core: it owns the
/// model state and subsystems and provides the entry points the driver calls.
///
/// The constructor performs the full initialization. It is the analogue of
/// MOM6's initialize_MOM (src/core/MOM.F90). The constructor is decomposed
/// into phases that mirror the topology of MOM6's initialize_MOM.
class Model {
public:
  /// @brief Scalar configuration switches of the model (the analogue of the
  /// scalar members of MOM6's MOM_control_struct).
  struct Config {
    bool split = true;            ///< Use split time stepping.
    bool use_alt_split = false;   ///< Use the split_RK2b variant of the split scheme.
    bool use_RK2 = false;         ///< Use RK2 (not RK3) in unsplit stepping.
    bool fpmix = false;           ///< Use the FPMIX algorithm.
    bool debug = false;           ///< Write verbose debugging data.
    amrex::Real dt = 0.0;         ///< The baroclinic dynamics time step [T ~> s].
    int first_direction = 0;      ///< Which direction goes first in directionally
                                  ///< split updates: x if even, y if odd. MOM6
                                  ///< keeps it in G%first_direction.
  };

  /// @brief Initialize the model (the analogue of MOM6's initialize_MOM).
  /// @param params Runtime parameters. Injected by the driver.
  /// @pre The infrastructure layer (AMReX) is initialized.
  explicit Model(RuntimeParams &params);

  /// @brief Advance the model over a time interval (the analogue of MOM6's
  /// step_MOM). As in MOM6, the interval is split into equal dynamics steps
  /// no longer than DT, and each step dispatches on the time stepping scheme.
  /// The force terms of the unsplit RK2 step are stubs for now, so a state at
  /// rest stays at rest.
  /// @param forces The mechanical forcing over the interval.
  /// @param time_interval The time interval covered by this call [T ~> s].
  /// @throws logger::FatalError for currently unsupported schemes.
  void step(const MechForcing &forces, amrex::Real time_interval);

  /// @brief Read-only access to the model configuration switches.
  /// @return Const reference to the model configuration switches.
  const Config &config() const { return config_; }

  /// @brief Read-only access to the computational domain.
  /// @return Const reference to the model's domain.
  const Domain &domain() const { return domain_; }

  /// @brief Read-only access to the horizontal grid.
  /// @return Const reference to the model's horizontal grid.
  const Grid &grid() const { return grid_; }

  /// @brief Read-only access to the vertical grid.
  /// @return Const reference to the model's vertical grid.
  const VerticalGrid &vertical_grid() const { return vgrid_; }

  /// @brief Read-only access to the prognostic state.
  /// @return Const reference to the model's prognostic state.
  const State &state() const { return state_; }

private:
  // config_ initialization must precede domain_: its initializer sets the log 
  // verbosity in effect for the later initializers' messages.
  Config config_;

  /// @brief The computational domain: global extents, connectivity, halo
  /// metadata, and the horizontal decomposition.
  Domain domain_;

  /// @brief The horizontal grid: metric fields at the h/q/u/v points and the
  /// Coriolis parameter, on domain_'s decomposition.
  Grid grid_;

  /// @brief The vertical grid and its coordinate: the layer count, the reduced
  /// gravities across the interfaces, and the layer target densities.
  VerticalGrid vgrid_;

  /// @brief The prognostic state: the velocities and the layer thicknesses,
  /// on domain_'s decomposition with vgrid_'s layers.
  State state_;

  /// @brief The open fractions of the cell faces, 1 everywhere without
  /// porous barriers.
  PorousBarrier pbv_;

  /// @brief The unsplit RK2 time stepper's options. Set only when SPLIT is
  /// false and USE_RK2 is true.
  std::optional<DynUnsplitRK2CS> dyn_unsplit_RK2_CS_;

  /// @brief Read the scalar configuration switches into a Config object.
  static Config read_config_switches(RuntimeParams &params);

  /// @brief Initialize the dynamics for the configured time stepping scheme,
  /// with MOM6's four-way dispatch of register_restarts_dyn_* and
  /// initialize_dyn_* (split_RK2b, split_RK2, unsplit_RK2, unsplit).
  /// @param params Runtime parameters.
  /// @throws logger::FatalError for currently unsupported schemes.
  void initialize_dynamics(RuntimeParams &params);
};

} // namespace MOM
