// Unit test for the Model initialization skeleton (src/core/MOM.cpp).
//
// A Model's only constructor argument is a RuntimeParams object, which is
// injected by the driver. Additionally, the Model implicitly depends on
// the infra (AMReX) layer being initialized. (Hence the main() below).

#include <filesystem>
#include <gtest/gtest.h>
#include <string>

#include "MOM.h"
#include "MOM_infra.h"

// Helper function to get the absolute path to the test data directory
std::filesystem::path get_test_data_dir() {
  return std::filesystem::path(__FILE__).parent_path() / "MOM_param_files";
}

// Check if the Model can be constructed from a RuntimeParams object.
TEST(MOMModelTest, ConstructsFromInjectedParams) {
  auto test_file_path = get_test_data_dir() / "MOM_input_test";
  ASSERT_TRUE(std::filesystem::exists(test_file_path)) << "Test file " << test_file_path << " does not exist";

  MOM::RuntimeParams params(test_file_path.string());
  const MOM::Model model(params);

  EXPECT_FALSE(model.config().split);
  EXPECT_TRUE(model.config().use_RK2);
}

/// @brief Test-binary entry point: initialize GTest, bring up the infrastructure
/// layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
