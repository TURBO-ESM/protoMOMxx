/**
 * @file MOM_driver.cpp
 * @brief Driver program for protoMOMxx
 */

#include "MOM.h"
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

    // Initialize the core MOM object
    const MOM::Model mom;

    return 0;

  } catch (const MOM::logger::FatalError&) {
    return EXIT_FAILURE;
  }
}
