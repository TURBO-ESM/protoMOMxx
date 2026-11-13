// Sanity tests for the surface forcing of the solo driver
// (config_src/drivers/solo_driver/MOM_surface_forcing.cpp).
//
// The forcing lives on the domain's decomposition, so the infra layer must be
// up, hence the main() below.

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "MOM_domain_infra.hpp"
#include "MOM_domains.hpp"
#include "MOM_fixed_initialization.hpp"
#include "MOM_forcing_type.hpp"
#include "MOM_grid.hpp"
#include "MOM_infra.hpp"
#include "MOM_surface_forcing.hpp"

namespace {

// A parameter file given relative to this test's directory.
std::string param_file(const std::string &rel_path) {
  return (std::filesystem::path(__FILE__).parent_path() / rel_path).string();
}

} // namespace

// The 2-gyre winds blow eastward everywhere, peak at twice TAUX_MAGNITUDE
// halfway between the southern and the northern edges (40N on this grid), and
// have no meridional component.
TEST(SurfaceForcing, TwoGyreWinds) {
  MOM::RuntimeParams params(param_file("MOM_param_files/MOM_input_2gyre"));
  const MOM::Domain domain = MOM::make_domain(params);
  const MOM::Grid grid = MOM::make_grid(domain, params);

  MOM::SurfaceForcing surface_forcing(params);
  MOM::MechForcing forces = MOM::make_mech_forcing(domain);
  surface_forcing.set_forcing(forces, grid);

  const double taux_magnitude = 0.1;  // [Pa] (the default)
  EXPECT_GT(forces.taux.min(0), 0.0);
  EXPECT_EQ(forces.taux.max(0), 2.0 * taux_magnitude);
  EXPECT_EQ(forces.tauy.norminf(0, 1, domain.nghost()), 0.0);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
