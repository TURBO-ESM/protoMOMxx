// Tests for the continuity solver, src/core/MOM_continuity.cpp.
//
// The tests run the solver on double_gyre's grids and initial thicknesses.
// The model starts from rest, which would leave h unchanged, so the tests set
// a made-up flow instead.

#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <AMReX_FArrayBox.H>
#include <AMReX_Gpu.H>
#include <AMReX_MFIter.H>
#include <AMReX_MultiFab.H>

#include "MOM_continuity.hpp"
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

/// @brief double_gyre's time step, DT in its MOM_input [T ~> s].
constexpr amrex::Real dt = 1200.0;

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

/// @brief Fill a velocity field, ghost cells included, with a flow that
/// varies in i, j and k and is zero on land faces.
/// @param vel The velocity field.
/// @param mask The land/sea mask at the same points.
/// @param phase Shifts the pattern, so that u and v differ.
void set_flow(amrex::MultiFab &vel, const amrex::MultiFab &mask, const amrex::Real phase) {
  for (amrex::MFIter mfi(vel); mfi.isValid(); ++mfi) {
    const amrex::Array4<amrex::Real> a = vel.array(mfi);
    const amrex::Array4<const amrex::Real> m = mask.const_array(mfi);
    amrex::ParallelFor(mfi.fabbox(), [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      a(i, j, k) = m(i, j, 0) * 0.2 * std::sin(0.7 * i + 0.3 * j + 1.1 * k + phase);
    });
  }
}

/// @brief double_gyre's grids and initial thicknesses on a domain split into
/// n_boxes, with a made-up flow in place of the state at rest.
struct Case {
  MOM::RuntimeParams params{double_gyre_input()};
  MOM::Domain domain;
  MOM::Grid G;
  MOM::VerticalGrid GV;
  MOM::State state;
  MOM::PorousBarrier pbv;  ///< The open face fractions: 1, as MOM6 sets them.
  MOM::Continuity continuity;
  amrex::MultiFab h, uh, vh;  ///< The solver's outputs, set to 0 at first.

  // continuity refers to this case's grids and open face fractions. A copy or
  // a move would leave it pointing at the original's.
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
        continuity(MOM::make_continuity(G, GV, pbv, params)),
        h(domain.make_h_field({.nk = GV.nk()})),
        uh(domain.make_u_field({.nk = GV.nk()})),
        vh(domain.make_v_field({.nk = GV.nk()})) {
    set_flow(state.u, G.mask2dCu(), 0.5);
    set_flow(state.v, G.mask2dCv(), 0.2);
    h.setVal(0.0);
    uh.setVal(0.0);
    vh.setVal(0.0);
  }

  /// Run the solver over one time step, zonal direction first.
  void run() {
    continuity.continuity(state.u, state.v, state.h, h, uh, vh, dt, /*x_first=*/true);
  }
};

/// @brief A copy of one box of a field, ghost cells included, numbered as in
/// MOM6: u(i) is storage point i+1. The test shifts the numbers itself rather
/// than calling mom_view, so that it checks mom_view.
amrex::FArrayBox mom6_numbered_copy(const amrex::MultiFab &mf, const amrex::MFIter &mfi) {
  amrex::FArrayBox copy(mf[mfi].box(), mf.nComp());
  copy.copy<amrex::RunOn::Device>(mf[mfi]);
  copy.shift(-mf.ixType().ixType());
  return copy;
}

/// @brief Expect one box of a field to match, ghost cells included, an array
/// numbered as in MOM6.
void expect_matches(const amrex::MultiFab &mf, const amrex::MFIter &mfi,
                    amrex::FArrayBox &expected, const std::string_view name) {
  expected.shift(mf.ixType().ixType());  // back to the storage numbering
  ASSERT_EQ(expected.box(), mf[mfi].box()) << name;
  amrex::FArrayBox diff(mf[mfi].box(), mf.nComp());
  diff.copy<amrex::RunOn::Device>(mf[mfi]);
  diff.minus<amrex::RunOn::Device>(expected);
  EXPECT_EQ(diff.maxabs<amrex::RunOn::Device>(0), 0.0) << name;
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

// On one box, the solver gives the same h, uh and vh, ghost cells included,
// as the kernel called directly on copies of the fields numbered as in MOM6.
// This catches a view or a box that is one point off.
TEST(ContinuityTest, MatchesKernelOnMOM6Numbering) {
  Case c(/*n_boxes=*/1);
  c.run();
  // The flow moves mass.
  ASSERT_GT(c.uh.norminf(), 0.0);
  ASSERT_GT(c.vh.norminf(), 0.0);

  for (amrex::MFIter mfi(c.state.h); mfi.isValid(); ++mfi) {
    const auto copy = [&](const amrex::MultiFab &mf) { return mom6_numbered_copy(mf, mfi); };
    const amrex::FArrayBox u = copy(c.state.u), v = copy(c.state.v), hin = copy(c.state.h);
    const amrex::FArrayBox mask2dT = copy(c.G.mask2dT()), dy_Cu = copy(c.G.dy_Cu()),
                           IareaT = copy(c.G.IareaT()), IdxT = copy(c.G.IdxT()),
                           areaT = copy(c.G.areaT()), dxT = copy(c.G.dxT()),
                           mask2dCu = copy(c.G.mask2dCu()), dxCu = copy(c.G.dxCu()),
                           dx_Cv = copy(c.G.dx_Cv()), IdyT = copy(c.G.IdyT()),
                           dyT = copy(c.G.dyT()), mask2dCv = copy(c.G.mask2dCv()),
                           dyCv = copy(c.G.dyCv()), por_u = copy(c.pbv.por_face_areaU),
                           por_v = copy(c.pbv.por_face_areaV);
    amrex::FArrayBox h(hin.box(), hin.nComp()), uh(u.box(), u.nComp()), vh(v.box(), v.nComp());
    h.setVal<amrex::RunOn::Device>(0.0);
    uh.setVal<amrex::RunOn::Device>(0.0);
    vh.setVal<amrex::RunOn::Device>(0.0);

    MOM::continuity_PPM(
        u.const_array(), v.const_array(), hin.const_array(), h.array(), uh.array(), vh.array(),
        dt, mfi.validbox(), c.continuity.stencil(), /*x_first=*/true,
        mask2dT.const_array(), dy_Cu.const_array(), IareaT.const_array(), IdxT.const_array(),
        areaT.const_array(), dxT.const_array(), mask2dCu.const_array(), dxCu.const_array(),
        dx_Cv.const_array(), IdyT.const_array(), dyT.const_array(), mask2dCv.const_array(),
        dyCv.const_array(), hin.box().smallEnd(0), hin.box().bigEnd(0), c.GV.Angstrom_H(),
        c.GV.H_subroundoff(), c.continuity.reconstruction, c.continuity.transport_adjust,
        /*obc=*/nullptr,
        por_u.const_array(), por_v.const_array(), {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
        {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {});
    amrex::Gpu::streamSynchronize();

    expect_matches(c.h, mfi, h, "h");
    expect_matches(c.uh, mfi, uh, "uh");
    expect_matches(c.vh, mfi, vh, "vh");
  }
}

// On six boxes, the solver gives the same h, uh and vh as on one box, at the
// valid points. Each box fills part of its own halo, so this catches a box
// that reads too few ghost cells or is given the wrong bounds.
TEST(ContinuityTest, SameOnSixBoxesAsOnOne) {
  Case one(/*n_boxes=*/1);
  Case six(/*n_boxes=*/6);
  one.run();
  six.run();
  expect_same_values(six.h, one.h, "h");
  expect_same_values(six.uh, one.uh, "uh");
  expect_same_values(six.vh, one.vh, "vh");
}

// A halo narrower than the PPM stencil of 3 is fatal, as in MOM6.
TEST(ContinuityTest, NarrowHaloIsFatal) {
  MOM::RuntimeParams params(double_gyre_input());
  const MOM::Domain domain({.ni_global = 44, .nj_global = 40, .ni_halo = 2, .nj_halo = 2});
  const MOM::Grid G = MOM::make_grid(domain, params);
  const MOM::VerticalGrid GV(params);
  const MOM::PorousBarrier pbv = MOM::make_porous_barrier(domain, GV.nk());
  EXPECT_THROW(MOM::make_continuity(G, GV, pbv, params), MOM::logger::FatalError);
}

// Passing a field with the wrong stagger is fatal. Otherwise the view would
// read it one point off, with no error.
TEST(ContinuityTest, WrongStaggerIsFatal) {
  MOM::RuntimeParams params(double_gyre_input());
  const MOM::Domain domain = MOM::make_domain(params);
  const MOM::Grid G = MOM::make_grid(domain, params);
  const MOM::VerticalGrid GV(params);
  const MOM::PorousBarrier pbv = MOM::make_porous_barrier(domain, GV.nk());
  const MOM::Continuity continuity = MOM::make_continuity(G, GV, pbv, params);
  const amrex::MultiFab h_as_u = domain.make_h_field({.nk = 1});
  const amrex::MultiFab v = domain.make_v_field({.nk = 1});
  const amrex::MultiFab hin = domain.make_h_field({.nk = 1});
  amrex::MultiFab h = domain.make_h_field({.nk = 1});
  amrex::MultiFab uh = domain.make_u_field({.nk = 1});
  amrex::MultiFab vh = domain.make_v_field({.nk = 1});
  EXPECT_THROW(continuity.continuity(h_as_u, v, hin, h, uh, vh, 1.0, /*x_first=*/true),
               MOM::logger::FatalError);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
