// Unit tests for the Domain class (config_src/infra/MOM_domain_infra.cpp),
// a thin wrapper over TIM::Domain. These tests cover what the wrapper adds.
// The decomposition mechanics are TIM::Domain's concern, tested in TIM's suite.
//
// A Domain requires the infra layer to be initialized, hence the main()
// below, which instantiates a MOM::Infra.

#include <stdexcept>

#include <gtest/gtest.h>

#include <AMReX_Box.H>
#include <AMReX_BoxArray.H>

#include "MOM_domain_infra.h"
#include "MOM_infra.h"

// Exercise the whole wrapper surface
TEST(Domain, MapsVocabularyAndDelegates) {
  const int n_boxes = 6;
  const int n_levels = 3;
  const MOM::Domain domain(10, 8, 2, 3, /*reentrant_x=*/true,
                           /*reentrant_y=*/false, /*tripolar_n=*/false,
                           n_boxes);

  EXPECT_EQ(domain.ni_global(), 10);
  EXPECT_EQ(domain.nj_global(), 8);
  EXPECT_EQ(domain.ni_halo(), 2);
  EXPECT_EQ(domain.nj_halo(), 3);
  EXPECT_TRUE(domain.reentrant_x());
  EXPECT_FALSE(domain.reentrant_y());
  EXPECT_FALSE(domain.tripolar_n());

  EXPECT_TRUE(domain.periodicity().isPeriodic(0));   // reentrant_x
  EXPECT_FALSE(domain.periodicity().isPeriodic(1));  // not reentrant_y
  EXPECT_FALSE(domain.periodicity().isPeriodic(2));  // never periodic in k

  EXPECT_EQ(domain.n_boxes(), n_boxes);
  const amrex::BoxArray box_array = domain.box_array(n_levels);
  EXPECT_EQ(static_cast<int>(box_array.size()), domain.n_boxes());
  EXPECT_EQ(box_array[0].bigEnd(2), n_levels - 1);
  EXPECT_EQ(static_cast<int>(domain.distribution_mapping().size()), domain.n_boxes());
}

// Invalid configurations are rejected under protoMOMxx's throw policy,
// before they can reach TIM's abort-based checks.
TEST(Domain, InvalidConfigurationsThrow) {
  const auto make = [](int ni_global, int nj_global, int ni_halo, int nj_halo,
                       bool reentrant_x, bool reentrant_y, bool tripolar_n,
                       int n_boxes = 0) {
    return MOM::Domain(ni_global, nj_global, ni_halo, nj_halo,
                       reentrant_x, reentrant_y, tripolar_n, n_boxes);
  };
  EXPECT_THROW(make(0, 8, 2, 2, false, false, false), std::invalid_argument);      // non-positive extent
  EXPECT_THROW(make(4, 8, -1, 2, false, false, false), std::invalid_argument);     // negative halo
  EXPECT_THROW(make(4, 8, 2, 2, false, true, true), std::invalid_argument);        // reentrant_y with tripolar_n
  EXPECT_THROW(make(4, 8, 2, 2, false, false, true), std::invalid_argument);       // tripolar_n not implemented yet
  EXPECT_THROW(make(4, 8, 2, 2, false, false, false, -1), std::invalid_argument);  // negative n_boxes
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
