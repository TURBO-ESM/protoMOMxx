// Unit tests for the time setup of the solo driver
// (config_src/drivers/solo_driver/MOM_run_control.cpp).

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "MOM_run_control.hpp"

namespace {

// A file or directory given relative to this test's directory.
std::string test_path(const std::string &rel_path) {
  return (std::filesystem::path(__FILE__).parent_path() / rel_path).string();
}

} // namespace

// The double_gyre run: its input.nml has no ocean_solo_nml, so the calendar is
// Julian and the run starts at the calendar base date, and DAYMAX = 10 days
// takes 360 forcing steps of DT_FORCING = 2400 s.
TEST(RunControl, DoubleGyre) {
  const std::string double_gyre = "../../../double_gyre/";

  const MOM::RunStart start =
      MOM::read_ocean_solo_nml(test_path(double_gyre + "input.nml"), test_path(double_gyre));
  EXPECT_EQ(start.calendar, MOM::Calendar::Julian);
  EXPECT_EQ(start.start_time, MOM::Time{});

  MOM::RuntimeParams params(std::vector<std::string>{test_path(double_gyre + "MOM_input"),
                                                     test_path(double_gyre + "MOM_override")});
  const MOM::RunControl run = MOM::read_run_control(params, start.start_time);
  EXPECT_EQ(run.time_step_ocean, MOM::Duration{2400});
  EXPECT_EQ(run.time_end, MOM::Time{} + std::chrono::days{10});
  EXPECT_EQ((run.time_end - start.start_time) / run.time_step_ocean, 360);
}
