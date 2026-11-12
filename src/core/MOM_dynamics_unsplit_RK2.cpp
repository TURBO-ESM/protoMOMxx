#include "MOM_dynamics_unsplit_RK2.hpp"

#include "MOM_logger.hpp"

namespace MOM {

DynUnsplitRK2CS initialize_dyn_unsplit_RK2(const Grid &G, const VerticalGrid &GV,
                                           RuntimeParams &params) {

  params.doc_module("MOM_dynamics_unsplit_RK2", "");

  DynUnsplitRK2CS CS;

  params.get("BE", CS.be,
             {.default_value = 0.6,
              .desc = "If SPLIT is true, BE determines the relative weighting "
                      "of a 2nd-order Runga-Kutta baroclinic time stepping "
                      "scheme (0.5) and a backward Euler scheme (1) that is "
                      "used for the Coriolis and inertial terms.  BE may be "
                      "from 0.5 to 1, but instability may occur near 0.5. "
                      "BE is also applicable if SPLIT is false and USE_RK2 "
                      "is true.",
              .units = "nondim"});
  params.get("BEGW", CS.begw,
             {.default_value = 0.0,
              .desc = "If SPLIT is true, BEGW is a number from 0 to 1 that "
                      "controls the extent to which the treatment of gravity "
                      "waves is forward-backward (0) or simulated backward "
                      "Euler (1).  0 is almost always used. "
                      "If SPLIT is false and USE_RK2 is true, BEGW can be "
                      "between 0 and 0.5 to damp gravity waves.",
              .units = "nondim"});

  params.get("UNSPLIT_DT_VISC_BUG", CS.dt_visc_bug,
             {.default_value = false,
              .desc = "If false, use the correct timestep in the viscous terms applied in the first "
                      "predictor step with the unsplit time stepping scheme, and in the calculation "
                      "of the turbulent mixed layer properties for viscosity with unsplit or "
                      "unsplit_RK2.  If true, an older incorrect value is used."});

  params.get("DEBUG", CS.debug,
             {.default_value = false,
              .desc = "If true, write out verbose debugging data.",
              .debugging_param = true});
  if (CS.debug) {
    // defer: the DEBUG checksums of the time step.
    logger::fatal("initialize_dyn_unsplit_RK2: The debugging checksums (DEBUG = True) "
                  "are not implemented yet.");
  }
  params.get("TIDES", CS.use_tides,
             {.default_value = false,
              .desc = "If true, apply tidal momentum forcing."});
  params.get("CALCULATE_SAL", CS.calculate_SAL,
             {.default_value = CS.use_tides,
              .desc = "If true, calculate self-attraction and loading."});

  CS.continuity = continuity_init(G, GV, params);
  // todo: CoriolisAdv_init
  if (CS.calculate_SAL) {
    // defer: SAL_init
    logger::fatal("initialize_dyn_unsplit_RK2: Self-attraction and loading "
                  "(CALCULATE_SAL = True) is not implemented yet.");
  }
  if (CS.use_tides) {
    // defer: tidal_forcing_init
    logger::fatal("initialize_dyn_unsplit_RK2: Tidal forcing (TIDES = True) "
                  "is not implemented yet.");
  }
  // todo: PressureForce_init, hor_visc_init and vertvisc_init

  return CS;
}

} // namespace MOM
