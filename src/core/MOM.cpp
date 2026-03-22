#include "MOM.h"
#include "MOM_logger.h"
#include "MOM_directories.h"

namespace MOM {

Model::Model(const int ensemble_num)
  : ensemble_num_(ensemble_num) {

  logger::info("Initializing protoMOMxx...");
  if (ensemble_num_ >= 0) logger::info("Ensemble number: ", ensemble_num_);

  // Directories container reads the input.nml file to determine where to read/write files.
  Directories directories(ensemble_num_);

  // RuntimeParams reads the parameter files specified by Directories and makes them available for querying.
  params = std::make_shared<RuntimeParams>(
    directories.parameter_filenames(),
    "MOM_parameters_doc"
  );
  params->doc_module("MOM", "Main MOM ocean model module");

  int verbosity = 0;
  params->get("VERBOSITY", verbosity, 
              {.default_value = 0,
               .desc = "Integer controlling level of messaging\n"
                       "\t0 = Only FATAL messages\n"
                       "\t2 = Only FATAL, WARNING, NOTE [default]\n"
                       "\t9 = All",
               .units = "",
               .fail_if_missing = false});

  logger::set_verbosity(static_cast<logger::LogLevel>(verbosity));
  //logger::info("Log verbosity set to ", logger::get_verbosity());

  // todo: Below parameter queries are currently just examples to demonstrate the get() interface and are not used yet.

  bool split = false;
  params->get("SPLIT", split, {.default_value = true, .desc = "Use the split time stepping if true."});

  bool split_rk4 = false;
  params->get("SPLIT_RK4", split_rk4,
              {.default_value = false,
               .desc = "If true, use a version of the split explicit time stepping scheme that "
                       "exchanges velocities with step_MOM that have the average barotropic phase over "
                       "a baroclinic timestep rather than the instantaneous barotropic phase.",
               .do_not_log = !split});

  bool use_RK2 = false;
  if (!split) {
    params->get("USE_RK2", use_RK2,
                {.default_value = false, 
                 .desc = "If true, use RK2 instead of RK3 in the unsplit time stepping."});
  }

  bool fpmix = false;
  params->get("FPMIX", fpmix, 
              {.default_value = false,
               .desc = "If true, use the FPMIX algorithm for tracer advection.",
               .do_not_log = true});
  
  if (fpmix && !split) {
    logger::fatal("FPMIX is only implemented for the split time stepping.");
  }

  int N_SMOOTH = 0;
  params->open_block("KPP", "KPP module parameters");
  params->get("N_SMOOTH", N_SMOOTH,
              {.default_value = 0,
               .desc = "Number of times to apply the smoothing operator to the initial condition",
               .units = "nondim"});
  params->close_block();

  bool debug = false;
  params->get("DEBUG", debug,
              {.default_value = false,
               .desc = "If true, write out verbose debugging data.",
               .units = "nondim",
               .debugging_param = true});

  bool global_indexing = false;
  params->get("GLOBAL_INDEXING", global_indexing,
              {.default_value = false,
               .desc = "If true, use global indexing for all I/O and internal operations. "
                       "If false, use local indexing with halo regions.",
               .layout_param = true});
  
  double rad_earth = 0.;
  params->get("RAD_EARTH", rad_earth,
              {.default_value = 6.378e6,
               .desc = "The radius of the Earth.",
               .units = "m"});

  // todo: set_calendar_type()
  //		Functionality from TIM/time_manager/time_manager.F90
  // defer: time_interp_external_init()
  // 		Initializes the interpolation capability in MOM
  // todo: initialize_MOM()
  //		Performs the core initialization of MOM
  // todo: get_MOM_state_eleemnts()
  // 		Functionality from src/core/MOM.F90
  // defer: initialize_ice_shelf_fluxes()
  // defer: initialize_ice_shelf_forces()	
  // defer: ice_shelf_query()
  // defer: data_override_init()
  // todo: extract_surface_state()
  // todo: surface_forcing_init()
  // 		config_src/drivers/solo_dirver/MOM_surface_forcing.F90
  // defer: MOM_wave_interface_init()
  // 		src/user/MOM_wave_interface.F90
  // todo: real_to_time()
  // defer: diag_mediator_close_registration()
  // defer: write_time_stamp_file()
  // todo: set_forcing()
  // 		MOM_surface_forcing.F90
  // todo: finish_MOM_initilization()
  // todo: step_MOM()
  // 		src/core/MOM.F90
  // defer: mech_forcing_diags()
  // defer: forcing_diagnostics()
  // defer: save_MOM_restart()
  // defer: write_ocean_solo_res()
  // defer: diag_mediator_end()
  // todo: MOM_end()

}

} // namespace MOM
