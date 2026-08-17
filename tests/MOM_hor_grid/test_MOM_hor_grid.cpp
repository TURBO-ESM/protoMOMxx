// Sanity/smoke tests for the HorGrid class (src/core/MOM_hor_grid.cpp):
// construction on the double_gyre configuration, staggering, boundary
// alignment, and a few simple physical properties.
//
// A HorGrid requires the infra layer to be initialized, hence the main()
// below, which instantiates a MOM::Infra.

#include <cmath>
#include <gtest/gtest.h>

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.h"
#include "MOM_hor_grid.h"
#include "MOM_infra.h"

namespace {

// The double_gyre spherical configuration (tests/double_gyre/MOM_input).
// Note dlon == dlat == 0.5 degrees, which some checks below rely on.
constexpr int NI = 44;
constexpr int NJ = 40;
constexpr double SOUTH_LAT = 30.0;  // [degrees_N]
constexpr double LEN_LAT = 20.0;    // [degrees_N]
constexpr double WEST_LON = 0.0;    // [degrees_E]
constexpr double LEN_LON = 22.0;    // [degrees_E]
constexpr double RAD_EARTH = 6.378e6;   // [m]
constexpr double OMEGA = 7.2921e-5;     // [s-1]
constexpr int HALO = 2;

double deg2rad(const double deg) { return deg * std::acos(-1.0) / 180.0; }

} // namespace

TEST(HorGrid, DoubleGyreGridSanity) {
  const MOM::Domain domain({.ni_global = NI, .nj_global = NJ,
                            .ni_halo = HALO, .nj_halo = HALO,
                            .reentrant_x = false});
  const MOM::HorGrid grid(domain, {.south_lat = SOUTH_LAT, .len_lat = LEN_LAT,
                                   .west_lon = WEST_LON, .len_lon = LEN_LON,
                                   .rad_earth = RAD_EARTH, .omega = OMEGA});

  // The spec scalars are retained.
  EXPECT_DOUBLE_EQ(grid.south_lat(), SOUTH_LAT);
  EXPECT_DOUBLE_EQ(grid.len_lat(), LEN_LAT);
  EXPECT_DOUBLE_EQ(grid.west_lon(), WEST_LON);
  EXPECT_DOUBLE_EQ(grid.len_lon(), LEN_LON);
  EXPECT_DOUBLE_EQ(grid.rad_earth(), RAD_EARTH);

  // Each field sits at its C-grid point type, observable as AMReX nodality.
  EXPECT_TRUE(grid.dxT().ixType().cellCentered());
  EXPECT_EQ(grid.dxCu().ixType(), amrex::IndexType(amrex::IntVect(1, 0, 0)));
  EXPECT_EQ(grid.dxCv().ixType(), amrex::IndexType(amrex::IntVect(0, 1, 0)));
  EXPECT_EQ(grid.CoriolisBu().ixType(), amrex::IndexType(amrex::IntVect(1, 1, 0)));

  // The corner coordinates align exactly with the domain boundaries, and the
  // cell-center coordinates lie strictly inside them.
  EXPECT_DOUBLE_EQ(grid.geoLatBu().min(0), SOUTH_LAT);
  EXPECT_DOUBLE_EQ(grid.geoLatBu().max(0), SOUTH_LAT + LEN_LAT);
  EXPECT_DOUBLE_EQ(grid.geoLonBu().min(0), WEST_LON);
  EXPECT_DOUBLE_EQ(grid.geoLonBu().max(0), WEST_LON + LEN_LON);
  EXPECT_GT(grid.geoLatT().min(0), SOUTH_LAT);
  EXPECT_LT(grid.geoLatT().max(0), SOUTH_LAT + LEN_LAT);

  // Halos extrapolate the coordinates beyond the boundaries (MOM6's convention)
  EXPECT_DOUBLE_EQ(grid.geoLatT().norminf(0, 1, domain.nghost()),
                   SOUTH_LAT + LEN_LAT + (HALO - 0.5) * (LEN_LAT / NJ));

  // dy is uniform on a spherical grid and matches R * dlat (in radians); dx
  // shrinks poleward, so it varies and (with dlon == dlat) stays below dy.
  EXPECT_DOUBLE_EQ(grid.dyT().min(0), grid.dyT().max(0));
  EXPECT_NEAR(grid.dyT().max(0), RAD_EARTH * deg2rad(LEN_LAT / NJ),
              1e-12 * grid.dyT().max(0));
  EXPECT_GT(grid.dxT().min(0), 0.0);
  EXPECT_LT(grid.dxT().min(0), grid.dxT().max(0));
  EXPECT_LT(grid.dxT().max(0), grid.dyT().max(0));

  // f = 2 OMEGA sin(lat) increases northward from the southern boundary,
  // where sin(30 degrees N) = 1/2 makes f equal to OMEGA itself.
  EXPECT_NEAR(grid.CoriolisBu().min(0), OMEGA, 1e-12 * OMEGA);
  EXPECT_GT(grid.CoriolisBu().max(0), OMEGA);
  EXPECT_LT(grid.CoriolisBu().max(0), 2.0 * OMEGA);
}

// On a zonally reentrant domain the halo longitudes keep MOM6's monotonic
// extrapolation rather than wrapping: the halo cell centers east of the wrap
// continue past 360 degrees (here up to 375 with 10-degree cells); wrapped
// values would stay below 360.
TEST(HorGrid, ReentrantXGeoLonKeepsMonotonicExtrapolation) {
  const int ni = 36, nj = 10;
  const MOM::Domain domain({.ni_global = ni, .nj_global = nj,
                            .ni_halo = HALO, .nj_halo = HALO,
                            .reentrant_x = true});
  ASSERT_TRUE(domain.reentrant_x());

  const double len_lon = 360.0;
  const MOM::HorGrid grid(domain, {.south_lat = -60.0, .len_lat = 30.0,
                                   .west_lon = 0.0, .len_lon = len_lon,
                                   .rad_earth = RAD_EARTH, .omega = OMEGA});

  const double dlon = len_lon / ni;
  EXPECT_DOUBLE_EQ(grid.geoLonT().norminf(0, 1, domain.nghost()),
                   len_lon + (HALO - 0.5) * dlon);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
