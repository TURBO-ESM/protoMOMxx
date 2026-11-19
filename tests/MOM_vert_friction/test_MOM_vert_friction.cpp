// Tests for the vertical viscosity and the wind stress,
// src/parameterizations/vertical/MOM_vert_friction.cpp.
//
// The tests run vertvisc_coef and vertvisc on double_gyre's grids and initial
// thicknesses, with made-up velocities and wind stresses.

#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <AMReX_MFIter.H>
#include <AMReX_MultiFab.H>
#include <AMReX_ParallelDescriptor.H>
#include <AMReX_Reduce.H>

#include "MOM_domain_infra.hpp"
#include "MOM_domains.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_fixed_initialization.hpp"
#include "MOM_forcing_type.hpp"
#include "MOM_grid.hpp"
#include "MOM_infra.hpp"
#include "MOM_interface_heights.hpp"
#include "MOM_logger.hpp"
#include "MOM_porous_barrier.hpp"
#include "MOM_set_viscosity.hpp"
#include "MOM_state.hpp"
#include "MOM_state_initialization.hpp"
#include "MOM_vert_friction.hpp"
#include "MOM_vertical_grid.hpp"
#include "MOM_visc_fields.hpp"

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

/// @brief Fill a field, ghost cells included, with value*pattern, where
/// pattern varies in i, j and k.
void set_pattern(amrex::MultiFab &mf, const amrex::Real value) {
  for (amrex::MFIter mfi(mf); mfi.isValid(); ++mfi) {
    const amrex::Array4<amrex::Real> a = mf.array(mfi);
    amrex::ParallelFor(mfi.fabbox(), [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      a(i, j, k) = value * std::sin(0.7 * i + 0.3 * j + 1.1 * k);
    });
  }
}

/// @brief double_gyre's grids, initial thicknesses, viscosity setup and
/// vertical viscosity on a domain split into n_boxes, at rest and without
/// wind at first.
struct Case {
  MOM::RuntimeParams params{double_gyre_input()};
  MOM::Domain domain;
  MOM::Grid G;
  MOM::VerticalGrid GV;
  MOM::State state;
  MOM::PorousBarrier pbv;
  MOM::ViscFields visc;
  MOM::SetVisc set_visc;
  MOM::VertVisc vertvisc;
  MOM::MechForcing forces;
  MOM::ViscCoupling coupling;
  amrex::MultiFab dz;

  // set_visc and vertvisc refer to this case's grids and open face
  // fractions. A copy or a move would leave them pointing at the original's.
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
        set_visc(MOM::make_set_visc(domain, G, GV, pbv, params, visc)),
        vertvisc(MOM::make_vertvisc(G, GV, params)),
        forces(MOM::make_mech_forcing(domain)),
        coupling(MOM::make_visc_coupling(domain, GV.nk())),
        dz(domain.make_h_field({.nk = GV.nk()})) {}

  /// @brief Set the coupling coefficients and apply the vertical viscosity
  /// and the wind stress over one time step of double_gyre's 300 s.
  void run() {
    MOM::thickness_to_dz(state.h, dz, GV, /*halo_size=*/1);
    vertvisc.vertvisc_coef(state.u, state.v, state.h, dz, visc, /*dt=*/300.0, coupling);
    vertvisc.vertvisc(state.u, state.v, state.h, forces, visc, /*dt=*/300.0, coupling);
  }
};

/// @brief The sum of a field over the valid points of layer k, each point
/// counted once. For a single box only.
amrex::Real layer_sum(const amrex::MultiFab &mf, const int k) {
  amrex::ReduceOps<amrex::ReduceOpSum> op;
  amrex::ReduceData<amrex::Real> data(op);
  for (amrex::MFIter mfi(mf); mfi.isValid(); ++mfi) {
    amrex::Box bx = mfi.validbox();
    bx.setSmall(2, k);
    bx.setBig(2, k);
    const amrex::Array4<const amrex::Real> a = mf.const_array(mfi);
    op.eval(bx, data, [=] AMREX_GPU_DEVICE(int i, int j, int kk) -> amrex::GpuTuple<amrex::Real> {
      return {a(i, j, kk)};
    });
  }
  amrex::Real total = amrex::get<0>(data.value());
  amrex::ParallelDescriptor::ReduceRealSum(total);
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
TEST(VertViscTest, DoubleGyreSetsUp) {
  MOM::RuntimeParams params(double_gyre_input());
  const MOM::Domain domain = MOM::make_domain(params);
  const MOM::Grid G = MOM::make_grid(domain, params);
  const MOM::VerticalGrid GV(params);
  EXPECT_NO_THROW(MOM::make_vertvisc(G, GV, params));
}

// Without wind, a state at rest stays at rest.
TEST(VertViscTest, NoWindLeavesStateAtRest) {
  Case c(/*n_boxes=*/1);
  c.run();
  EXPECT_EQ(c.state.u.norminf(), 0.0);
  EXPECT_EQ(c.state.v.norminf(), 0.0);
}

// An eastward wind accelerates the water eastward, the top layer more than
// the bottom one.
TEST(VertViscTest, WindAcceleratesTheTopLayer) {
  Case c(/*n_boxes=*/1);
  c.forces.taux.setVal(0.1);
  c.run();
  const int bottom = c.GV.nk() - 1;
  EXPECT_GT(layer_sum(c.state.u, 0), 0.0);
  EXPECT_GE(layer_sum(c.state.u, bottom), 0.0);
  EXPECT_LT(layer_sum(c.state.u, bottom), layer_sum(c.state.u, 0));
}

// On six boxes, vertvisc_coef and vertvisc give the same u and v as on one
// box, at the valid points.
TEST(VertViscTest, SameOnSixBoxesAsOnOne) {
  Case one(/*n_boxes=*/1);
  Case six(/*n_boxes=*/6);
  for (Case *c : {&one, &six}) {
    set_pattern(c->state.u, 0.2);
    set_pattern(c->state.v, -0.1);
    set_pattern(c->forces.taux, 0.1);
    set_pattern(c->forces.tauy, 0.05);
    c->run();
  }
  ASSERT_GT(one.state.u.norminf(), 0.0);
  expect_same_values(six.state.u, one.state.u, "u");
  expect_same_values(six.state.v, one.state.v, "v");
}

// Passing a field with the wrong stagger is fatal. Otherwise the view would
// read it one point off, with no error.
TEST(VertViscTest, WrongStaggerIsFatal) {
  Case c(/*n_boxes=*/1);
  const amrex::MultiFab h_as_u = c.domain.make_h_field({.nk = c.GV.nk()});
  EXPECT_THROW(c.vertvisc.vertvisc_coef(h_as_u, c.state.v, c.state.h, c.dz, c.visc, 300.0,
                                        c.coupling),
               MOM::logger::FatalError);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
