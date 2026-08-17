#pragma once
/// @file MOM_hor_grid.h
/// @brief The horizontal ocean grid of a model instance: the geographic
///        locations, grid spacings, and cell areas of the h/q/u/v points of
///        the Arakawa C-grid, and the Coriolis parameter. The analogue of
///        MOM6's MOM_grid (ocean_grid_type) together with the metric and
///        rotation setup of MOM_grid_initialize / MOM_shared_initialization.

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.h"

namespace MOM {

// A note on unit descriptions in comments: MOM6 rescales units at runtime for
// dimensional consistency testing and annotates them like "[L ~> m]".
// protoMOMxx doesn't implement a unit scaling yet, so all values are in the
// MKS units on the right-hand side of "~>".

/// @brief The construction specification of a HorGrid: the geographic extents
/// of a simple spherical grid (GRID_CONFIG = "spherical") and the planetary
/// rotation rate. Other grid configurations (mosaic, cartesian, mercator)
/// will extend this specification when they are implemented.
struct HorGridSpec {
  amrex::Real south_lat = 0.0;      ///< The southern latitude of the domain [degrees_N].
  amrex::Real len_lat = 0.0;        ///< The latitudinal length of the domain [degrees_N].
  amrex::Real west_lon = 0.0;       ///< The western longitude of the domain [degrees_E].
  amrex::Real len_lon = 0.0;        ///< The longitudinal length of the domain [degrees_E].
  amrex::Real rad_earth = 6.378e6;  ///< The radius of the Earth [L ~> m].
  amrex::Real omega = 7.2921e-5;    ///< The rotation rate of the Earth [T-1 ~> s-1].
};

/// @class HorGrid
/// @brief The horizontal grid: metric fields at the four C-grid point types
/// (h: cell centers, q: cell corners, u: east faces, v: north faces) and the
/// Coriolis parameter at q points.
///
/// protoMOMxx carries one horizontal grid type: MOM6's transient
/// dyn_horgrid_type / ocean_grid_type duplication (an artifact of static
/// -memory support and the rotated-grid verification machinery) is dropped.
class HorGrid {
public:
  /// @brief Construct the horizontal grid: define  and set the grid metrics
  /// and the planetary rotation. The analogues of MOM6's set_grid_metrics_*
  /// (MOM_grid_initialize.F90) and set_rotation_planetary
  /// (MOM_shared_initialization.F90).
  /// @param domain The computational domain: provides the decomposition the
  ///        metric fields are created on and the halo widths. Not retained.
  /// @param spec The grid specification; see HorGridSpec.
  /// @pre The infrastructure layer (MOM::Infra) is initialized.
  /// @pre The values are physically consistent (positive lengths and radius);
  HorGrid(const Domain &domain, const HorGridSpec &spec);

  /// @brief The southern latitude of the domain [degrees_N].
  /// @return The southern latitude.
  amrex::Real south_lat() const { return south_lat_; }

  /// @brief The latitudinal length of the domain [degrees_N].
  /// @return The latitudinal length.
  amrex::Real len_lat() const { return len_lat_; }

  /// @brief The western longitude of the domain [degrees_E].
  /// @return The western longitude.
  amrex::Real west_lon() const { return west_lon_; }

  /// @brief The longitudinal length of the domain [degrees_E].
  /// @return The longitudinal length.
  amrex::Real len_lon() const { return len_lon_; }

  /// @brief The radius of the Earth [L ~> m].
  /// @return The Earth radius.
  amrex::Real rad_earth() const { return rad_earth_; }

  /// @brief The geographic latitude at h (tracer) points [degrees_N].
  /// @return The h-point latitude field.
  const amrex::MultiFab &geoLatT() const { return geoLatT_; }

  /// @brief The geographic longitude at h (tracer) points [degrees_E].
  /// @return The h-point longitude field.
  const amrex::MultiFab &geoLonT() const { return geoLonT_; }

  /// @brief Delta x at h points [L ~> m].
  /// @return The h-point zonal grid spacing.
  const amrex::MultiFab &dxT() const { return dxT_; }

  /// @brief Delta y at h points [L ~> m].
  /// @return The h-point meridional grid spacing.
  const amrex::MultiFab &dyT() const { return dyT_; }

  /// @brief The area of an h-cell [L2 ~> m2].
  /// @return The h-cell area field.
  const amrex::MultiFab &areaT() const { return areaT_; }

  /// @brief The geographic latitude at u points [degrees_N].
  /// @return The u-point latitude field.
  const amrex::MultiFab &geoLatCu() const { return geoLatCu_; }

  /// @brief The geographic longitude at u points [degrees_E].
  /// @return The u-point longitude field.
  const amrex::MultiFab &geoLonCu() const { return geoLonCu_; }

  /// @brief Delta x at u points [L ~> m].
  /// @return The u-point zonal grid spacing.
  const amrex::MultiFab &dxCu() const { return dxCu_; }

  /// @brief Delta y at u points [L ~> m].
  /// @return The u-point meridional grid spacing.
  const amrex::MultiFab &dyCu() const { return dyCu_; }

  /// @brief The geographic latitude at v points [degrees_N].
  /// @return The v-point latitude field.
  const amrex::MultiFab &geoLatCv() const { return geoLatCv_; }

  /// @brief The geographic longitude at v points [degrees_E].
  /// @return The v-point longitude field.
  const amrex::MultiFab &geoLonCv() const { return geoLonCv_; }

  /// @brief Delta x at v points [L ~> m].
  /// @return The v-point zonal grid spacing.
  const amrex::MultiFab &dxCv() const { return dxCv_; }

  /// @brief Delta y at v points [L ~> m].
  /// @return The v-point meridional grid spacing.
  const amrex::MultiFab &dyCv() const { return dyCv_; }

  /// @brief The geographic latitude at q (corner) points [degrees_N].
  /// @return The q-point latitude field.
  const amrex::MultiFab &geoLatBu() const { return geoLatBu_; }

  /// @brief The geographic longitude at q (corner) points [degrees_E].
  /// @return The q-point longitude field.
  const amrex::MultiFab &geoLonBu() const { return geoLonBu_; }

  /// @brief Delta x at q points [L ~> m].
  /// @return The q-point zonal grid spacing.
  const amrex::MultiFab &dxBu() const { return dxBu_; }

  /// @brief Delta y at q points [L ~> m].
  /// @return The q-point meridional grid spacing.
  const amrex::MultiFab &dyBu() const { return dyBu_; }

  /// @brief The Coriolis parameter at q points [T-1 ~> s-1].
  /// @return The q-point Coriolis parameter field.
  const amrex::MultiFab &CoriolisBu() const { return CoriolisBu_; }

private:
  amrex::Real south_lat_ = 0.0;  ///< The southern latitude of the domain [degrees_N].
  amrex::Real len_lat_ = 0.0;    ///< The latitudinal length of the domain [degrees_N].
  amrex::Real west_lon_ = 0.0;   ///< The western longitude of the domain [degrees_E].
  amrex::Real len_lon_ = 0.0;    ///< The longitudinal length of the domain [degrees_E].
  amrex::Real rad_earth_ = 0.0;  ///< The radius of the Earth [L ~> m].

  amrex::MultiFab geoLatT_;   ///< The geographic latitude at h points [degrees_N].
  amrex::MultiFab geoLonT_;   ///< The geographic longitude at h points [degrees_E].
  amrex::MultiFab dxT_;       ///< Delta x at h points [L ~> m].
  amrex::MultiFab dyT_;       ///< Delta y at h points [L ~> m].
  amrex::MultiFab areaT_;     ///< The area of an h-cell [L2 ~> m2].

  amrex::MultiFab geoLatCu_;  ///< The geographic latitude at u points [degrees_N].
  amrex::MultiFab geoLonCu_;  ///< The geographic longitude at u points [degrees_E].
  amrex::MultiFab dxCu_;      ///< Delta x at u points [L ~> m].
  amrex::MultiFab dyCu_;      ///< Delta y at u points [L ~> m].

  amrex::MultiFab geoLatCv_;  ///< The geographic latitude at v points [degrees_N].
  amrex::MultiFab geoLonCv_;  ///< The geographic longitude at v points [degrees_E].
  amrex::MultiFab dxCv_;      ///< Delta x at v points [L ~> m].
  amrex::MultiFab dyCv_;      ///< Delta y at v points [L ~> m].

  amrex::MultiFab geoLatBu_;  ///< The geographic latitude at q points [degrees_N].
  amrex::MultiFab geoLonBu_;  ///< The geographic longitude at q points [degrees_E].
  amrex::MultiFab dxBu_;      ///< Delta x at q points [L ~> m].
  amrex::MultiFab dyBu_;      ///< Delta y at q points [L ~> m].

  amrex::MultiFab CoriolisBu_;  ///< The Coriolis parameter at q points [T-1 ~> s-1].

  /// @brief Set the geographic locations, grid spacings, and cell areas of a
  /// simple spherical grid, over the full grown boxes (halos included). The
  /// analogue of MOM6's set_grid_metrics_spherical (MOM_grid_initialize.F90).
  /// @param domain The computational domain (global extents).
  /// @param spec The grid specification.
  void set_grid_metrics_spherical(const Domain &domain, const HorGridSpec &spec);

  /// @brief Set the Coriolis parameter, f = 2 OMEGA sin(latitude), at q
  /// points from the already-set q-point latitudes. The analogue of MOM6's
  /// set_rotation_planetary (MOM_shared_initialization.F90).
  /// @param spec The grid specification (the rotation rate).
  void init_rotation(const HorGridSpec &spec);
};

} // namespace MOM
