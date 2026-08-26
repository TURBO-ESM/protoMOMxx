#pragma once
/// @file MOM_grid_fields.h
/// @brief The construction-phase counterpart of the horizontal grid: the
///        grid specification read from the runtime parameters, and the
///        struct of grid fields that src/initialization computes and the
///        Grid constructor takes over. The analogue of MOM6's
///        MOM_dyn_horgrid (dyn_horgrid_type).

#include <AMReX_MultiFab.H>

namespace MOM {

// A note on unit descriptions in comments: MOM6 rescales units at runtime for
// dimensional consistency testing and annotates them like "[L ~> m]".
// protoMOMxx doesn't implement a unit scaling yet, so all values are in the
// MKS units on the right-hand side of "~>".

/// @brief The construction specification of a horizontal grid: the geographic
/// extents of a simple spherical grid (GRID_CONFIG = "spherical") and the
/// planetary rotation rate. Other grid configurations (mosaic, cartesian,
/// mercator) will extend this specification when they are implemented.
struct GridSpec {
  amrex::Real south_lat = 0.0;      ///< The southern latitude of the domain [degrees_N].
  amrex::Real len_lat = 0.0;        ///< The latitudinal length of the domain [degrees_N].
  amrex::Real west_lon = 0.0;       ///< The western longitude of the domain [degrees_E].
  amrex::Real len_lon = 0.0;        ///< The longitudinal length of the domain [degrees_E].
  amrex::Real rad_earth = 6.378e6;  ///< The radius of the Earth [L ~> m].
  amrex::Real omega = 7.2921e-5;    ///< The rotation rate of the Earth [T-1 ~> s-1].
};

/// @brief The grid fields (the metrics at the four C-grid point types and the
/// Coriolis parameter) and the geographic extents they were computed from.
/// The analogue of MOM6's dyn_horgrid_type.
///
/// This is the construction-phase counterpart of Grid: a plain
/// struct with no behavior of its own, so the setup functions in
/// src/initialization can fill it freely. Each field is empty until its
/// setup function creates and computes it. Once complete, the struct is
/// moved into the Grid constructor, which checks that every field is
/// created and becomes the read-only owner.
struct GridFields {
  amrex::Real south_lat = 0.0;  ///< The southern latitude of the domain [degrees_N].
  amrex::Real len_lat = 0.0;    ///< The latitudinal length of the domain [degrees_N].
  amrex::Real west_lon = 0.0;   ///< The western longitude of the domain [degrees_E].
  amrex::Real len_lon = 0.0;    ///< The longitudinal length of the domain [degrees_E].
  amrex::Real rad_earth = 0.0;  ///< The radius of the Earth [L ~> m].

  amrex::MultiFab geoLatT;   ///< The geographic latitude at h points [degrees_N].
  amrex::MultiFab geoLonT;   ///< The geographic longitude at h points [degrees_E].
  amrex::MultiFab dxT;       ///< Delta x at h points [L ~> m].
  amrex::MultiFab dyT;       ///< Delta y at h points [L ~> m].
  amrex::MultiFab areaT;     ///< The area of an h-cell [L2 ~> m2].

  amrex::MultiFab geoLatCu;  ///< The geographic latitude at u points [degrees_N].
  amrex::MultiFab geoLonCu;  ///< The geographic longitude at u points [degrees_E].
  amrex::MultiFab dxCu;      ///< Delta x at u points [L ~> m].
  amrex::MultiFab dyCu;      ///< Delta y at u points [L ~> m].

  amrex::MultiFab geoLatCv;  ///< The geographic latitude at v points [degrees_N].
  amrex::MultiFab geoLonCv;  ///< The geographic longitude at v points [degrees_E].
  amrex::MultiFab dxCv;      ///< Delta x at v points [L ~> m].
  amrex::MultiFab dyCv;      ///< Delta y at v points [L ~> m].

  amrex::MultiFab geoLatBu;  ///< The geographic latitude at q points [degrees_N].
  amrex::MultiFab geoLonBu;  ///< The geographic longitude at q points [degrees_E].
  amrex::MultiFab dxBu;      ///< Delta x at q points [L ~> m].
  amrex::MultiFab dyBu;      ///< Delta y at q points [L ~> m].
  amrex::MultiFab areaBu;    ///< The area of a q-cell [L2 ~> m2].

  amrex::MultiFab CoriolisBu;  ///< The Coriolis parameter at q points [T-1 ~> s-1].
};

} // namespace MOM
