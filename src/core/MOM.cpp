#include "MOM.h"
#include "MOM_logger.h"
#include "MOM_directories.h"

namespace MOM {

Model::Model(const int ensemble_num)
  : ensemble_num_(ensemble_num) {

  logger::info("Initializing protoMOMxx...");

  // Initialize the directories container, which will read the namelist and determine the paths to use for input and output.
  Directories directories(ensemble_num_);

  // todo: read VERBOSITY param instead of hardcoding it below
  logger::set_verbosity(logger::LogLevel::DEBUG);

  if (ensemble_num_ >= 0) {
    logger::info("Ensemble number: ", ensemble_num_);
  }

  // todo: Read namelist file
  //
  // todo: get_MOM_input():
  //          	Gets the names of the I/O directories and initialization files
  //          	Also calls the subroutine that opens run-time parameter files
  //          	Needs namelist file capability
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
