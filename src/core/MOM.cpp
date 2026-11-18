#include <cmath>

#include "MOM.hpp"
#include "MOM_domains.hpp"
#include "MOM_fixed_initialization.hpp"
#include "MOM_logger.hpp"
#include "MOM_state_initialization.hpp"

namespace MOM {

Model::Model(RuntimeParams &params)
  : config_(read_config_switches(params)),
    domain_(make_domain(params)),
    grid_(make_grid(domain_, params)),
    vgrid_(params),
    state_(make_state(domain_, grid_, vgrid_, params)),
    pbv_(make_porous_barrier(domain_, vgrid_.nk())) {

  initialize_dynamics(params);

  logger::note("MOM core initialization complete.");
}

void Model::step(const MechForcing &forces, const amrex::Real time_interval) {

  // defer: the parts of step_MOM around the dynamics: the DEBUG checksums, the
  //        halo update of the forcing, the thermodynamic, tracer advection and
  //        bottom boundary layer cadences (DT_THERM, ...), and the diagnostics.

  // The number of dynamics steps in this call, and their length [T ~> s].
  int n_max = 1;
  if (time_interval > config_.dt) {
    n_max = static_cast<int>(std::ceil(time_interval / config_.dt - 0.001));
  }
  const amrex::Real dt = time_interval / amrex::Real(n_max);

  for (int n = 1; n <= n_max; ++n) {
    if (config_.split && config_.use_alt_split) {
      // defer: step_MOM_dyn_split_RK2b
      logger::fatal("step: The split RK2b time stepping (SPLIT_RK2B = True) "
                    "is not implemented yet.");
    } else if (config_.split) {
      // defer: step_MOM_dyn_split_RK2
      logger::fatal("step: The split time stepping (SPLIT = True) is not implemented yet.");
    } else if (config_.use_RK2) {
      dyn_unsplit_RK2_->step(state_, dt, forces, config_.first_direction % 2 == 0);
    } else {
      // defer: step_MOM_dyn_unsplit
      logger::fatal("step: The unsplit RK3 time stepping (USE_RK2 = False) "
                    "is not implemented yet.");
    }
  }
}

Model::Config Model::read_config_switches(RuntimeParams &params) {

  logger::info("Initializing the MOM core...");

  params.doc_module("MOM", "Main MOM ocean model module"); // set current param module for documentation purposes

  int verbosity = 2;
  params.get("VERBOSITY", verbosity,
             {.default_value = 2,
              .desc = "Integer controlling level of messaging\n"
                      "\t0 = Only FATAL messages\n"
                      "\t2 = Only FATAL, WARNING, NOTE [default]\n"
                      "\t9 = All",
              .units = "",
              .fail_if_missing = false});

  logger::set_verbosity(verbosity);
  logger::info("Log verbosity: ", logger::get_verbosity());

  Config config;

  params.get("SPLIT", config.split, {.default_value = true, .desc = "Use the split time stepping if true."});

  params.get("SPLIT_RK2B", config.use_alt_split,
             {.default_value = false,
              .desc = "If true, use a version of the split explicit time stepping scheme that "
                      "exchanges velocities with step_MOM that have the average barotropic phase over "
                      "a baroclinic timestep rather than the instantaneous barotropic phase.",
              .do_not_log = !config.split});

  if (!config.split) {
    params.get("USE_RK2", config.use_RK2,
               {.default_value = false,
                .desc = "If true, use RK2 instead of RK3 in the unsplit time stepping."});
  }

  params.get("FPMIX", config.fpmix,
             {.default_value = false,
              .desc = "If true, use the FPMIX algorithm for tracer advection.",
              .do_not_log = true});

  if (config.fpmix && !config.split) {
    logger::fatal("FPMIX is only implemented for the split time stepping.");
  }

  bool use_temperature = true;
  params.get("ENABLE_THERMODYNAMICS", use_temperature,
             {.default_value = true,
              .desc = "If true, Temperature and salinity are used as state variables."});
  if (use_temperature) {
    // defer: the temperature and salinity state (the tv structure) and the
    //        equation of state.
    logger::fatal("MOM: Temperature and salinity as state variables "
                  "(ENABLE_THERMODYNAMICS = True) are not implemented yet.");
  }

  bool diabatic_first = false;
  params.get("DIABATIC_FIRST", diabatic_first,
             {.default_value = false,
              .desc = "If true, apply diabatic and thermodynamic processes, including buoyancy "
                      "forcing and mass gain or loss, before stepping the dynamics forward."});
  if (diabatic_first) {
    // defer: the thermodynamic step of step_MOM (step_MOM_thermo).
    logger::fatal("MOM: Applying the diabatic processes before the dynamics "
                  "(DIABATIC_FIRST = True) is not implemented yet.");
  }

  bool adiabatic = false;
  params.get("ADIABATIC", adiabatic,
             {.default_value = false,
              .desc = "There are no diapycnal mass fluxes if ADIABATIC is true.  This assumes "
                      "that KD = 0.0 and that there is no buoyancy forcing, but makes the model "
                      "faster by eliminating subroutine calls."});
  if (!adiabatic) {
    // defer: the diabatic processes (MOM_diabatic_driver) and the buoyancy forcing.
    logger::fatal("MOM: The diapycnal mass fluxes (ADIABATIC = False) are not implemented yet.");
  }

  bool use_porbar = false;
  params.get("USE_POROUS_BARRIER", use_porbar,
             {.default_value = false,
              .desc = "If true, use porous barrier to constrain the widths "
                      "and face areas at the edges of the grid cells."});
  if (use_porbar) {
    // defer: porous_barriers_init and the update of the open fractions in
    //        each step (porous_widths_layer).
    logger::fatal("MOM: Porous barriers (USE_POROUS_BARRIER = True) are not implemented yet.");
  }

  params.get("DEBUG", config.debug,
             {.default_value = false,
              .desc = "If true, write out verbose debugging data.",
              .debugging_param = true});

  params.get("DT", config.dt,
             {.desc = "The (baroclinic) dynamics time step.  The time-step that "
                      "is actually used will be an integer fraction of the "
                      "forcing time-step (DT_FORCING in ocean-only mode or the "
                      "coupling timestep in coupled mode.)",
              .units = "s",
              .fail_if_missing = true});

  // Read so that it is logged. MOM6 uses DT_THERM for the thermodynamic and
  // bottom boundary layer cadences of step_MOM, which are not implemented yet.
  amrex::Real dt_therm = config.dt;
  params.get("DT_THERM", dt_therm,
             {.default_value = config.dt,
              .desc = "The thermodynamic time step. Ideally DT_THERM should be an integer "
                      "multiple of DT and of DT_TRACER_ADVECT and less than the forcing or "
                      "coupling time-step. However, if THERMO_SPANS_COUPLING is true, DT_THERM "
                      "can be an integer multiple of the coupling timestep. By default DT_THERM "
                      "is set to DT.",
              .units = "s"});

  params.get("FIRST_DIRECTION", config.first_direction,
             {.default_value = 0,
              .desc = "An integer that indicates which direction goes first "
                      "in parts of the code that use directionally split "
                      "updates, with even numbers (or 0) used for x- first "
                      "and odd numbers used for y-first."});
  bool alternate_first_direction = false;
  params.get("ALTERNATE_FIRST_DIRECTION", alternate_first_direction,
             {.default_value = false,
              .desc = "If true, after every dynamic timestep alternate whether the x- or y- "
                      "direction updates occur first in directionally split parts of the calculation. "
                      "If this is true, FIRST_DIRECTION applies at the start of a new run or if "
                      "the next first direction can not be found in the restart file."});
  if (alternate_first_direction) {
    // defer: the switch of first_direction after each dynamics step.
    logger::fatal("MOM: Alternating the first direction (ALTERNATE_FIRST_DIRECTION = True) "
                  "is not implemented yet.");
  }

  return config;
}

void Model::initialize_dynamics(RuntimeParams &params) {

  // defer: restart registration (register_restarts_dyn_*) and diagnostics.
  if (config_.split && config_.use_alt_split) {
    // defer: initialize_dyn_split_RK2b
    logger::fatal("initialize_dynamics: The split RK2b time stepping (SPLIT_RK2B = True) "
                  "is not implemented yet.");
  } else if (config_.split) {
    // defer: initialize_dyn_split_RK2
    logger::fatal("initialize_dynamics: The split time stepping (SPLIT = True) "
                  "is not implemented yet.");
  } else if (config_.use_RK2) {
    dyn_unsplit_RK2_.emplace(make_dyn_unsplit_RK2(domain_, grid_, vgrid_, pbv_, params));
    logger::note("initialize_dynamics: The Coriolis, pressure, viscosity and vertical "
                 "friction terms of the unsplit RK2 step are stubs that add nothing yet, "
                 "so a state at rest stays at rest.");
  } else {
    // defer: initialize_dyn_unsplit
    logger::fatal("initialize_dynamics: The unsplit RK3 time stepping (USE_RK2 = False) "
                  "is not implemented yet.");
  }
}

} // namespace MOM
