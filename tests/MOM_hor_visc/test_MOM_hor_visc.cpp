// Tests for the horizontal viscosity, src/parameterizations/lateral/MOM_hor_visc.cpp.
//
// The tests run horizontal_viscosity on double_gyre's grids and initial
// thicknesses, with made-up velocities.

#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <AMReX_MFIter.H>
#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.hpp"
#include "MOM_domains.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_fixed_initialization.hpp"
#include "MOM_grid.hpp"
#include "MOM_hor_visc.hpp"
#include "MOM_infra.hpp"
#include "MOM_logger.hpp"
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
/// where pattern varies in i, j and k. The field is zero on land faces.
void set_flow(amrex::MultiFab &vel, const amrex::MultiFab &mask, const amrex::Real value) {
  for (amrex::MFIter mfi(vel); mfi.isValid(); ++mfi) {
    const amrex::Array4<amrex::Real> a = vel.array(mfi);
    const amrex::Array4<const amrex::Real> m = mask.const_array(mfi);
    amrex::ParallelFor(mfi.fabbox(), [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      a(i, j, k) = m(i, j, 0) * value * std::sin(0.7 * i + 0.3 * j + 1.1 * k);
    });
  }
}

/// @brief Set a field to 1 at the point (i0, j0) of the top layer, and to 0
/// everywhere else.
void set_spike(amrex::MultiFab &vel, const int i0, const int j0) {
  vel.setVal(0.0);
  for (amrex::MFIter mfi(vel); mfi.isValid(); ++mfi) {
    const amrex::Array4<amrex::Real> a = vel.array(mfi);
    amrex::ParallelFor(mfi.fabbox(), [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      if (i == i0 && j == j0 && k == 0) a(i, j, k) = 1.0;
    });
  }
}

/// @brief double_gyre's grids, initial thicknesses and horizontal viscosity
/// on a domain split into n_boxes, with u, v, uh and vh set to 0 at first.
struct Case {
  MOM::RuntimeParams params{double_gyre_input()};
  MOM::Domain domain;
  MOM::Grid G;
  MOM::VerticalGrid GV;
  MOM::State state;
  MOM::HorVisc hor_visc;
  amrex::MultiFab diffu, diffv;  ///< The outputs, set to 0 at first.

  // hor_visc refers to this case's grids. A copy or a move would leave it
  // pointing at the original's.
  Case(const Case &) = delete;
  Case &operator=(const Case &) = delete;
  Case(Case &&) = delete;
  Case &operator=(Case &&) = delete;

  explicit Case(const int n_boxes)
      : domain(split_domain(params, n_boxes)),
        G(MOM::make_grid(domain, params)),
        GV(params),
        state(MOM::make_state(domain, G, GV, params)),
        hor_visc(MOM::make_hor_visc(domain, G, GV, params)),
        diffu(domain.make_u_field({.nk = GV.nk()})),
        diffv(domain.make_v_field({.nk = GV.nk()})) {
    state.u.setVal(0.0);
    state.v.setVal(0.0);
    state.uh.setVal(0.0);
    state.vh.setVal(0.0);
    diffu.setVal(0.0);
    diffv.setVal(0.0);
  }

  void run() {
    hor_visc.horizontal_viscosity(state.u, state.v, state.h, state.uh, state.vh, diffu, diffv);
  }
};

/// @brief The sum over the valid points of a times b, each point counted once.
amrex::Real dot(const amrex::MultiFab &a, const amrex::MultiFab &b) {
  amrex::MultiFab prod(a.boxArray(), a.DistributionMap(), a.nComp(), 0);
  amrex::MultiFab::Copy(prod, a, 0, 0, a.nComp(), 0);
  amrex::MultiFab::Multiply(prod, b, 0, 0, a.nComp(), 0);
  amrex::Real total = 0.0;
  for (int n = 0; n < prod.nComp(); ++n) total += prod.sum_unique(n);
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
TEST(HorViscTest, DoubleGyreSetsUp) {
  MOM::RuntimeParams params(double_gyre_input());
  const MOM::Domain domain = MOM::make_domain(params);
  const MOM::Grid G = MOM::make_grid(domain, params);
  const MOM::VerticalGrid GV(params);
  EXPECT_NO_THROW(MOM::make_hor_visc(domain, G, GV, params));
}

// With no flow there are no stresses, so both accelerations are 0.
TEST(HorViscTest, NoFlowGivesNoAcceleration) {
  Case c(/*n_boxes=*/1);
  c.diffu.setVal(1.0);
  c.diffv.setVal(1.0);
  c.run();
  EXPECT_EQ(c.diffu.norminf(), 0.0);
  EXPECT_EQ(c.diffv.norminf(), 0.0);
}

// Viscosity slows down a flow that is faster than its neighbors: a spike of u
// or v in the middle of the basin decelerates.
TEST(HorViscTest, SpikeDecelerates) {
  {
    Case c(/*n_boxes=*/1);
    set_spike(c.state.u, c.domain.ni_global() / 2, c.domain.nj_global() / 2);
    c.run();
    EXPECT_LT(dot(c.state.u, c.diffu), 0.0);
  }
  {
    Case c(/*n_boxes=*/1);
    set_spike(c.state.v, c.domain.ni_global() / 2, c.domain.nj_global() / 2);
    c.run();
    EXPECT_LT(dot(c.state.v, c.diffv), 0.0);
  }
}

// On six boxes, horizontal_viscosity gives the same diffu and diffv as on one
// box, at the valid points. This catches a box that reads too few ghost cells
// or is given the wrong bounds, in the kernel or in the metric fields.
TEST(HorViscTest, SameOnSixBoxesAsOnOne) {
  Case one(/*n_boxes=*/1);
  Case six(/*n_boxes=*/6);
  for (Case *c : {&one, &six}) {
    set_flow(c->state.u, c->G.mask2dCu(), 0.2);
    set_flow(c->state.v, c->G.mask2dCv(), -0.1);
    c->run();
  }
  ASSERT_GT(one.diffu.norminf(), 0.0);
  ASSERT_GT(one.diffv.norminf(), 0.0);
  expect_same_values(six.diffu, one.diffu, "diffu");
  expect_same_values(six.diffv, one.diffv, "diffv");
}

// Passing a field with the wrong stagger is fatal. Otherwise the view would
// read it one point off, with no error.
TEST(HorViscTest, WrongStaggerIsFatal) {
  Case c(/*n_boxes=*/1);
  const amrex::MultiFab h_as_u = c.domain.make_h_field({.nk = c.GV.nk()});
  EXPECT_THROW(c.hor_visc.horizontal_viscosity(h_as_u, c.state.v, c.state.h, c.state.uh,
                                               c.state.vh, c.diffu, c.diffv),
               MOM::logger::FatalError);
}

// A metric field that an option needs but that was not created is fatal.
// Otherwise the kernel would read an empty field.
TEST(HorViscTest, MissingMetricFieldIsFatal) {
  Case c(/*n_boxes=*/1);
  ASSERT_TRUE(c.hor_visc.opts.bound_Kh);
  c.hor_visc.Kh_Max_xx = amrex::MultiFab();
  EXPECT_THROW(c.run(), MOM::logger::FatalError);
}

// A thickness with fewer ghost cells than the stencil of 2 is fatal.
TEST(HorViscTest, NarrowHaloIsFatal) {
  Case c(/*n_boxes=*/1);
  const amrex::MultiFab h = c.domain.make_h_field({.nk = c.GV.nk(), .nghost_i = 1});
  EXPECT_THROW(c.hor_visc.horizontal_viscosity(c.state.u, c.state.v, h, c.state.uh, c.state.vh,
                                               c.diffu, c.diffv),
               MOM::logger::FatalError);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
