#pragma once
/// @file MOM_domains.h
/// @brief Runtime-parameter-driven construction of the model Domain.
///        The analogue of MOM6's MOM_domains module.

#include "MOM_domain_infra.h"
#include "MOM_file_parser.h"

namespace MOM {

/// @brief Read the domain parameters (global extents, halos, connectivity)
/// and construct the Domain and its horizontal decomposition. The analogue
/// of MOM6's MOM_domains_init.
/// @param params Runtime parameters.
/// @return The constructed Domain.
/// @pre The infrastructure layer (MOM::Infra) is initialized.
Domain make_domain(RuntimeParams &params);

} // namespace MOM
