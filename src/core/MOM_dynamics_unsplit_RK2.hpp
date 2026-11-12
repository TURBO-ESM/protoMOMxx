#pragma once
/// @file MOM_dynamics_unsplit_RK2.hpp
/// @brief The unsplit RK2 time stepper: its options, the setup of the modules
///        it calls, and the time step. The analogue of MOM6's
///        MOM_dynamics_unsplit_RK2.

#include <AMReX_MultiFab.H>
#include <AMReX_REAL.H>

#include "MOM_continuity.hpp"
#include "MOM_domain_infra.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_forcing_type.hpp"
#include "MOM_grid.hpp"
#include "MOM_porous_barrier.hpp"
#include "MOM_state.hpp"
#include "MOM_vertical_grid.hpp"

namespace MOM {

// todo: once a second time stepping scheme arrives (e.g., unsplit RK3), move
//       the members that the schemes share (the module options, the
//       accelerations, and the debug, dt_visc_bug, calculate_SAL and
//       use_tides flags) into a common struct.

/// @brief The unsplit RK2 time stepper's options, the options of the modules
/// it calls, and the accelerations it keeps between its stages. The analogue
/// of MOM6's MOM_dyn_unsplit_RK2_CS.
struct DynUnsplitRK2CS {
  /// A number from 0.5 to 1 that sets the backward weighting of the time
  /// stepping [nondim].
  amrex::Real be = 0.0;
  /// A number from 0 to 1 that sets how far the gravity waves are treated
  /// forward-backward (0) or as simulated backward Euler (1) [nondim].
  amrex::Real begw = 0.0;
  /// If true, use an older, incorrect time step in the viscous terms.
  bool dt_visc_bug = false;
  bool debug = false;          ///< If true, write verbose checksums.
  bool calculate_SAL = false;  ///< If true, calculate self-attraction and loading.
  bool use_tides = false;      ///< If true, apply tidal forcing.

  amrex::MultiFab CAu;    ///< The zonal Coriolis and advective acceleration [L T-2 ~> m s-2].
  amrex::MultiFab CAv;    ///< The meridional Coriolis and advective acceleration [L T-2 ~> m s-2].
  amrex::MultiFab PFu;    ///< The zonal pressure force acceleration [L T-2 ~> m s-2].
  amrex::MultiFab PFv;    ///< The meridional pressure force acceleration [L T-2 ~> m s-2].
  amrex::MultiFab diffu;  ///< The zonal acceleration from horizontal viscosity [L T-2 ~> m s-2].
  amrex::MultiFab diffv;  ///< The meridional acceleration from horizontal viscosity [L T-2 ~> m s-2].

  ContinuityCS continuity;     ///< The continuity solver's options.
  // todo: the options of CoriolisAdv, PressureForce, hor_visc and vertvisc,
  //       which come with their callers.
};

/// @brief Read the unsplit RK2 time stepper's options and set up the modules
/// it calls. The analogue of MOM6's initialize_dyn_unsplit_RK2.
/// @param domain The domain the accelerations are created on.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param params Runtime parameters.
/// @return The time stepper's options, with the accelerations set to zero.
/// @throws logger::FatalError if DEBUG, CALCULATE_SAL or TIDES is true,
///         which are not implemented yet.
DynUnsplitRK2CS initialize_dyn_unsplit_RK2(const Domain &domain, const Grid &G,
                                           const VerticalGrid &GV, RuntimeParams &params);

/// @brief Advance u, v and h by one time step with the unsplit RK2 scheme.
/// The analogue of MOM6's step_MOM_dyn_unsplit_RK2.
///
/// The Coriolis, pressure, horizontal viscosity and vertical friction terms
/// are stubs for now. They add nothing, so a state at rest stays at rest.
/// @param state The state. The step advances u, v and h, sets uh and vh to
///        the volume fluxes of the step, and adds dt*uh and dt*vh to uhtr, vhtr.
/// @param dt The time step [T ~> s].
/// @param forces The mechanical forcing.
/// @param domain The domain, for the halo exchanges.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param CS The time stepper's options and accelerations.
/// @param pbv The open fractions of the cell faces.
/// @param x_first If true, the continuity solver advects zonally first.
void step_MOM_dyn_unsplit_RK2(State &state, amrex::Real dt, const MechForcing &forces,
                              const Domain &domain, const Grid &G, const VerticalGrid &GV,
                              DynUnsplitRK2CS &CS, const PorousBarrier &pbv, bool x_first);

} // namespace MOM
