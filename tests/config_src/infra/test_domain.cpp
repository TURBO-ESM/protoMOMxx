// Unit tests for the Domain class (config_src/infra/MOM_domain_infra.cpp),
// a thin wrapper over TIM::Domain. These tests cover what the wrapper adds.
// The decomposition mechanics are TIM::Domain's concern, tested in TIM's suite.
//
// A Domain requires the infra layer to be initialized, hence the main()
// below, which instantiates a MOM::Infra.

#include <gtest/gtest.h>

#include "MOM_domain_infra.h"
#include "MOM_infra.h"

// Exercise the whole wrapper surface
TEST(Domain, MapsVocabularyAndDelegates) {
  const int n_boxes = 6;
  const MOM::Domain domain({.ni_global = 10, .nj_global = 8,
                            .ni_halo = 2, .nj_halo = 3,
                            .reentrant_x = true,
                            .n_boxes = n_boxes});

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
  EXPECT_EQ(domain.n_boxes(), n_boxes);              // consistent num. boxes
}

// (Invalid plain values aborting is TIM::Domain's contract, tested in TIM's
// suite; the configuration-driven path is validated under protoMOMxx's throw
// policy in make_domain and tested in tests/MOM_domains.)

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
