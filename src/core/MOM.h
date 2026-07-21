#pragma once
/// @file MOM.h
/// @brief Main header for the Modular Ocean Model (MOM) core.

#include <AMReX.H>
#include <AMReX_MultiFab.H>

#include "MOM_file_parser.h"

namespace MOM {

/// @brief Scales input to be within necessary bounds
/// @param x Global index
/// @param x_min x min
/// @param x_max x max
/// @param xi_min grid x min
/// @param xi_max grid x max
/// @return Index within scaled space for local grid
AMREX_GPU_DEVICE AMREX_FORCE_INLINE
amrex::Real LinearMapCoordinates(const amrex::Real x,
                                 const amrex::Real x_min, const amrex::Real x_max,
                                 const amrex::Real xi_min, const amrex::Real xi_max);

/// @brief The Model class is the main interface of the MOM core: it owns the
/// model state and subsystems and provides the entry points the driver calls.
///
/// The constructor performs the full initialization. It is the analogue of
/// MOM6's initialize_MOM (src/core/MOM.F90). The constructor is decomposed
/// into phases that mirror the topology of MOM6's initialize_MOM where each
/// phase is currently a stub that will be filled in by upcoming PRs (Domain,
/// HorGrid, VerticalGrid, State, Dynamics). The initialize_state stub runs
/// the original psi (stream function) demo, which exercises the AMReX machinery
/// its real replacement will use.
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
    bool global_indexing = false; ///< Use global indexing (see MOM_domains).
  };

  /// @brief Initialize the model (the analogue of MOM6's initialize_MOM).
  /// @param params Runtime parameters. Injected by the driver.
  /// @pre The infrastructure layer (AMReX) is initialized.
  explicit Model(RuntimeParams &params);

  /// @brief Read-only access to the model configuration switches.
  /// @return Const reference to the model configuration switches.
  const Config &config() const { return config_; }

  /// @brief Initializes AMReX multifab data with static values bounded by geometry object
  /// @param geom Bounds for variable initialization
  /// @param psi Container of fields (variables).
  void InitializeVariables(const amrex::Geometry & geom,
                         amrex::MultiFab & psi);

private:
  Config config_;

  // tmp: global grid extents, kept as scalar members until the Domain and
  // VerticalGrid classes take ownership of them.
  int ni_global_ = 0;
  int nj_global_ = 0;
  int nk_ = 0;

  /// @brief Read the scalar configuration switches into a Config object.
  static Config read_config_switches(RuntimeParams &params);

  /// @brief Initialize the time-invariant setup: domain decomposition,
  /// horizontal grid metrics, topography, masks, and rotation.
  /// Analogue of MOM6's MOM_domains_init + MOM_grid_init +
  /// MOM_initialize_fixed. (stub)
  void initialize_fixed(RuntimeParams &params);

  /// @brief Initialize the vertical grid and coordinate: nk, reduced
  /// gravities, target densities. Analogue of MOM6's verticalGridInit +
  /// MOM_initialize_coord. (stub)
  void initialize_vertical(RuntimeParams &params);

  /// @brief Initialize the prognostic state (u, v, h, ...). Analogue of
  /// MOM6's MOM_initialize_state. (stub -- currently runs the original psi
  /// demo to exercise the AMReX machinery.)
  void initialize_state(RuntimeParams &params);

  /// @brief Initialize the dynamics subsystem for the configured time
  /// stepping scheme. Analogue of MOM6's register_restarts_dyn_* +
  /// initialize_dyn_* four-way dispatch. (stub)
  void initialize_dynamics(RuntimeParams &params);

  void DefineCellCenteredMultiFab(const int ni_global, const int nj_global, const int nk,
                                const int max_chunk_size,
                                amrex::MultiFab & cell_centered_MultiFab);
  void InitializeGeometry(const int ni_global, const int nj_global, const int nk,
                        const amrex::Real dx, const amrex::Real dy, const amrex::Real dz,
                        amrex::Geometry & geom);
};

} // namespace MOM
