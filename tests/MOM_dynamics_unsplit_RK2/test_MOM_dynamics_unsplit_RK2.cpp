// Tests for the unsplit RK2 time stepper, src/core/MOM_dynamics_unsplit_RK2.cpp:
// its setup and one time step. Both need the grids, which live on the infra
// layer, hence the main() below.

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <AMReX_MFIter.H>
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
#include "MOM_set_viscosity.hpp"
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
  // SetVisc comes from double_gyre's own settings, so the override reaches
  // only make_dyn_unsplit_RK2. make_set_visc also rejects DEBUG = True.
  MOM::RuntimeParams base(double_gyre_input());
  MOM::ViscFields visc;
  const MOM::SetVisc set_visc = MOM::make_set_visc(domain, G, GV, pbv, base, visc);
  (void)MOM::make_dyn_unsplit_RK2(domain, G, GV, pbv, set_visc, params);
}

/// @brief Expect the setup with an override file to stop with a fatal error
/// from make_dyn_unsplit_RK2 itself.
void expect_fatal_from_stepper(const std::string &override_file) {
  try {
    initialize_with(override_file);
    ADD_FAILURE() << override_file << ": no fatal error";
  } catch (const MOM::logger::FatalError &e) {
    EXPECT_NE(std::string(e.what()).find("make_dyn_unsplit_RK2"), std::string::npos)
        << override_file << ": " << e.what();
  }
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
  expect_fatal_from_stepper("MOM_override_debug");
}

TEST(DynUnsplitRK2Test, CalculateSALIsFatal) {
  expect_fatal_from_stepper("MOM_override_sal");
}

TEST(DynUnsplitRK2Test, TidesIsFatal) {
  expect_fatal_from_stepper("MOM_override_tides");
}

// One step from double_gyre's state at rest, on one box and on six. Without
// wind every force term is 0, and the pressure force is still a stub, so the
// state stays at rest and h doesn't change. This checks that the step runs,
// including its halo exchanges and the continuity call that updates h in
// place.
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
    MOM::ViscFields visc;
    const MOM::SetVisc set_visc = MOM::make_set_visc(domain, G, GV, pbv, params, visc);
    const MOM::DynUnsplitRK2 dyn =
        MOM::make_dyn_unsplit_RK2(domain, G, GV, pbv, set_visc, params);
    const MOM::MechForcing forces = MOM::make_mech_forcing(domain);

    const amrex::IntVect ng = state.h.nGrowVect();
    amrex::MultiFab h_start(state.h.boxArray(), state.h.DistributionMap(), state.h.nComp(), ng);
    amrex::MultiFab::Copy(h_start, state.h, 0, 0, state.h.nComp(), ng);

    dyn.step(state, visc, /*dt=*/300.0, forces, /*x_first=*/true);

    EXPECT_EQ(max_difference(state.h, h_start), 0.0) << n_boxes << " boxes";
    EXPECT_EQ(max_abs(state.u), 0.0) << n_boxes << " boxes";
    EXPECT_EQ(max_abs(state.v), 0.0) << n_boxes << " boxes";
    EXPECT_EQ(max_abs(state.uhtr), 0.0) << n_boxes << " boxes";
    EXPECT_EQ(max_abs(state.vhtr), 0.0) << n_boxes << " boxes";
  }
}

namespace {

/// @brief double_gyre's setup and unsplit RK2 time stepper on a domain split
/// into n_boxes, at rest, with a made-up wind stress that varies in space.
struct WindCase {
  MOM::RuntimeParams params{double_gyre_input()};
  MOM::Domain domain;
  MOM::Grid G;
  MOM::VerticalGrid GV;
  MOM::State state;
  MOM::PorousBarrier pbv;
  MOM::ViscFields visc;
  MOM::SetVisc set_visc;
  MOM::DynUnsplitRK2 dyn;
  MOM::MechForcing forces;

  // set_visc and dyn refer to this case's domain, grids and other members. A
  // copy or a move would leave them pointing at the original's.
  WindCase(const WindCase &) = delete;
  WindCase &operator=(const WindCase &) = delete;
  WindCase(WindCase &&) = delete;
  WindCase &operator=(WindCase &&) = delete;

  explicit WindCase(const int n_boxes)
      : domain(split(n_boxes)),
        G(MOM::make_grid(domain, params)),
        GV(params),
        state(MOM::make_state(domain, G, GV, params)),
        pbv(MOM::make_porous_barrier(domain, GV.nk())),
        set_visc(MOM::make_set_visc(domain, G, GV, pbv, params, visc)),
        dyn(MOM::make_dyn_unsplit_RK2(domain, G, GV, pbv, set_visc, params)),
        forces(MOM::make_mech_forcing(domain)) {
    // The pattern depends only on the global index, so it is the same on any
    // number of boxes.
    for (amrex::MultiFab *tau : {&forces.taux, &forces.tauy}) {
      for (amrex::MFIter mfi(*tau); mfi.isValid(); ++mfi) {
        const amrex::Array4<amrex::Real> a = tau->array(mfi);
        amrex::ParallelFor(mfi.fabbox(), [=] AMREX_GPU_DEVICE(int i, int j, int k) {
          a(i, j, k) = 0.1 * std::sin(0.7 * i + 0.3 * j);
        });
      }
    }
  }

  /// @brief double_gyre's domain, split into n_boxes.
  MOM::Domain split(const int n_boxes) {
    const MOM::Domain whole = MOM::make_domain(params);
    return MOM::Domain({.ni_global = whole.ni_global(),
                        .nj_global = whole.nj_global(),
                        .ni_halo = whole.ni_halo(),
                        .nj_halo = whole.nj_halo(),
                        .n_boxes = n_boxes});
  }
};

/// @brief Expect two fields on different boxes to have the same values at
/// their valid points.
void expect_same_values(const amrex::MultiFab &got, const amrex::MultiFab &expected,
                        const std::string &name) {
  amrex::MultiFab diff(got.boxArray(), got.DistributionMap(), got.nComp(), 0);
  diff.ParallelCopy(expected);
  amrex::MultiFab::Subtract(diff, got, 0, 0, got.nComp(), 0);
  EXPECT_EQ(diff.norminf(), 0.0) << name;
}

} // namespace

// Two wind-driven steps give the same u, v and h on six boxes as on one, at
// the valid points. This catches a missing halo exchange in the step, or a
// module that reads too few ghost cells.
TEST(DynUnsplitRK2Test, WindDrivenStepsSameOnSixBoxesAsOnOne) {
  WindCase one(/*n_boxes=*/1);
  WindCase six(/*n_boxes=*/6);
  for (WindCase *c : {&one, &six}) {
    for (int n = 0; n < 2; ++n) {
      c->dyn.step(c->state, c->visc, /*dt=*/300.0, c->forces, /*x_first=*/true);
    }
  }
  ASSERT_GT(one.state.u.norminf(), 0.0);
  expect_same_values(six.state.u, one.state.u, "u");
  expect_same_values(six.state.v, one.state.v, "v");
  expect_same_values(six.state.h, one.state.h, "h");
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
