#pragma once
/// @file MOM_dynamics_unsplit_RK2.hpp
/// @brief The unsplit RK2 time stepper: its options and the setup of the
///        modules it calls. The analogue of MOM6's MOM_dynamics_unsplit_RK2.

#include <AMReX_REAL.H>

#include "MOM_continuity.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_grid.hpp"
#include "MOM_vertical_grid.hpp"

namespace MOM {

/// @brief The unsplit RK2 time stepper's options, and the options of the
/// modules it calls. The analogue of MOM6's MOM_dyn_unsplit_RK2_CS.
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

  ContinuityCS continuity;     ///< The continuity solver's options.
  // todo: the options of CoriolisAdv, PressureForce, hor_visc and vertvisc,
  //       which come with their callers.
};

/// @brief Read the unsplit RK2 time stepper's options and set up the modules
/// it calls. The analogue of MOM6's initialize_dyn_unsplit_RK2.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param params Runtime parameters.
/// @return The time stepper's options.
/// @throws logger::FatalError if DEBUG, CALCULATE_SAL or TIDES is true,
///         which are not implemented yet.
DynUnsplitRK2CS initialize_dyn_unsplit_RK2(const Grid &G, const VerticalGrid &GV,
                                           RuntimeParams &params);

} // namespace MOM
