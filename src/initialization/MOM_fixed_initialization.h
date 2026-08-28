#pragma once
/// @file MOM_fixed_initialization.h
/// @brief Construction of the fixed (time-invariant) aspects of the model:
///        the horizontal grid metrics, the bottom topography, and the
///        planetary rotation, and later the land/sea masks. The analogue of MOM6's
///        MOM_initialize_fixed (MOM_fixed_initialization.F90).

#include "MOM_domain_infra.h"
#include "MOM_file_parser.h"
#include "MOM_grid.h"

namespace MOM {

/// @brief Read TOPO_CONFIG and set up the bottom depth.
/// @param domain The computational domain the field is created on.
/// @param geoLatT The geographic latitude at h points [degrees_N].
/// @param geoLonT The geographic longitude at h points [degrees_E].
/// @param params Runtime parameters.
/// @return The computed bottom depth field [Z ~> m].
/// @throws logger::FatalError on an unsupported or unrecognized TOPO_CONFIG.
amrex::MultiFab initialize_topography(const Domain &domain,
                                      const amrex::MultiFab &geoLatT,
                                      const amrex::MultiFab &geoLonT,
                                      RuntimeParams &params);

/// @brief Run the fixed-initialization setups in MOM6's order, each reading
/// its own runtime parameters and computing its fields on the domain's
/// decomposition, and construct the Grid from them. The analogue of MOM6's
/// MOM_initialize_fixed.
/// @param domain The computational domain the grid fields are created on.
/// @param params Runtime parameters.
/// @return The constructed Grid.
/// @pre The infrastructure layer (MOM::Infra) is initialized.
/// @throws logger::FatalError on an unsupported or an invalid configuration.
Grid make_grid(const Domain &domain, RuntimeParams &params);

} // namespace MOM
