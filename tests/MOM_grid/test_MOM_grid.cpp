// Sanity/smoke tests for the Grid class and the spherical metric, topography,
// and rotation setup (src/types/MOM_grid.cpp, src/initialization): construction on the
// double_gyre configuration, staggering, boundary alignment, and a few simple
// physical properties.
//
// A Grid requires the infra layer to be initialized, hence the main() below,
// which instantiates a MOM::Infra.

#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <gtest/gtest.h>

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.h"
#include "MOM_grid.h"
#include "MOM_grid_initialize.h"
#include "MOM_infra.h"
#include "MOM_logger.h"
#include "MOM_shared_initialization.h"

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
constexpr double MINIMUM_DEPTH = 1.0;    // [m]
constexpr double MAXIMUM_DEPTH = 2000.0; // [m]
constexpr double EDGE_DEPTH = 100.0;     // [m] (the MOM6 default)
constexpr int HALO = 2;

double deg2rad(const double deg) { return deg * std::numbers::pi / 180.0; }

// Compute-then-construct, as make_grid does past the parameter reading.
MOM::Grid make_spherical_grid(const MOM::Domain &domain, const MOM::GridExtents &extents,
                              const std::string &topo_config,
                              const std::optional<double> mask_depth = std::nullopt) {
  MOM::GridFields fields = MOM::spherical_grid_fields(domain, extents);
  const MOM::TopoSpec spec = {.min_depth = MINIMUM_DEPTH, .max_depth = MAXIMUM_DEPTH,
                              .mask_depth = mask_depth};
  fields.bathyT = MOM::initialize_topography_named(domain, topo_config, extents,
                                                   spec, fields.geoLatT,
                                                   fields.geoLonT);
  MOM::limit_topography(fields.bathyT, spec);
  domain.pass_var(fields.bathyT);
  MOM::set_masks(domain, fields, spec.mask_depth.value_or(spec.min_depth));
  fields.CoriolisBu =
      MOM::set_rotation_planetary(domain, fields.geoLatBu, {.omega = OMEGA});
  return MOM::Grid(std::move(fields));
}

// The value of a single-level field at point (i, j), searched over the local
// boxes grown by their ghost cells.
std::optional<double> value_at(const amrex::MultiFab &mf, const int i, const int j) {
  std::optional<double> value;
  for (amrex::MFIter mfi(mf); mfi.isValid(); ++mfi) {
    if (mfi.growntilebox().contains(amrex::IntVect(i, j, 0))) {
      value = mf.const_array(mfi)(i, j, 0);
    }
  }
  return value;
}

} // namespace

TEST(Grid, DoubleGyreGridSanity) {
  const MOM::Domain domain({.ni_global = NI, .nj_global = NJ,
                            .ni_halo = HALO, .nj_halo = HALO,
                            .reentrant_x = false});
  const MOM::Grid grid = make_spherical_grid(
      domain, {.south_lat = SOUTH_LAT, .len_lat = LEN_LAT, .west_lon = WEST_LON,
               .len_lon = LEN_LON, .rad_earth = RAD_EARTH},
      "spoon");

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

  // u points share q's longitudes and h's latitudes; v points share h's
  // longitudes and q's latitudes.
  EXPECT_DOUBLE_EQ(grid.geoLonCu().min(0), grid.geoLonBu().min(0));
  EXPECT_DOUBLE_EQ(grid.geoLonCu().max(0), grid.geoLonBu().max(0));
  EXPECT_DOUBLE_EQ(grid.geoLatCu().min(0), grid.geoLatT().min(0));
  EXPECT_DOUBLE_EQ(grid.geoLatCu().max(0), grid.geoLatT().max(0));
  EXPECT_DOUBLE_EQ(grid.geoLonCv().min(0), grid.geoLonT().min(0));
  EXPECT_DOUBLE_EQ(grid.geoLonCv().max(0), grid.geoLonT().max(0));
  EXPECT_DOUBLE_EQ(grid.geoLatCv().min(0), grid.geoLatBu().min(0));
  EXPECT_DOUBLE_EQ(grid.geoLatCv().max(0), grid.geoLatBu().max(0));

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

  // dx follows the latitude of its own point type, so u matches h and v
  // matches q; dy is the same constant at all four point types.
  EXPECT_DOUBLE_EQ(grid.dxCu().min(0), grid.dxT().min(0));
  EXPECT_DOUBLE_EQ(grid.dxCu().max(0), grid.dxT().max(0));
  EXPECT_DOUBLE_EQ(grid.dxCv().min(0), grid.dxBu().min(0));
  EXPECT_DOUBLE_EQ(grid.dxCv().max(0), grid.dxBu().max(0));
  EXPECT_DOUBLE_EQ(grid.dyCu().min(0), grid.dyT().min(0));
  EXPECT_DOUBLE_EQ(grid.dyCv().min(0), grid.dyT().min(0));
  EXPECT_DOUBLE_EQ(grid.dyBu().min(0), grid.dyT().min(0));

  // With dy uniform, the largest cell areas are exactly the products of the
  // dx and dy extremes.
  EXPECT_DOUBLE_EQ(grid.areaT().max(0), grid.dxT().max(0) * grid.dyT().max(0));
  EXPECT_DOUBLE_EQ(grid.areaBu().max(0), grid.dxBu().max(0) * grid.dyBu().max(0));

  // f = 2 OMEGA sin(lat) increases northward from the southern boundary,
  // where sin(30 degrees N) = 1/2 makes f equal to OMEGA itself.
  EXPECT_NEAR(grid.CoriolisBu().min(0), OMEGA, 1e-12 * OMEGA);
  EXPECT_GT(grid.CoriolisBu().max(0), OMEGA);
  EXPECT_LT(grid.CoriolisBu().max(0), 2.0 * OMEGA);

  // In spoon topography, the deep center analytically exceeds MAXIMUM_DEPTH,
  // so the depth limiting makes the maximum exactly MAXIMUM_DEPTH.
  EXPECT_GT(grid.bathyT().min(0), EDGE_DEPTH);
  EXPECT_DOUBLE_EQ(grid.bathyT().max(0), MAXIMUM_DEPTH);

  // Everything deeper than MINIMUM_DEPTH is ocean, so all valid cells are wet.
  EXPECT_DOUBLE_EQ(grid.mask2dT().min(0), 1.0);
}

// A flat topography is MAXIMUM_DEPTH at every valid cell, and beyond the
// closed boundaries the halo bathymetry is land at MOM6's conventional depth:
TEST(Grid, FlatTopographyAndClosedBoundaryHalos) {
  const MOM::Domain domain({.ni_global = NI, .nj_global = NJ,
                            .ni_halo = HALO, .nj_halo = HALO,
                            .reentrant_x = false});
  const MOM::Grid grid = make_spherical_grid(
      domain, {.south_lat = SOUTH_LAT, .len_lat = LEN_LAT, .west_lon = WEST_LON,
               .len_lon = LEN_LON, .rad_earth = RAD_EARTH},
      "flat");

  EXPECT_DOUBLE_EQ(grid.bathyT().min(0), MAXIMUM_DEPTH);
  EXPECT_DOUBLE_EQ(grid.bathyT().max(0), MAXIMUM_DEPTH);

  const auto west = value_at(grid.bathyT(), -1, NJ / 2);
  if (west) {
    EXPECT_DOUBLE_EQ(*west, 0.5 * MINIMUM_DEPTH);
  }

  // The land halos close the masks at the walls: wall faces and vertices are
  // masked out and their u-cell areas are zero; the interior is all ocean.
  const auto halo_cell = value_at(grid.mask2dT(), -1, NJ / 2);
  if (halo_cell) {
    EXPECT_DOUBLE_EQ(*halo_cell, 0.0);
  }
  const auto wall_face = value_at(grid.mask2dCu(), 0, NJ / 2);
  if (wall_face) {
    EXPECT_DOUBLE_EQ(*wall_face, 0.0);
  }
  const auto wall_vertex = value_at(grid.mask2dBu(), 0, NJ / 2);
  if (wall_vertex) {
    EXPECT_DOUBLE_EQ(*wall_vertex, 0.0);
  }
  const auto wall_area = value_at(grid.areaCu(), 0, NJ / 2);
  if (wall_area) {
    EXPECT_DOUBLE_EQ(*wall_area, 0.0);
  }
  EXPECT_DOUBLE_EQ(grid.mask2dT().min(0), 1.0);
  EXPECT_DOUBLE_EQ(grid.areaCu().max(0), grid.dxCu().max(0) * grid.dyCu().max(0));
}


// An explicit MASKING_DEPTH turns cells at least that shallow into land: the
// spoon shallows northward, so the north-center cell is land and the deep
// south-center cell is ocean. The face and vertex masks follow the h mask
// through the southwest stencils, checked everywhere.
TEST(Grid, MaskingDepthLandAndMaskStencils) {
  const MOM::Domain domain({.ni_global = NI, .nj_global = NJ,
                            .ni_halo = HALO, .nj_halo = HALO,
                            .reentrant_x = false});
  const double mask_depth = 500.0;  // [m]
  const MOM::Grid grid = make_spherical_grid(
      domain, {.south_lat = SOUTH_LAT, .len_lat = LEN_LAT, .west_lon = WEST_LON,
               .len_lon = LEN_LON, .rad_earth = RAD_EARTH},
      "spoon", mask_depth);

  const auto north = value_at(grid.mask2dT(), NI / 2, NJ - 1);
  if (north) {
    EXPECT_DOUBLE_EQ(*north, 0.0);
  }
  const auto south = value_at(grid.mask2dT(), NI / 2, 0);
  if (south) {
    EXPECT_DOUBLE_EQ(*south, 1.0);
  }

  for (amrex::MFIter mfi(grid.mask2dT()); mfi.isValid(); ++mfi) {
    const auto maskT = grid.mask2dT().const_array(mfi);
    const auto maskCu = grid.mask2dCu().const_array(mfi);
    const auto maskCv = grid.mask2dCv().const_array(mfi);
    const auto maskBu = grid.mask2dBu().const_array(mfi);
    // The valid q nodes of the box, which cover its valid u/v nodes too.
    const amrex::Box bx = amrex::convert(mfi.validbox(), amrex::IntVect(1, 1, 0));
    for (int j = bx.smallEnd(1); j <= bx.bigEnd(1); ++j) {
      for (int i = bx.smallEnd(0); i <= bx.bigEnd(0); ++i) {
        EXPECT_DOUBLE_EQ(maskCu(i, j, 0), maskT(i - 1, j, 0) * maskT(i, j, 0));
        EXPECT_DOUBLE_EQ(maskCv(i, j, 0), maskT(i, j - 1, 0) * maskT(i, j, 0));
        EXPECT_DOUBLE_EQ(maskBu(i, j, 0),
                         (maskCu(i, j - 1, 0) * maskCu(i, j, 0)) *
                         (maskCv(i - 1, j, 0) * maskCv(i, j, 0)));
      }
    }
  }
}

// On a zonally reentrant domain the halo longitudes keep MOM6's monotonic
// extrapolation rather than wrapping: the halo cell centers east of the wrap
// continue past 360 degrees (here up to 375 with 10-degree cells); wrapped
// values would stay below 360.
TEST(Grid, ReentrantXGeoLonKeepsMonotonicExtrapolation) {
  const int ni = 36, nj = 10;
  const MOM::Domain domain({.ni_global = ni, .nj_global = nj,
                            .ni_halo = HALO, .nj_halo = HALO,
                            .reentrant_x = true});
  ASSERT_TRUE(domain.reentrant_x());

  const double len_lon = 360.0;
  const MOM::Grid grid = make_spherical_grid(
      domain, {.south_lat = -60.0, .len_lat = 30.0, .west_lon = 0.0,
               .len_lon = len_lon, .rad_earth = RAD_EARTH},
      "flat");

  const double dlon = len_lon / ni;
  EXPECT_DOUBLE_EQ(grid.geoLonT().norminf(0, 1, domain.nghost()),
                   len_lon + (HALO - 0.5) * dlon);

  // The wrap halos of the bathymetry are exchanged, not extrapolated: beyond
  // the western edge they carry the (wet) values of the eastern cells, where
  // a closed boundary would hold the 0.5*MINIMUM_DEPTH land value.
  const auto west = value_at(grid.bathyT(), -1, nj / 2);
  if (west) {
    EXPECT_DOUBLE_EQ(*west, MAXIMUM_DEPTH);
  }

  // The wrap keeps the western edge faces open: their western neighbor is
  // the wet easternmost cell, where a closed boundary would mask them out.
  const auto west_face = value_at(grid.mask2dCu(), 0, nj / 2);
  if (west_face) {
    EXPECT_DOUBLE_EQ(*west_face, 1.0);
  }
}

// Unfilled extents cannot be consumed.
TEST(Grid, UnfilledExtentsAreFatal) {
  const MOM::Domain domain({.ni_global = NI, .nj_global = NJ,
                            .ni_halo = HALO, .nj_halo = HALO,
                            .reentrant_x = false});
  EXPECT_THROW(MOM::spherical_grid_fields(domain, {}), MOM::logger::FatalError);
}

// The Grid constructor checks that every field is created: the metric fields
// are computed here, but the topography and rotation fields are not created.
TEST(Grid, ConstructorRejectsMissingFields) {
  const MOM::Domain domain({.ni_global = NI, .nj_global = NJ,
                            .ni_halo = HALO, .nj_halo = HALO,
                            .reentrant_x = false});
  MOM::GridFields fields = MOM::spherical_grid_fields(
      domain, {.south_lat = SOUTH_LAT, .len_lat = LEN_LAT, .west_lon = WEST_LON,
               .len_lon = LEN_LON, .rad_earth = RAD_EARTH});
  EXPECT_THROW(MOM::Grid(std::move(fields)), MOM::logger::FatalError);
}

/// @brief Test-binary entry point: initialize GTest, bring up the
/// infrastructure layer, and run all tests.
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const MOM::Infra infra(argc, argv);
  return RUN_ALL_TESTS();
}
