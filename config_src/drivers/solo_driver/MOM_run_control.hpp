#pragma once
/// @file MOM_run_control.hpp
/// @brief The time setup of the solo driver.
///
/// MOM6's solo driver (MOM_driver.F90) does all of this inline and keeps the
/// results as locals of the driver program. Here the reads live in a library,
/// so they can be unit tested.

#include <string>

#include "MOM_file_parser.hpp"
#include "MOM_time_manager.hpp"

namespace MOM {

/// @brief The calendar and the start time of the run.
struct RunStart {
  Calendar calendar = Calendar::Julian;  ///< The calendar (ocean_solo_nml calendar).
  Time start_time;                       ///< The start time of the whole simulation
                                         ///< (ocean_solo_nml date_init).
};

/// @brief The run-control parameters of the solo driver.
struct RunControl {
  Duration time_step_ocean{};  ///< The forcing time step, from DT_FORCING.
  Time time_end;               ///< The end time of the run, from DAYMAX.
};

/// @brief Read ocean_solo_nml and set the calendar and the start time, as
/// MOM6's solo driver does before it initializes the model.
/// @param nml_file The namelist file (input.nml).
/// @param restart_input_dir The restart input directory.
/// @return The calendar and the start time.
/// @throws logger::FatalError for an invalid or not yet supported setting.
RunStart read_ocean_solo_nml(const std::string &nml_file, const std::string &restart_input_dir);

/// @brief Read the run-control parameters of the solo driver.
/// @param params Runtime parameters.
/// @param time The model time at the start of the run.
/// @return The run-control parameters.
/// @throws logger::FatalError if DT_FORCING is not positive or not a whole
///         number of seconds, if SINGLE_STEPPING_CALL is false, or if @p time
///         is at or after the end time.
/// @throws std::out_of_range if DT or DAYMAX is missing.
RunControl read_run_control(RuntimeParams &params, Time time);

} // namespace MOM
