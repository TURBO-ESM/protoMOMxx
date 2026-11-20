// Tests for the pressure gradient acceleration, src/core/MOM_PressureForce.cpp
// and src/core/MOM_PressureForce_FV.cpp.
//
// The finite volume form is a stub until its kernel is ported, so these tests
// cover the setup and the checks on the fields.

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include <AMReX_MultiFab.H>

#include "MOM_PressureForce.hpp"
#include "MOM_domain_infra.hpp"
#include "MOM_domains.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_fixed_initialization.hpp"
#include "MOM_grid.hpp"
#include "MOM_infra.hpp"
#include "MOM_logger.hpp"
#include "MOM_vertical_grid.hpp"

namespace {

/// @brief The double_gyre parameter file.
std::string double_gyre_input() {
  return (std::filesystem::path(__FILE__).parent_path().parent_path() / "double_gyre" /
          "MOM_input")
      .string();
}

/// @brief double_gyre's grids, the pressure force module, and fields for its
/// inputs and outputs.
struct Case {
  MOM::RuntimeParams params{double_gyre_input()};
  MOM::Domain domain;
  MOM::Grid G;
  MOM::VerticalGrid GV;
  MOM::PressureForce pressure_force;
  amrex::MultiFab h, PFu, PFv;

  // pressure_force refers to this case's grids. A copy or a move would leave
  // it pointing at the original's.
  Case(const Case &) = delete;
  Case &operator=(const Case &) = delete;
  Case(Case &&) = delete;
  Case &operator=(Case &&) = delete;

  Case()
      : domain(MOM::make_domain(params)),
        G(MOM::make_grid(domain, params)),
        GV(params),
        pressure_force(MOM::make_pressure_force(G, GV, params)),
        h(domain.make_h_field({.nk = GV.nk()})),
        PFu(domain.make_u_field({.nk = GV.nk()})),
        PFv(domain.make_v_field({.nk = GV.nk()})) {
    h.setVal(1.0);
  }
};

} // namespace

// double_gyre's own settings set up without an error.
TEST(PressureForceTest, DoubleGyreSetsUp) {
  MOM::RuntimeParams params(double_gyre_input());
  const MOM::Domain domain = MOM::make_domain(params);
  const MOM::Grid G = MOM::make_grid(domain, params);
  const MOM::VerticalGrid GV(params);
  EXPECT_NO_THROW(MOM::make_pressure_force(G, GV, params));
}

// Passing a field with the wrong stagger is fatal. Otherwise the view would
// read it one point off, with no error.
TEST(PressureForceTest, WrongStaggerIsFatal) {
  Case c;
  amrex::MultiFab h_as_u = c.domain.make_h_field({.nk = c.GV.nk()});
  EXPECT_THROW(c.pressure_force.pressure_force(c.h, h_as_u, c.PFv), MOM::logger::FatalError);
}

// A thickness with no ghost cells is fatal, since the stencil needs 1.
TEST(PressureForceTest, NarrowHaloIsFatal) {
  Case c;
  const amrex::MultiFab h = c.domain.make_h_field({.nk = c.GV.nk(), .nghost_i = 0});
  EXPECT_THROW(c.pressure_force.pressure_force(h, c.PFu, c.PFv), MOM::logger::FatalError);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
