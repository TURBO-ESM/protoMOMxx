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

// todo: once a second time stepping scheme arrives (e.g., unsplit RK3), share
//       what the schemes have in common (the modules they call) instead of
//       repeating it in each scheme.

/// @brief The unsplit RK2 time stepper: its options, the modules it calls, and
/// the domain, grids and open face fractions it reads. make_dyn_unsplit_RK2
/// creates it, and step() advances the state by one time step. The analogue
/// of MOM6's MOM_dyn_unsplit_RK2_CS and the routines that take it.
///
/// The Coriolis, pressure, horizontal viscosity and vertical friction terms
/// are stubs for now. They add nothing, so a state at rest stays at rest.
///
/// It refers to the domain, the grids and the open face fractions, so they
/// must outlive it.
struct DynUnsplitRK2 {
  const Domain &domain;      ///< The domain, for the halo exchanges and the work fields.
  const Grid &G;             ///< The horizontal grid.
  const VerticalGrid &GV;    ///< The vertical grid.
  const PorousBarrier &pbv;  ///< The open fractions of the cell faces.

  /// A number from 0.5 to 1 that sets the backward weighting of the time
  /// stepping [nondim].
  amrex::Real be = 0.0;
  /// A number from 0 to 1 that sets how far the gravity waves are treated
  /// forward-backward (0) or as simulated backward Euler (1) [nondim].
  amrex::Real begw = 0.0;
  /// If true, use an older, incorrect time step in the viscous terms.
  bool dt_visc_bug = false;

  Continuity continuity;  ///< The continuity solver.
  // todo: the CoriolisAdv, PressureForce, hor_visc and vertvisc modules,
  //       which come with their callers.

  /// @brief Advance u, v and h by one time step. The analogue of MOM6's
  /// step_MOM_dyn_unsplit_RK2.
  /// @param state The state. The step advances u, v and h, sets uh and vh to
  ///        the volume fluxes of the step, and adds dt*uh and dt*vh to uhtr, vhtr.
  /// @param dt The time step [T ~> s].
  /// @param forces The mechanical forcing.
  /// @param x_first If true, the continuity solver advects zonally first.
  void step(State &state, amrex::Real dt, const MechForcing &forces, bool x_first) const;
};

/// @brief Read the unsplit RK2 time stepper's options and set up the modules
/// it calls. The analogue of MOM6's initialize_dyn_unsplit_RK2.
/// @param domain The domain, for the halo exchanges and the work fields.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param pbv The open fractions of the cell faces.
/// @param params Runtime parameters.
/// @return The time stepper, referring to domain, G, GV and pbv.
/// @throws logger::FatalError if DEBUG, CALCULATE_SAL or TIDES is true,
///         which are not implemented yet.
DynUnsplitRK2 make_dyn_unsplit_RK2(const Domain &domain, const Grid &G, const VerticalGrid &GV,
                                   const PorousBarrier &pbv, RuntimeParams &params);

} // namespace MOM
