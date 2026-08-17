#pragma once
/// @file MOM_grid_initialize.h
/// @brief Computation of the horizontal grid metrics for the supported
///        GRID_CONFIG options. The analogue of MOM6's MOM_grid_initialize.F90
///        (set_grid_metrics_*); the land/sea mask initialization
///        (initialize_masks) lands here with the topography PR.

#include "MOM_domain_infra.h"
#include "MOM_grid_fields.h"

namespace MOM {

/// @brief Create the metric fields of a simple spherical grid
/// (GRID_CONFIG = "spherical") on the domain's decomposition and compute the
/// geographic locations, grid spacings, and cell areas, over the full grown
/// boxes (halos included). The analogue of MOM6's set_grid_metrics_spherical.
/// @param domain The computational domain the fields are created on.
/// @param spec The grid specification.
/// @return The computed grid fields (the Coriolis parameter is left to
///         planetary_rotation).
GridFields spherical_grid_fields(const Domain &domain, const GridSpec &spec);

} // namespace MOM
