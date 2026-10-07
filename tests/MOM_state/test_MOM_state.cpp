// Sanity tests for the prognostic state initialization
// (src/initialization/MOM_state_initialization.cpp) on the double_gyre
// configuration: the layer thicknesses against the bottom depth, the halo
// fill, and the fluid at rest.
//
// The state lives on the domain's decomposition, so the infra layer must be
// up, hence the main() below.

#include <filesystem>
#include <string>
#include <gtest/gtest.h>

#include <AMReX_MultiFab.H>
#include <AMReX_ParallelDescriptor.H>

#include "MOM_domain_infra.hpp"
#include "MOM_domains.hpp"
#include "MOM_fixed_initialization.hpp"
#include "MOM_grid.hpp"
#include "MOM_infra.hpp"
#include "MOM_state.hpp"
#include "MOM_state_initialization.hpp"
#include "MOM_vertical_grid.hpp"

namespace {

// The double_gyre configuration (tests/double_gyre/MOM_input).
constexpr int NJ = 40;
constexpr int NK = 2;
constexpr double MAXIMUM_DEPTH = 2000.0;  // [m]
constexpr double ANGSTROM = 1.0e-10;      // [m] (the default)

// A parameter file given relative to this test's directory.
std::string param_file(const std::string &rel_path) {
  return (std::filesystem::path(__FILE__).parent_path() / rel_path).string();
}

// The domain and the grids of the driving testcase, which the state is set
// up on.
struct DoubleGyre {
  MOM::RuntimeParams params{param_file("../double_gyre/MOM_input")};
  MOM::Domain domain = MOM::make_domain(params);
  MOM::Grid grid = MOM::make_grid(domain, params);
  MOM::VerticalGrid vgrid{params};
};

// Check the value of a field at point (i, j, k), over every local box grown
// by its ghost cells that holds the point. Ranks holding none of it stay
// quiet, but the check fails if no rank holds it at all.
// todo: move to a shared test utils module and remove the k = 0 copy in
//       test_MOM_grid.cpp. Note, amrex::get_cell_data does not replace it, since
//       it searches valid boxes only and the checks here are on halo points. It
//       reads on the device, though, which this helper should also do once
//       tests run on GPU builds.
void expect_value_at(const amrex::MultiFab &mf, const amrex::IntVect &p,
                     const double expected) {
  bool found = false;
  for (amrex::MFIter mfi(mf); mfi.isValid(); ++mfi) {
    if (mfi.growntilebox().contains(p)) {
      found = true;
      EXPECT_DOUBLE_EQ(mf.const_array(mfi)(p), expected);
    }
  }
  amrex::ParallelDescriptor::ReduceBoolOr(found);
  EXPECT_TRUE(found);
}

} // namespace

TEST(State, DoubleGyreStateSanity) {
  DoubleGyre dg;
  const MOM::State state = MOM::make_state(dg.domain, dg.grid, dg.vgrid, dg.params);

  // Where the bottom is deeper than MAXIMUM_DEPTH/nk, the top layer is
  // MAXIMUM_DEPTH/nk thick. The spoon shallows to well above that depth at
  // its edges, where the bottom layer is squeezed down to an Angstrom.
  EXPECT_DOUBLE_EQ(state.h.max(0), MAXIMUM_DEPTH / NK);
  EXPECT_DOUBLE_EQ(state.h.min(0), ANGSTROM);

  // The layers fill the water column: summed over all columns, the
  // thicknesses add up to the bottom depths.
  const double total_depth = dg.grid.bathyT().sum(0);
  EXPECT_NEAR(state.h.sum(0), total_depth, 1e-14 * total_depth);

  // Beyond the closed western boundary, the halo keeps the Angstrom that h
  // is allocated with.
  expect_value_at(state.h, amrex::IntVect(-1, NJ / 2, 0), ANGSTROM);

  // The fluid starts at rest, halos included.
  EXPECT_EQ(state.u.norminf(0, 1, dg.domain.nghost()), 0.0);
  EXPECT_EQ(state.v.norminf(0, 1, dg.domain.nghost()), 0.0);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
