/**
 * @file MOM_driver.cpp
 * @brief Driver program for protoMOMxx
 */

#include <cstdlib>
#include <exception>
#include <iostream>

#include "MOM.h"
#include "MOM_directories.h"
#include "MOM_infra.h"
#include "MOM_logger.h"

/// @brief Main entry point for the protoMOMxx driver program.
/// @param argc Number of arguments including binary name.
/// @param argv Parameter command line as array pointers.
/// @return Exit code (0 for success, non-zero for failure)
int main(int argc, char* argv[]) {
  try {

    MOM::logger::info("Hello C++ world. This is protoMOMxx!");

    // Initialize the infrastructure layer (AMReX).
    // (It is finalized automatically when this scope exits.)
    const MOM::Infra infra(argc, argv);

    // todo: ensemble manager (from the infrastructure layer.)
    const int ensemble_num = -1;

    // Read input.nml to determine input/output directories and param file names
    const MOM::Directories directories(ensemble_num);

    // RuntimeParams reads the parameter files specified in input.nml.
    MOM::RuntimeParams params(directories.parameter_filenames(), "MOM_parameters_doc");

    // todo: set_calendar_type() -- time manager (Clock) comes with the time
    //       loop.
    // defer: time_interp_external_init()

    // Initialize the core MOM object (the analogue of MOM6's initialize_MOM).
    const MOM::Model model(params);

    // todo: extract_surface_state()
    // todo: surface_forcing_init()
    // defer: MOM_wave_interface_init(), data_override_init(), ice shelf hooks
    // todo: read run-control params (DT, DT_FORCING, DAYMAX, ...) and run the
    //       time loop:
    //         while (clock.time() < clock.end()) {
    //           set_forcing(...); model.step(...); clock.advance();
    //         }
    // todo: finish_MOM_initialization() on the first iteration
    // defer: mech_forcing_diags(), forcing_diagnostics()
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
