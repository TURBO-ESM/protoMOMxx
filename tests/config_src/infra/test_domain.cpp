#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

#include "MOM_domain.h"

TEST(DomainConstructor, Constructor) {
  const std::size_t ni_global = 4;
  const std::size_t nj_global = 8;
  const std::size_t ni_halo = 2;
  const std::size_t nj_halo = 2;
  const bool reentrant_i = false;
  const bool reentrant_j = false;
  const bool tripolar_N = false;

  // Test valid configuration
  MOM::Domain domain(ni_global, nj_global, ni_halo, nj_halo, reentrant_i,
                     reentrant_j, tripolar_N);
  EXPECT_EQ(ni_global, domain.ni_global);
  EXPECT_EQ(nj_global, domain.nj_global);
  EXPECT_EQ(ni_halo, domain.ni_halo);
  EXPECT_EQ(nj_halo, domain.nj_halo);
  EXPECT_EQ(reentrant_i, domain.reentrant_i);
  EXPECT_EQ(reentrant_j, domain.reentrant_j);
  EXPECT_EQ(tripolar_N, domain.tripolar_N);

  // Test invalid configuration: reentrant_j and tripolar_N cannot both be true
  EXPECT_THROW(MOM::Domain(ni_global, nj_global, ni_halo, nj_halo, reentrant_i,
                           true, true),
               std::invalid_argument);
}

// Test harness for Domain class.
class DomainTest : public ::testing::Test {
protected:
  const std::size_t ni_global = 4;
  const std::size_t nj_global = 8;
  const std::size_t ni_halo = 2;
  const std::size_t nj_halo = 2;
  const bool reentrant_i = false;
  const bool reentrant_j = false;
  const bool tripolar_N = false;
  MOM::Domain domain = MOM::Domain(ni_global, nj_global, ni_halo, nj_halo,
                                   reentrant_i, reentrant_j, tripolar_N);
};

TEST_F(DomainTest, ConstructorAgainToVerifyFixture) {
  // Just to verify that the fixture is set up correctly.
  // Checks we have access to its protected members (domain and ni_global)
  // inside the tests derived from the fixture.
  EXPECT_EQ(ni_global, domain.ni_global);
}
