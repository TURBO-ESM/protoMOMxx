// Tests for the Coriolis and momentum advection terms, src/core/MOM_CoriolisAdv.cpp.
//
// The tests run CorAdCalc on double_gyre's grids and initial thicknesses,
// with made-up velocities and volume fluxes.

#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <AMReX_MFIter.H>
#include <AMReX_MultiFab.H>

#include "MOM_CoriolisAdv.hpp"
#include "MOM_domain_infra.hpp"
#include "MOM_domains.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_fixed_initialization.hpp"
#include "MOM_grid.hpp"
#include "MOM_infra.hpp"
#include "MOM_logger.hpp"
#include "MOM_porous_barrier.hpp"
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

/// @brief double_gyre's domain, split into n_boxes.
MOM::Domain split_domain(MOM::RuntimeParams &params, const int n_boxes) {
  const MOM::Domain domain = MOM::make_domain(params);
  return MOM::Domain({.ni_global = domain.ni_global(),
                      .nj_global = domain.nj_global(),
                      .ni_halo = domain.ni_halo(),
                      .nj_halo = domain.nj_halo(),
                      .n_boxes = n_boxes});
}

/// @brief Fill a u or v field, ghost cells included, with value*pattern,
/// where pattern varies in i, j and k if vary is true and is 1 otherwise. The
/// field is zero on land faces.
void set_flow(amrex::MultiFab &vel, const amrex::MultiFab &mask, const amrex::Real value,
              const bool vary) {
  for (amrex::MFIter mfi(vel); mfi.isValid(); ++mfi) {
    const amrex::Array4<amrex::Real> a = vel.array(mfi);
    const amrex::Array4<const amrex::Real> m = mask.const_array(mfi);
    amrex::ParallelFor(mfi.fabbox(), [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      const amrex::Real pattern = vary ? std::sin(0.7 * i + 0.3 * j + 1.1 * k) : 1.0;
      a(i, j, k) = m(i, j, 0) * value * pattern;
    });
  }
}

/// @brief double_gyre's grids and initial thicknesses on a domain split into
/// n_boxes, with u, v, uh and vh set to 0 at first.
struct Case {
  MOM::RuntimeParams params{double_gyre_input()};
  MOM::Domain domain;
  MOM::Grid G;
  MOM::VerticalGrid GV;
  MOM::State state;
  MOM::PorousBarrier pbv;  ///< The open face fractions: 1, as MOM6 sets them.
  MOM::CoriolisAdv coriolis_adv;
  amrex::MultiFab CAu, CAv;  ///< The outputs, set to 0 at first.

  // coriolis_adv refers to this case's grids and open face fractions. A copy
  // or a move would leave it pointing at the original's.
  Case(const Case &) = delete;
  Case &operator=(const Case &) = delete;
  Case(Case &&) = delete;
  Case &operator=(Case &&) = delete;

  explicit Case(const int n_boxes)
      : domain(split_domain(params, n_boxes)),
        G(MOM::make_grid(domain, params)),
        GV(params),
        state(MOM::make_state(domain, G, GV, params)),
        pbv(MOM::make_porous_barrier(domain, GV.nk())),
        coriolis_adv(MOM::make_coriolis_adv(G, GV, pbv, params)),
        CAu(domain.make_u_field({.nk = GV.nk()})),
        CAv(domain.make_v_field({.nk = GV.nk()})) {
    state.uh.setVal(0.0);
    state.vh.setVal(0.0);
    CAu.setVal(0.0);
    CAv.setVal(0.0);
  }

  void run() {
    coriolis_adv.CorAdCalc(state.u, state.v, state.h, state.uh, state.vh, CAu, CAv);
  }
};

/// @brief The sum of a field over its valid points, each counted once.
amrex::Real sum(const amrex::MultiFab &mf) {
  amrex::Real total = 0.0;
  for (int k = 0; k < mf.nComp(); ++k) total += mf.sum_unique(k);
  return total;
}

/// @brief Expect two fields on different boxes to have the same values at
/// their valid points.
void expect_same_values(const amrex::MultiFab &got, const amrex::MultiFab &expected,
                        const std::string_view name) {
  amrex::MultiFab diff(got.boxArray(), got.DistributionMap(), got.nComp(), 0);
  diff.ParallelCopy(expected);
  amrex::MultiFab::Subtract(diff, got, 0, 0, got.nComp(), 0);
  EXPECT_EQ(diff.norminf(), 0.0) << name;
}

} // namespace

// double_gyre's own settings set up without an error.
TEST(CoriolisAdvTest, DoubleGyreSetsUp) {
  MOM::RuntimeParams params(double_gyre_input());
  const MOM::Domain domain = MOM::make_domain(params);
  const MOM::Grid G = MOM::make_grid(domain, params);
  const MOM::VerticalGrid GV(params);
  const MOM::PorousBarrier pbv = MOM::make_porous_barrier(domain, GV.nk());
  EXPECT_NO_THROW(MOM::make_coriolis_adv(G, GV, pbv, params));
}

// With no flow there is no Coriolis force and no kinetic energy, so both
// accelerations are 0.
TEST(CoriolisAdvTest, NoFlowGivesNoAcceleration) {
  Case c(/*n_boxes=*/1);
  c.CAu.setVal(1.0);
  c.CAv.setVal(1.0);
  c.run();
  EXPECT_EQ(c.CAu.norminf(), 0.0);
  EXPECT_EQ(c.CAv.norminf(), 0.0);
}

// In the northern hemisphere, the Coriolis force turns a flow to its right:
// an eastward flow accelerates southward, and a northward flow eastward.
TEST(CoriolisAdvTest, FlowTurnsToTheRight) {
  {
    Case c(/*n_boxes=*/1);
    set_flow(c.state.u, c.G.mask2dCu(), 0.1, /*vary=*/false);
    set_flow(c.state.uh, c.G.mask2dCu(), 1.0e4, /*vary=*/false);
    c.run();
    EXPECT_LT(sum(c.CAv), 0.0);
  }
  {
    Case c(/*n_boxes=*/1);
    set_flow(c.state.v, c.G.mask2dCv(), 0.1, /*vary=*/false);
    set_flow(c.state.vh, c.G.mask2dCv(), 1.0e4, /*vary=*/false);
    c.run();
    EXPECT_GT(sum(c.CAu), 0.0);
  }
}

// On six boxes, CorAdCalc gives the same CAu and CAv as on one box, at the
// valid points. This catches a box that reads too few ghost cells or is given
// the wrong bounds.
TEST(CoriolisAdvTest, SameOnSixBoxesAsOnOne) {
  Case one(/*n_boxes=*/1);
  Case six(/*n_boxes=*/6);
  for (Case *c : {&one, &six}) {
    set_flow(c->state.u, c->G.mask2dCu(), 0.2, /*vary=*/true);
    set_flow(c->state.v, c->G.mask2dCv(), -0.1, /*vary=*/true);
    set_flow(c->state.uh, c->G.mask2dCu(), 3.0e4, /*vary=*/true);
    set_flow(c->state.vh, c->G.mask2dCv(), 2.0e4, /*vary=*/true);
    c->run();
  }
  ASSERT_GT(one.CAu.norminf(), 0.0);
  ASSERT_GT(one.CAv.norminf(), 0.0);
  expect_same_values(six.CAu, one.CAu, "CAu");
  expect_same_values(six.CAv, one.CAv, "CAv");
}

// Passing a field with the wrong stagger is fatal. Otherwise the view would
// read it one point off, with no error.
TEST(CoriolisAdvTest, WrongStaggerIsFatal) {
  Case c(/*n_boxes=*/1);
  const amrex::MultiFab h_as_u = c.domain.make_h_field({.nk = c.GV.nk()});
  EXPECT_THROW(
      c.coriolis_adv.CorAdCalc(h_as_u, c.state.v, c.state.h, c.state.uh, c.state.vh, c.CAu, c.CAv),
      MOM::logger::FatalError);
}

// A thickness with fewer ghost cells than the stencil of 2 is fatal.
TEST(CoriolisAdvTest, NarrowHaloIsFatal) {
  Case c(/*n_boxes=*/1);
  const amrex::MultiFab h = c.domain.make_h_field({.nk = c.GV.nk(), .nghost_i = 1});
  EXPECT_THROW(
      c.coriolis_adv.CorAdCalc(c.state.u, c.state.v, h, c.state.uh, c.state.vh, c.CAu, c.CAv),
      MOM::logger::FatalError);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
