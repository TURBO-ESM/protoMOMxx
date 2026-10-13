/// @file MOM_run_control.cpp
/// @brief Implementation of the time setup of the solo driver.

#include "MOM_run_control.hpp"

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <vector>

#include "MOM_logger.hpp"
#include "MOM_nml_parser.hpp"
#include "MOM_string_utils.hpp"

namespace MOM {

namespace {

// The units that DAYMAX and RESTINT are logged in, named after TIMEUNIT, as
// MOM6's log_param_time names them.
std::string time_units(double timeunit) {
  if (std::abs(timeunit - 1.0) < 0.01) return "seconds";
  if (std::abs(timeunit - 3600.0) < 1.0) return "hours";
  if (std::abs(timeunit - 86400.0) < 1.0) return "days";
  if (std::abs(timeunit - 3.1e7) < 1.0e6) return "years";
  // Fortran's es8.2 format, e.g. "1.20E+03 sec".
  std::ostringstream units;
  units << std::uppercase << std::scientific << std::setprecision(2) << std::setw(8)
        << timeunit << " sec";
  return units.str();
}

} // namespace

RunStart read_ocean_solo_nml(const std::string &nml_file, const std::string &restart_input_dir) {

  // The ocean_solo_nml variables, with MOM6's defaults.
  std::vector<int> date_init(6, 0);
  std::string calendar = "julian";
  int months = 0, days = 0, hours = 0, minutes = 0, seconds = 0;

  const NamelistParams nml(nml_file);
  const std::string ocean_solo_nml = "ocean_solo_nml";
  nml.get("date_init", date_init, ocean_solo_nml);
  if (date_init.size() > 6) {
    logger::fatal("MOM_driver: ocean_solo_nml date_init has more than six values.");
  }
  date_init.resize(6);  // As in Fortran, a shorter list leaves the rest at zero.

  nml.get("calendar", calendar, ocean_solo_nml);
  nml.get("months", months, ocean_solo_nml);
  nml.get("days", days, ocean_solo_nml);
  nml.get("hours", hours, ocean_solo_nml);
  nml.get("minutes", minutes, ocean_solo_nml);
  nml.get("seconds", seconds, ocean_solo_nml);

  // MOM6 sets the end time from the run length when one is given here, and
  // from DAYMAX otherwise.
  if (months + days + hours + minutes + seconds > 0) {
    logger::fatal("MOM_driver: Setting the run length in ocean_solo_nml is not implemented yet. "
                  "Set DAYMAX instead.");
  }

  // MOM6 takes the calendar and the segment start time from ocean_solo.res when
  // the file exists, which is how a run continues from a restart.
  const std::string solo_res = restart_input_dir + "ocean_solo.res";
  if (std::filesystem::exists(solo_res)) {
    logger::fatal("MOM_driver: Starting from ", solo_res, " is not implemented yet.");
  }

  RunStart start;

  calendar = string_utils::uppercase(calendar);
  if (calendar.starts_with("JULIAN")) {
    start.calendar = Calendar::Julian;
  } else if (calendar.starts_with("GREGORIAN")) {
    start.calendar = Calendar::Gregorian;
  } else if (calendar.starts_with("NOLEAP")) {
    start.calendar = Calendar::NoLeap;
  } else if (calendar.starts_with("THIRTY_DAY")) {
    start.calendar = Calendar::ThirtyDayMonths;
  } else if (calendar.starts_with("NO_CALENDAR")) {
    start.calendar = Calendar::NoCalendar;
  } else if (!calendar.empty()) {
    logger::fatal("MOM_driver: Invalid namelist value ", calendar, " for calendar");
  } else {
    logger::fatal("MOM_driver: No namelist value for calendar");
  }

  if (std::accumulate(date_init.begin(), date_init.end(), 0) > 0) {
    const Date date{.year = date_init[0], .month = date_init[1], .day = date_init[2],
                    .hour = date_init[3], .minute = date_init[4], .second = date_init[5]};
    start.start_time = date.to_time(start.calendar);
  }

  return start;
}

RunControl read_run_control(RuntimeParams &params, Time time) {

  params.doc_module("MOM_main (MOM_driver)", "");

  RunControl run;

  // DT is logged by the MOM module, which reads it first.
  double dt = 0.0;
  params.get("DT", dt, {.fail_if_missing = true});

  double dt_forcing = dt;
  params.get("DT_FORCING", dt_forcing,
             {.default_value = dt,
              .desc = "The time step for changing forcing, coupling with other "
                      "components, or potentially writing certain diagnostics. "
                      "The default value is given by DT.",
              .units = "s"});
  // defer: DT_OFFLINE, read in offline tracer mode.
  // MOM6's ntstep, the number of dynamics steps in a forcing step, counts the
  // steps for MOM_write_cputime and sizes the diabatic steps of the separate
  // calls (SINGLE_STEPPING_CALL = False). Neither is implemented yet.

  if (dt_forcing <= 0.0) {
    logger::fatal("MOM_driver: DT_FORCING = ", dt_forcing, " s, but it must be positive.");
  }
  run.time_step_ocean = real_to_time(dt_forcing);
  const bool elapsed_time_master =
      std::abs(dt_forcing - time_to_real(run.time_step_ocean)) > 1.0e-12 * dt_forcing;
  if (elapsed_time_master) {
    logger::fatal("MOM_driver: DT_FORCING = ", dt_forcing, " s is not a whole number of seconds. "
                  "Using the real elapsed time for the master clock is not implemented yet.");
  }

  double time_unit = 86400.0;
  params.get("TIMEUNIT", time_unit,
             {.default_value = 86400.0,
              .desc = "The time unit for DAYMAX, ENERGYSAVEDAYS, and RESTINT.",
              .units = "s"});
  const std::string time_unit_name = time_units(time_unit);

  // The run length from ocean_solo_nml is not implemented (read_ocean_solo_nml
  // stops if it is set), so the end time comes from DAYMAX.
  double daymax = 0.0;
  params.get("DAYMAX", daymax,
             {.desc = "The final time of the whole simulation, in units of "
                      "TIMEUNIT seconds.  This also sets the potential end "
                      "time of the present run segment if the end time is "
                      "not set via ocean_solo_nml in input.nml.",
              .units = time_unit_name,
              .fail_if_missing = true});
  run.time_end = Time{real_to_time(daymax * time_unit)};
  // defer: diag_manager_set_time_end_infra()

  bool single_step_call = true;
  params.get("SINGLE_STEPPING_CALL", single_step_call,
             {.default_value = true,
              .desc = "If true, advance the state of MOM with a single step "
                      "including both dynamics and thermodynamics.  If false "
                      "the two phases are advanced with separate calls."});
  if (!single_step_call) {
    logger::fatal("MOM_driver: SINGLE_STEPPING_CALL = False, which advances the dynamics and the "
                  "thermodynamics with separate calls, is not implemented yet.");
  }
  // MOM6 reads DT_THERM and DIABATIC_FIRST here, for the separate calls only.
  // Both are logged by the MOM module, which reads them first.

  if (time >= run.time_end) {
    logger::fatal("MOM_driver: The run has been started at or after the end time of the run.");
  }

  // defer: restart writing. RESTART_CONTROL and RESTINT are read so that they
  //        are logged, and are not used yet.
  int restart_control = 1;
  params.get("RESTART_CONTROL", restart_control,
             {.default_value = 1,
              .desc = "An integer whose bits encode which restart files are "
                      "written. Add 2 (bit 1) for a time-stamped file, and odd "
                      "(bit 0) for a non-time-stamped file. A non-time-stamped "
                      "restart file is saved at the end of the run segment "
                      "for any non-negative value."});
  double restint = 0.0;
  params.get("RESTINT", restint,
             {.default_value = 0.0,
              .desc = "The interval between saves of the restart file in units "
                      "of TIMEUNIT.  Use 0 (the default) to not save "
                      "incremental restart files at all.",
              .units = time_unit_name});

  // defer: MOM_write_cputime (the CPU time file and the MAXCPU run limit).
  //        WRITE_CPU_STEPS is read so that it is logged, and is not used yet.
  int cpu_steps = 1000;
  params.get("WRITE_CPU_STEPS", cpu_steps,
             {.default_value = 1000,
              .desc = "The number of coupled timesteps between writing the cpu "
                      "time. If this is not positive, do not check cpu time, and "
                      "the segment run-length can not be set via an elapsed CPU time."});
  // The driver's DEBUG read comes with its use, the forcing checksums after set_forcing.

  return run;
}

} // namespace MOM
