// Unit tests for the MOM_domains module (src/framework/MOM_domains.cpp),
// which implicitly depends on the infra (AMReX) layer being initialized.
// (Hence the main() below, which instantiates a MOM::Infra instance.)

#include <exception>
#include <filesystem>
#include <gtest/gtest.h>

#include "MOM_domains.h"
#include "MOM_infra.h"

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

// A parameter file missing the mandatory extents fails under protoMOMxx's
// error policy: construction throws (currently std::out_of_range from the
// parameter table's mandatory-key lookup), and the driver's top-level
// std::exception handler converts it to a nonzero exit.
TEST(MOMDomainsTest, MissingExtentsAreFatal) {
  auto test_file_path = get_test_data_dir() / "MOM_input_no_extents";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  MOM::RuntimeParams params(test_file_path.string());
  EXPECT_THROW(MOM::make_domain(params), std::exception);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
