// Unit tests for the time manager (config_src/infra/MOM_time_manager.cpp).

#include <gtest/gtest.h>

#include "MOM_time_manager.hpp"

// Check that the conversion from a real number of seconds to a Duration
// rounds as MOM6 does.
TEST(TimeManager, RealToTimeRoundsToWholeSeconds) {
  EXPECT_EQ(MOM::real_to_time(2400.0), MOM::Duration{2400});
  EXPECT_EQ(MOM::real_to_time(2400.1), MOM::Duration{2400});
  EXPECT_EQ(MOM::real_to_time(2400.5), MOM::Duration{2401});
}
