#pragma once
/// @file MOM_grid_initialize.hpp
/// @brief The horizontal grid metrics: the GRID_CONFIG dispatch, the parameter
///        reads of the selected configuration, and the computation of its
///        metric fields. Also the land/sea masks, set from the bottom depth.
///        The analogue of MOM6's MOM_grid_initialize.F90

#include "MOM_domain_infra.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_grid_extents.hpp"
#include "MOM_grid_fields.hpp"

namespace MOM {

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

/// @brief Read the masking depths and set the land/sea masks and the
/// mask-dependent open face lengths and u/v-cell areas. The analogue of
/// MOM6's initialize_masks.
/// @param domain The computational domain the fields are created on.
/// @param fields The grid fields. Requires bathyT and the u/v grid
///        spacings; creates the masks, dy_Cu/dx_Cv, areaCu/areaCv and
///        IareaCu/IareaCv.
/// @param params Runtime parameters.
void initialize_masks(const Domain &domain, GridFields &fields,
                      RuntimeParams &params);

/// @brief Set the land/sea masks (mask2dT/Cu/Cv/Bu) from the bottom depth,
/// with points no deeper than the masking depth masked as land, and the
/// open face lengths (dy_Cu/dx_Cv), u/v-cell areas (areaCu/areaCv) and their
/// reciprocals (IareaCu/IareaCv), which are zero across land faces.
/// @param domain The computational domain the fields are created on.
/// @param fields The grid fields. Requires bathyT and the u/v grid
///        spacings; creates the masks, dy_Cu/dx_Cv, areaCu/areaCv and
///        IareaCu/IareaCv.
/// @param mask_depth The depth at or below which a point is land [Z ~> m].
void set_masks(const Domain &domain, GridFields &fields,
               amrex::Real mask_depth);

/// @brief Compute the reciprocals of the grid spacings and areas, with 0 where
/// the spacing or area is 0, over the grown boxes (halos included). Negative
/// spacings are set to 0 first, and a q-cell area that is not positive is set
/// to dxBu*dyBu, as in MOM6. IdxCu_OBCmask and IdyCv_OBCmask are also 0 at
/// land faces.
/// @param domain The computational domain the fields are created on.
/// @param fields The grid fields. Requires the spacings and areas at h, u, v
///        and q points and the u and v masks; creates IdxT, IdyT, IareaT,
///        IdxCu, IdyCu, IdxCu_OBCmask, IdxCv, IdyCv, IdyCv_OBCmask, IareaBu,
///        IdxBu and IdyBu.
void set_derived_metrics(const Domain &domain, GridFields &fields);

} // namespace MOM
