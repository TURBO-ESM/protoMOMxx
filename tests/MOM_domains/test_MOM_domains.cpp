// Unit tests for the MOM_domains module (src/framework/MOM_domains.cpp),
// which implicitly depends on the infra (AMReX) layer being initialized.
// (Hence the main() below, which instantiates a MOM::Infra instance.)

#include <filesystem>
#include <gtest/gtest.h>

#include <AMReX_MultiFab.H>

#include "MOM_domains.h"
#include "MOM_infra.h"
#include "MOM_logger.h"

// Helper function to get the absolute path to the test data directory
std::filesystem::path get_test_data_dir() {
  return std::filesystem::path(__FILE__).parent_path() / "MOM_param_files";
}

// Check that make_domain constructs a Domain from the parameter file,
// with defaults applied for the parameters the file omits.
TEST(MOMDomainsTest, MakeDomainFromParamFile) {
  auto test_file_path = get_test_data_dir() / "MOM_input_test";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  MOM::RuntimeParams params(test_file_path.string());
  const MOM::Domain domain = MOM::make_domain(params);

  EXPECT_EQ(domain.ni_global(), 44);
  EXPECT_EQ(domain.nj_global(), 40);
  EXPECT_EQ(domain.ni_halo(), 4);      // default
  EXPECT_EQ(domain.nj_halo(), 4);      // default
  EXPECT_FALSE(domain.reentrant_x());  // REENTRANT_X = False in the file
  EXPECT_FALSE(domain.reentrant_y());  // default
  EXPECT_FALSE(domain.tripolar_n());   // default
}

// Nonsensical configuration values are rejected through logger::fatal
// (NIGLOBAL = 0 standing in for the make_domain value checks).
TEST(MOMDomainsTest, NonPositiveExtentIsFatal) {
  auto test_file_path = get_test_data_dir() / "MOM_input_bad_extent";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  MOM::RuntimeParams params(test_file_path.string());
  EXPECT_THROW(MOM::make_domain(params), MOM::logger::FatalError);
}

// (Missing mandatory parameters -- NIGLOBAL, NJGLOBAL -- throwing is the
// parameter table's contract, tested in test_MOM_file_parser; it is
// deliberately not re-verified here.)

// The h/u/v/q field factories map onto the right staggerings.
TEST(MOMDomainsTest, PointVocabularyFactories) {
  const MOM::Domain domain({.ni_global = 8, .nj_global = 6});

  EXPECT_TRUE(domain.make_h_field(1, 1).ixType().cellCentered());
  EXPECT_EQ(domain.make_u_field(1, 1).ixType(), amrex::IndexType(amrex::IntVect(1, 0, 0)));
  EXPECT_EQ(domain.make_v_field(1, 1).ixType(), amrex::IndexType(amrex::IntVect(0, 1, 0)));
  EXPECT_EQ(domain.make_q_field(1, 1).ixType(), amrex::IndexType(amrex::IntVect(1, 1, 0)));
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
