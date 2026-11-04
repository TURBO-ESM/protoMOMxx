#include "MOM.hpp"

#include <cmath>

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
    state_(make_state(domain_, grid_, vgrid_, params)) {

  initialize_dynamics();

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
      // todo: step_MOM_dyn_unsplit_RK2, which advances the state over dt
      //       with the forces.
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

  // MOM6 reads CALC_RHO_FOR_SEA_LEVEL before ENABLE_THERMODYNAMICS, for the
  // sea level of the surface state, which is not implemented yet.

  bool use_EOS = use_temperature;
  params.get("USE_EOS", use_EOS,
             {.default_value = use_temperature,
              .desc = "If true,  density is calculated from temperature and salinity with an "
                      "equation of state.  If USE_EOS is true, ENABLE_THERMODYNAMICS must be true "
                      "as well."});
  if (use_EOS) {
    // defer: the equation of state.
    logger::fatal("MOM: The equation of state (USE_EOS = True) is not implemented yet.");
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

  // MOM6 reads USE_CONTEMP_ABSSAL here, and ADVECT_TS after DO_DYNAMICS. Both
  // matter only with temperature and salinity.

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

  bool do_dynamics = true;
  params.get("DO_DYNAMICS", do_dynamics,
             {.default_value = true,
              .desc = "If False, skips the dynamics calls that update u & v, as well as the "
                      "gravity wave adjustment to h. This may be a fragile feature, but can be "
                      "useful during development"});
  if (!do_dynamics) {
    logger::fatal("MOM: Skipping the dynamics (DO_DYNAMICS = False) is not implemented yet.");
  }

  // MOM6 reads OFFLINE_TRACER_MODE only when the driver supports it, as the
  // solo driver does.
  bool offline_tracer_mode = false;
  params.get("OFFLINE_TRACER_MODE", offline_tracer_mode,
             {.default_value = false,
              .desc = "If true, barotropic and baroclinic dynamics, thermodynamics are all "
                      "bypassed with all the fields necessary to integrate the tracer advection "
                      "and diffusion equation are read in from files stored from a previous "
                      "integration of the prognostic model. NOTE: This option only used in the "
                      "ocean_solo_driver."});
  if (offline_tracer_mode) {
    // defer: the offline tracer mode (MOM_offline_main).
    logger::fatal("MOM: The offline tracer mode (OFFLINE_TRACER_MODE = True) is not "
                  "implemented yet.");
  }

  bool use_ALE_algorithm = false;
  params.get("USE_REGRIDDING", use_ALE_algorithm,
             {.default_value = false,
              .desc = "If True, use the ALE algorithm (regridding/remapping). If False, use the "
                      "layered isopycnal algorithm."});
  if (use_ALE_algorithm) {
    // defer: ALE (MOM_ALE), with REMAP_UV_USING_OLD_ALG and REMAP_AUXILIARY_VARS.
    logger::fatal("MOM: The ALE algorithm (USE_REGRIDDING = True) is not implemented yet.");
  }

  bool bulkmixedlayer = use_temperature && !use_ALE_algorithm;
  params.get("BULKMIXEDLAYER", bulkmixedlayer,
             {.default_value = use_temperature && !use_ALE_algorithm,
              .desc = "If true, use a Kraus-Turner-like bulk mixed layer with transitional buffer "
                      "layers.  Layers 1 through NKML+NKBL have variable densities. There must be "
                      "at least NKML+NKBL+1 layers if BULKMIXEDLAYER is true. BULKMIXEDLAYER can "
                      "not be used with USE_REGRIDDING. The default is influenced by "
                      "ENABLE_THERMODYNAMICS."});
  if (bulkmixedlayer) {
    // defer: the bulk mixed layer, with NKML and NKBL.
    logger::fatal("MOM: The bulk mixed layer (BULKMIXEDLAYER = True) is not implemented yet.");
  }

  bool use_porbar = false;
  params.get("USE_POROUS_BARRIER", use_porbar,
             {.default_value = false,
              .desc = "If true, use porous barrier to constrain the widths and face areas at the "
                      "edges of the grid cells. "});
  if (use_porbar) {
    // defer: the porous barriers (MOM_porous_barriers).
    logger::fatal("MOM: The porous barriers (USE_POROUS_BARRIER = True) are not implemented yet.");
  }

  bool bathymetry_at_vel = false;
  params.get("BATHYMETRY_AT_VEL", bathymetry_at_vel,
             {.default_value = false,
              .desc = "If true, there are separate values for the basin depths at velocity "
                      "points.  Otherwise the effects of topography are entirely determined from "
                      "thickness points."});
  if (bathymetry_at_vel) {
    // defer: the depths at velocity points.
    logger::fatal("MOM: Separate depths at velocity points (BATHYMETRY_AT_VEL = True) are not "
                  "implemented yet.");
  }

  params.get("DEBUG", config.debug,
             {.default_value = false,
              .desc = "If true, write out verbose debugging data.",
              .units = "nondim",
              .debugging_param = true});

  // MOM6 reads ENABLE_BUGS_BY_DEFAULT here, which sets the defaults of the
  // bug-fix flags. No bug-fix flag is read yet.

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

  // MOM6 reads THERMO_SPANS_COUPLING and DT_TRACER_ADVECT next, for the
  // thermodynamic and tracer advection cadences.

  return config;
}

void Model::initialize_dynamics() {

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
    // todo: initialize_dyn_unsplit_RK2
    logger::note("initialize_dynamics: The unsplit RK2 dynamics is not implemented yet, "
                 "so the model state does not change in time.");
  } else {
    // defer: initialize_dyn_unsplit
    logger::fatal("initialize_dynamics: The unsplit RK3 time stepping (USE_RK2 = False) "
                  "is not implemented yet.");
  }
}

} // namespace MOM
