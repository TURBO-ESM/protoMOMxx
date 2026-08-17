#pragma once
/// @file MOM_shared_initialization.h
/// @brief Initialization code shared between configurations: currently the
///        planetary rotation, with the topography helpers to follow. The
///        analogue of MOM6's MOM_shared_initialization.F90.

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.h"
#include "MOM_grid_fields.h"

namespace MOM {

/// @brief Create the Coriolis parameter field at q points on the domain's
/// decomposition and compute f = 2 OMEGA sin(latitude) from the given q-point
/// latitudes, over the full grown boxes (halos included). The analogue of
/// MOM6's set_rotation_planetary.
/// @param domain The computational domain the field is created on.
/// @param spec The grid specification (the rotation rate).
/// @param geoLatBu The geographic latitude at q points [degrees_N].
/// @return The computed Coriolis parameter field [T-1 ~> s-1].
amrex::MultiFab planetary_rotation(const Domain &domain, const GridSpec &spec,
                                   const amrex::MultiFab &geoLatBu);

} // namespace MOM
