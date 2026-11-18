// Tests for the unsplit RK2 time stepper, src/core/MOM_dynamics_unsplit_RK2.cpp:
// its setup and one time step. Both need the grids, which live on the infra
// layer, hence the main() below.

#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <AMReX_MultiFab.H>

#include "MOM_domains.hpp"
#include "MOM_dynamics_unsplit_RK2.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_fixed_initialization.hpp"
#include "MOM_forcing_type.hpp"
#include "MOM_grid.hpp"
#include "MOM_infra.hpp"
#include "MOM_logger.hpp"
#include "MOM_porous_barrier.hpp"
#include "MOM_state.hpp"
#include "MOM_state_initialization.hpp"
#include "MOM_vertical_grid.hpp"

namespace {

/// @brief The directory of this test.
std::filesystem::path test_dir() { return std::filesystem::path(__FILE__).parent_path(); }

/// @brief The double_gyre parameter file.
std::string double_gyre_input() {
  return (test_dir().parent_path() / "double_gyre" / "MOM_input").string();
}

/// @brief Set up the unsplit RK2 time stepper from double_gyre's MOM_input,
/// followed by an override file from MOM_param_files if one is named.
void initialize_with(const std::string &override_file) {
  const std::filesystem::path dir = test_dir();
  std::vector<std::string> files{double_gyre_input()};
  if (!override_file.empty()) {
    files.push_back((dir / "MOM_param_files" / override_file).string());
  }
  MOM::RuntimeParams params(files);
  const MOM::Domain domain = MOM::make_domain(params);
  const MOM::Grid G = MOM::make_grid(domain, params);
  const MOM::VerticalGrid GV(params);
  const MOM::PorousBarrier pbv = MOM::make_porous_barrier(domain, GV.nk());
  (void)MOM::make_dyn_unsplit_RK2(domain, G, GV, pbv, params);
}

/// @brief The largest absolute value of a field, ghost cells included.
amrex::Real max_abs(const amrex::MultiFab &mf) {
  return mf.norminf(0, mf.nComp(), mf.nGrowVect());
}

/// @brief The largest difference between two fields, ghost cells included.
amrex::Real max_difference(const amrex::MultiFab &a, const amrex::MultiFab &b) {
  amrex::MultiFab diff(a.boxArray(), a.DistributionMap(), a.nComp(), a.nGrowVect());
  amrex::MultiFab::Copy(diff, a, 0, 0, a.nComp(), a.nGrowVect());
  amrex::MultiFab::Subtract(diff, b, 0, 0, a.nComp(), a.nGrowVect());
  return diff.norminf(0, a.nComp(), a.nGrowVect());
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

// One step from double_gyre's state at rest, on one box and on six. The
// force terms are still stubs, so the state stays at rest and h doesn't
// change. This checks that the step runs, including its halo exchanges and
// the continuity call that updates h in place.
TEST(DynUnsplitRK2Test, StateAtRestStaysAtRest) {
  for (const int n_boxes : {1, 6}) {
    MOM::RuntimeParams params(double_gyre_input());
    const MOM::Domain whole = MOM::make_domain(params);
    const MOM::Domain domain({.ni_global = whole.ni_global(),
                              .nj_global = whole.nj_global(),
                              .ni_halo = whole.ni_halo(),
                              .nj_halo = whole.nj_halo(),
                              .n_boxes = n_boxes});
    const MOM::Grid G = MOM::make_grid(domain, params);
    const MOM::VerticalGrid GV(params);
    MOM::State state = MOM::make_state(domain, G, GV, params);
    const MOM::PorousBarrier pbv = MOM::make_porous_barrier(domain, GV.nk());
    const MOM::DynUnsplitRK2 dyn = MOM::make_dyn_unsplit_RK2(domain, G, GV, pbv, params);
    const MOM::MechForcing forces = MOM::make_mech_forcing(domain);

    const amrex::IntVect ng = state.h.nGrowVect();
    amrex::MultiFab h_start(state.h.boxArray(), state.h.DistributionMap(), state.h.nComp(), ng);
    amrex::MultiFab::Copy(h_start, state.h, 0, 0, state.h.nComp(), ng);

    dyn.step(state, /*dt=*/300.0, forces, /*x_first=*/true);

    EXPECT_EQ(max_difference(state.h, h_start), 0.0) << n_boxes << " boxes";
    EXPECT_EQ(max_abs(state.u), 0.0) << n_boxes << " boxes";
    EXPECT_EQ(max_abs(state.v), 0.0) << n_boxes << " boxes";
    EXPECT_EQ(max_abs(state.uhtr), 0.0) << n_boxes << " boxes";
    EXPECT_EQ(max_abs(state.vhtr), 0.0) << n_boxes << " boxes";
  }
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
