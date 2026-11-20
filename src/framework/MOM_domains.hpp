#pragma once
/// @file MOM_domains.hpp
/// @brief Runtime-parameter-driven construction of the model Domain.
///        The analogue of MOM6's MOM_domains module.

#include <string_view>

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.hpp"
#include "MOM_file_parser.hpp"

namespace MOM {

/// @brief Read the domain parameters (global extents, halos, connectivity)
/// and construct the Domain and its horizontal decomposition. The analogue
/// of MOM6's MOM_domains_init.
/// @param params Runtime parameters.
/// @return The constructed Domain.
/// @pre The infrastructure layer (MOM::Infra) is initialized.
Domain make_domain(RuntimeParams &params);

/// @brief Stop if a field has the wrong stagger or too few ghost cells for a
/// routine. Otherwise a view of the field would read the wrong points, with
/// no error.
/// @param mf The field.
/// @param stagger The stagger the routine expects.
/// @param halo The number of ghost cells the routine needs in i and j.
/// @param caller The routine's name, for the message.
/// @param name The field's name, for the message.
/// @throws logger::FatalError if the stagger is wrong or the halo is too narrow.
void check_field(const amrex::MultiFab &mf, Stagger stagger, int halo, std::string_view caller,
                 std::string_view name);

} // namespace MOM
