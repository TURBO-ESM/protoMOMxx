#pragma once
/// @file MOM_grid_initialize.h
/// @brief The horizontal grid metrics: the GRID_CONFIG dispatch, the parameter
///        reads of the selected configuration, and the computation of its
///        metric fields. The analogue of MOM6's MOM_grid_initialize.F90

#include "MOM_domain_infra.h"
#include "MOM_file_parser.h"
#include "MOM_grid_fields.h"

namespace MOM {

/// @brief The geographic extents of an analytic horizontal grid.
/// These are initialization data rather than grid state.
struct GridExtents {
  amrex::Real south_lat = 0.0;      ///< The southern latitude of the domain [degrees_N].
  amrex::Real len_lat = 0.0;        ///< The latitudinal length of the domain [degrees_N].
  amrex::Real west_lon = 0.0;       ///< The western longitude of the domain [degrees_E].
  amrex::Real len_lon = 0.0;        ///< The longitudinal length of the domain [degrees_E].
  amrex::Real rad_earth = 6.378e6;  ///< The radius of the Earth [L ~> m].
};

/// @brief Read the geographic extents of an analytic grid.
/// @param params Runtime parameters.
/// @return The extents read from the parameters.
/// @throws logger::FatalError on non-positive extents.
GridExtents read_grid_extents(RuntimeParams &params);

/// @brief Read GRID_CONFIG and the extents of the selected grid, and compute
/// grid metrics fields. The analogue of MOM6's set_grid_metrics.
/// @param domain The computational domain the fields are created on.
/// @param params Runtime parameters.
/// @return The computed metric fields.
/// @throws logger::FatalError on an unsupported, unrecognized, or invalid config
GridFields set_grid_metrics(const Domain &domain, RuntimeParams &params);

/// @brief Create the metric fields of a simple spherical grid
/// (GRID_CONFIG = "spherical") on the domain's decomposition and compute the
/// geographic locations, grid spacings, and cell areas, over the full grown
/// boxes (halos included). The analogue of MOM6's set_grid_metrics_spherical.
/// @param domain The computational domain the fields are created on.
/// @param extents The geographic extents of the grid.
/// @return The computed grid fields.
/// @throws logger::FatalError on non-positive extents.
GridFields spherical_grid_fields(const Domain &domain, const GridExtents &extents);

} // namespace MOM
