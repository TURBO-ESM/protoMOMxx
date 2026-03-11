/**
 * @file MOM_driver.cpp
 * @brief Driver program for protoMOMxx
 */

#include "MOM.h"
#include "MOM_logger.h"

/// @brief Main entry point for the protoMOMxx driver program.
/// @return Exit code (0 for success, non-zero for failure)
int main() {
  try {

    MOM_logger::info("Hello C++ world. This is protoMOMxx!");

    // Initialize the core MOM object
    MOM mom;

    return 0;

  } catch (const MOM_logger::FatalError&) {
    return EXIT_FAILURE;
  }
}
