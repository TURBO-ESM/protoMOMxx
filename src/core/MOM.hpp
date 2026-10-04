#pragma once
/// @file MOM.hpp
/// @brief Main header for the Modular Ocean Model (MOM) core.

#include "MOM_domain_infra.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_grid.hpp"
#include "MOM_state.hpp"
#include "MOM_vertical_grid.hpp"

namespace MOM {

/// @brief The Model class is the main interface of the MOM core: it owns the
/// model state and subsystems and provides the entry points the driver calls.
///
/// The constructor performs the full initialization. It is the analogue of
/// MOM6's initialize_MOM (src/core/MOM.F90). The constructor is decomposed
/// into phases that mirror the topology of MOM6's initialize_MOM. The
/// remaining phase, initialize_dynamics, is a stub that will be filled in by
/// upcoming PRs.
class Model {
public:
  /// @brief Scalar configuration switches of the model (the analogue of the
  /// scalar members of MOM6's MOM_control_struct).
  struct Config {
    bool split = true;            ///< Use split time stepping.
    bool split_rk4 = false;       ///< Use the RK4 variant of the split scheme.
    bool use_RK2 = false;         ///< Use RK2 (not RK3) in unsplit stepping.
    bool fpmix = false;           ///< Use the FPMIX algorithm.
    bool debug = false;           ///< Write verbose debugging data.
  };

  /// @brief Initialize the model (the analogue of MOM6's initialize_MOM).
  /// @param params Runtime parameters. Injected by the driver.
  /// @pre The infrastructure layer (AMReX) is initialized.
  explicit Model(RuntimeParams &params);

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

  /// @brief Read the scalar configuration switches into a Config object.
  static Config read_config_switches(RuntimeParams &params);

  /// @brief Initialize the dynamics subsystem for the configured time
  /// stepping scheme. Analogue of MOM6's register_restarts_dyn_* +
  /// initialize_dyn_* four-way dispatch. (stub)
  void initialize_dynamics(RuntimeParams &params);
};

} // namespace MOM
