// Tests for the setup of the vertical viscosities,
// src/parameterizations/vertical/MOM_set_viscosity.cpp.
//
// set_viscous_BBL is a stub until its kernel is ported, so these tests cover
// the setup: the fields it creates and the checks on the fields it reads.

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
#include "MOM_logger.hpp"
#include "MOM_porous_barrier.hpp"
#include "MOM_set_viscosity.hpp"
#include "MOM_state.hpp"
#include "MOM_state_initialization.hpp"
#include "MOM_vertical_grid.hpp"
#include "MOM_visc_fields.hpp"

namespace {

/// @brief The double_gyre parameter file.
std::string double_gyre_input() {
  return (std::filesystem::path(__FILE__).parent_path().parent_path() / "double_gyre" /
          "MOM_input")
      .string();
}

/// @brief double_gyre's grids, initial state and vertical viscosity setup.
struct Case {
  MOM::RuntimeParams params{double_gyre_input()};
  MOM::Domain domain;
  MOM::Grid G;
  MOM::VerticalGrid GV;
  MOM::State state;
  MOM::PorousBarrier pbv;
  MOM::ViscFields visc;
  MOM::SetVisc set_visc;

  // set_visc refers to this case's grids and open face fractions. A copy or a
  // move would leave it pointing at the original's.
  Case(const Case &) = delete;
  Case &operator=(const Case &) = delete;
  Case(Case &&) = delete;
  Case &operator=(Case &&) = delete;

  Case()
      : domain(MOM::make_domain(params)),
        G(MOM::make_grid(domain, params)),
        GV(params),
        state(MOM::make_state(domain, G, GV, params)),
        pbv(MOM::make_porous_barrier(domain, GV.nk())),
        set_visc(MOM::make_set_visc(domain, G, GV, pbv, params, visc)) {}
};

} // namespace

// double_gyre uses a bottom drag law, so the setup creates the bottom boundary
// layer fields, set to 0. It doesn't use DRAG_AS_BODY_FORCE, so the Rayleigh
// drag fields are not created.
TEST(SetViscTest, DoubleGyreCreatesTheBottomBoundaryLayerFields) {
  Case c;
  for (const amrex::MultiFab *mf : {&c.visc.bbl_thick_u, &c.visc.bbl_thick_v, &c.visc.kv_bbl_u,
                                    &c.visc.kv_bbl_v, &c.visc.ustar_BBL}) {
    ASSERT_FALSE(mf->empty());
    EXPECT_EQ(mf->norminf(0, mf->nComp(), mf->nGrowVect()), 0.0);
  }
  EXPECT_TRUE(c.visc.Ray_u.empty());
  EXPECT_TRUE(c.visc.Ray_v.empty());
}

// Passing a field with the wrong stagger to set_viscous_BBL is fatal.
TEST(SetViscTest, WrongStaggerIsFatal) {
  Case c;
  const amrex::MultiFab h_as_u = c.domain.make_h_field({.nk = c.GV.nk()});
  EXPECT_THROW(c.set_visc.set_viscous_BBL(h_as_u, c.state.v, c.state.h, c.visc),
               MOM::logger::FatalError);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
