/**
 * @file MOM_driver.cpp
 * @brief Driver program for protoMOMxx
 */

#include <cstdlib>
#include <exception>
#include <iostream>

#include "MOM.hpp"
#include "MOM_directories.hpp"
#include "MOM_infra.hpp"
#include "MOM_logger.hpp"
#include "MOM_run_control.hpp"
#include "MOM_time_manager.hpp"

/// @brief Main entry point for the protoMOMxx driver program.
/// @param argc Number of arguments including binary name.
/// @param argv Parameter command line as array pointers.
/// @return Exit code (0 for success, non-zero for failure)
int main(int argc, char* argv[]) {
  try {

    MOM::logger::info("Hello C++ world. This is protoMOMxx!");

    // Initialize the infrastructure layer via MOM::Infra, a wrapper
    // around TIM::Runtime. This initializes MPI and AMReX, which are
    // finalized automatically when this scope exits.
    const MOM::Infra infra(argc, argv);

    // todo: ensemble manager (from the infrastructure layer.)
    const int ensemble_num = -1;

    // Read input.nml to determine input/output directories and param file names
    const MOM::Directories directories(ensemble_num);

    // RuntimeParams reads the parameter files specified in input.nml.
    MOM::RuntimeParams params(directories.parameter_filenames(), "MOM_parameters_doc");

    // The calendar and the start time of the run, from ocean_solo_nml.
    const MOM::RunStart start = MOM::read_ocean_solo_nml("input.nml", directories.restart_input_dir());
    // defer: time_interp_external_init()

    // Initialize the core MOM object (the analogue of MOM6's initialize_MOM).
    const MOM::Model model(params);

    // todo: extract_surface_state()
    // todo: surface_forcing_init()
    // defer: MOM_wave_interface_init(), data_override_init(), ice shelf hooks

    // The run-control parameters: the forcing time step and the end time.
    const MOM::RunControl run = MOM::read_run_control(params, start.start_time);
    // todo: close_param_file() (the unused-parameter check)
    // defer: write_time_stamp_file(), MOM_write_cputime_init(), the restart schedule

    // The main loop, one forcing time step per pass.
    MOM::Time time = start.start_time;
    while (time < run.time_end) {
      // todo: set_forcing()
      // todo: finish_MOM_initialization() on the first iteration
      // todo: step_MOM()
      time += run.time_step_ocean;
      // defer: mech_forcing_diags(), forcing_diagnostics(), incremental restarts
    }

    // defer: save_MOM_restart(), write_ocean_solo_res(), diag_mediator_end()
    // todo: MOM_end()

    return 0;

  } catch (const MOM::logger::FatalError&) {
    // Already logged by logger::fatal.
    return EXIT_FAILURE;
  } catch (const std::exception& e) {
    std::cerr << "protoMOMxx terminated with an unhandled exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
