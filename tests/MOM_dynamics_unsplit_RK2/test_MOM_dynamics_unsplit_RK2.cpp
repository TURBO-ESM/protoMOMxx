// Tests for the unsplit RK2 time stepper's setup,
// src/core/MOM_dynamics_unsplit_RK2.cpp. The setup needs the grids, which
// live on the infra layer, hence the main() below.

#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "MOM_domains.hpp"
#include "MOM_dynamics_unsplit_RK2.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_fixed_initialization.hpp"
#include "MOM_grid.hpp"
#include "MOM_infra.hpp"
#include "MOM_logger.hpp"
#include "MOM_vertical_grid.hpp"

namespace {

/// @brief Set up the unsplit RK2 time stepper from double_gyre's MOM_input,
/// followed by an override file from MOM_param_files if one is named.
void initialize_with(const std::string &override_file) {
  const std::filesystem::path dir = std::filesystem::path(__FILE__).parent_path();
  std::vector<std::string> files{(dir.parent_path() / "double_gyre" / "MOM_input").string()};
  if (!override_file.empty()) {
    files.push_back((dir / "MOM_param_files" / override_file).string());
  }
  MOM::RuntimeParams params(files);
  const MOM::Domain domain = MOM::make_domain(params);
  const MOM::Grid G = MOM::make_grid(domain, params);
  const MOM::VerticalGrid GV(params);
  (void)MOM::initialize_dyn_unsplit_RK2(G, GV, params);
}

} // namespace

// double_gyre's own settings set up without an error, so the fatal tests
// below fail for the reason they name.
TEST(DynUnsplitRK2Test, DoubleGyreSetsUp) {
  EXPECT_NO_THROW(initialize_with(""));
}

// The options that are not implemented yet are fatal.
TEST(DynUnsplitRK2Test, DebugIsFatal) {
  EXPECT_THROW(initialize_with("MOM_override_debug"), MOM::logger::FatalError);
}

TEST(DynUnsplitRK2Test, CalculateSALIsFatal) {
  EXPECT_THROW(initialize_with("MOM_override_sal"), MOM::logger::FatalError);
}

TEST(DynUnsplitRK2Test, TidesIsFatal) {
  EXPECT_THROW(initialize_with("MOM_override_tides"), MOM::logger::FatalError);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
