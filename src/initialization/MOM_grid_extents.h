#pragma once
/// @file MOM_grid_extents.h
/// @brief The geographic extents of an analytic horizontal grid, shared by the
///        grid metric setup that computes the coordinates from them and the
///        named topography setup that shapes the bottom within them.

#include <AMReX_REAL.H>

namespace MOM {

/// @brief The geographic extents of an analytic horizontal grid. These are
/// members of MOM6's dyn_horgrid_type, but they are initialization data rather
/// than grid state, so they are not retained on the Grid.
struct GridExtents {
  amrex::Real south_lat = 0.0;      ///< The southern latitude of the domain [degrees_N].
  amrex::Real len_lat = 0.0;        ///< The latitudinal length of the domain [degrees_N].
  amrex::Real west_lon = 0.0;       ///< The western longitude of the domain [degrees_E].
  amrex::Real len_lon = 0.0;        ///< The longitudinal length of the domain [degrees_E].
  amrex::Real rad_earth = 6.378e6;  ///< The radius of the Earth [L ~> m].
};

} // namespace MOM
