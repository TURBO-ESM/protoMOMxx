// Tests for the vertical extent of the layers, src/core/MOM_interface_heights.cpp.

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.hpp"
#include "MOM_domains.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_fixed_initialization.hpp"
#include "MOM_grid.hpp"
#include "MOM_infra.hpp"
#include "MOM_interface_heights.hpp"
#include "MOM_state.hpp"
#include "MOM_state_initialization.hpp"
#include "MOM_vertical_grid.hpp"

namespace {

/// @brief The double_gyre parameter file.
std::string double_gyre_input() {
  return (std::filesystem::path(__FILE__).parent_path().parent_path() / "double_gyre" /
          "MOM_input")
      .string();
}

} // namespace

// double_gyre is Boussinesq, where a thickness is a height, so dz equals h on
// the valid points and the first halo point.
TEST(ThicknessToDzTest, DzEqualsHInBoussinesqMode) {
  MOM::RuntimeParams params(double_gyre_input());
  const MOM::Domain domain = MOM::make_domain(params);
  const MOM::Grid G = MOM::make_grid(domain, params);
  const MOM::VerticalGrid GV(params);
  const MOM::State state = MOM::make_state(domain, G, GV, params);
  ASSERT_TRUE(GV.Boussinesq());

  amrex::MultiFab dz = domain.make_h_field({.nk = GV.nk()});
  MOM::thickness_to_dz(state.h, dz, GV, /*halo_size=*/1);

  const amrex::IntVect one(1, 1, 0);
  amrex::MultiFab diff(dz.boxArray(), dz.DistributionMap(), dz.nComp(), one);
  amrex::MultiFab::Copy(diff, dz, 0, 0, dz.nComp(), one);
  amrex::MultiFab::Subtract(diff, state.h, 0, 0, dz.nComp(), one);
  EXPECT_EQ(diff.norminf(0, dz.nComp(), one), 0.0);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
