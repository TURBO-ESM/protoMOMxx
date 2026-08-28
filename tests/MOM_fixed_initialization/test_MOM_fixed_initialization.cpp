// Unit tests for make_grid (src/initialization/MOM_fixed_initialization.cpp):
// the parameter-driven computation of the grid fields, including defaults and
// the rejection of invalid extents. The grid values themselves are covered in
// tests/MOM_grid.
//
// make_grid requires the infra layer to be initialized, hence the
// main() below, which instantiates a MOM::Infra.

#include <filesystem>
#include <gtest/gtest.h>

#include "MOM_domains.h"
#include "MOM_fixed_initialization.h"
#include "MOM_grid.h"
#include "MOM_grid_initialize.h"
#include "MOM_infra.h"
#include "MOM_logger.h"

namespace {

// Helper function to get the absolute path to the test data directory
std::filesystem::path get_test_data_dir() {
  return std::filesystem::path(__FILE__).parent_path() / "MOM_param_files";
}

// Construct the domain and the grid from one parameter file.
MOM::Grid grid_from_param_file(const std::string &file_name) {
  const auto path = get_test_data_dir() / file_name;
  EXPECT_TRUE(std::filesystem::exists(path)) << "Test file " << path << " does not exist";
  MOM::RuntimeParams params(path.string());
  const MOM::Domain domain = MOM::make_domain(params);
  return MOM::make_grid(domain, params);
}

} // namespace

// read_grid_extents takes the extents from the parameter file, with defaults
// applied for the parameters the file omits. The topography setup calls it for
// itself, so it is tested on its own rather than through the grid setup.
TEST(MOMFixedInitTest, ReadGridExtentsFromParamFile) {
  const auto path = get_test_data_dir() / "MOM_input_test";
  ASSERT_TRUE(std::filesystem::exists(path)) << "Test file " << path << " does not exist";
  MOM::RuntimeParams params(path.string());

  const MOM::GridExtents extents = MOM::read_grid_extents(params);
  EXPECT_DOUBLE_EQ(extents.south_lat, 30.0);
  EXPECT_DOUBLE_EQ(extents.len_lat, 20.0);
  EXPECT_DOUBLE_EQ(extents.len_lon, 22.0);
  EXPECT_DOUBLE_EQ(extents.west_lon, 0.0);       // default
  EXPECT_DOUBLE_EQ(extents.rad_earth, 6.378e6);  // default
}

// make_grid computes the grid fields from the parameter file end to end: the
// corner coordinates land on the configured domain boundaries, and the flat
// topography sits at MAXIMUM_DEPTH.
TEST(MOMFixedInitTest, MakeGridFromParamFile) {
  const MOM::Grid grid = grid_from_param_file("MOM_input_test");

  EXPECT_DOUBLE_EQ(grid.bathyT().max(0), 2000.0);         // TOPO_CONFIG = "flat" at MAXIMUM_DEPTH
  EXPECT_DOUBLE_EQ(grid.geoLatBu().min(0), 30.0);         // SOUTHLAT
  EXPECT_DOUBLE_EQ(grid.geoLatBu().max(0), 30.0 + 20.0);  // + LENLAT
  EXPECT_DOUBLE_EQ(grid.geoLonBu().max(0), 22.0);         // WESTLON + LENLON
}

// Invalid extents are rejected through logger::fatal:
TEST(MOMFixedInitTest, InvalidExtentIsFatal) {
  EXPECT_THROW(grid_from_param_file("MOM_input_bad_extent"), MOM::logger::FatalError);
}

// A MOM6 TOPO_CONFIG option that protoMOMxx defers is rejected
TEST(MOMFixedInitTest, DeferredTopoConfigIsFatal) {
  EXPECT_THROW(grid_from_param_file("MOM_input_bad_topo"), MOM::logger::FatalError);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
