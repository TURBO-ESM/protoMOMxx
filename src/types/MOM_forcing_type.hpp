#pragma once
/// @file MOM_forcing_type.hpp
/// @brief The mechanical forcing of the ocean surface. The analogue of MOM6's
///        mech_forcing type (MOM_forcing_type.F90).

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.hpp"

namespace MOM {

/// @brief The mechanical forcing of the ocean surface: the wind stress, with
/// taux at the u points and tauy at the v points. Both are 2-D fields, i.e.,
/// single-level MultiFabs indexed with k = 0. make_mech_forcing creates the fields.
struct MechForcing {
  amrex::MultiFab taux;  ///< The zonal wind stress at u points [R L Z T-2 ~> Pa].
  amrex::MultiFab tauy;  ///< The meridional wind stress at v points [R L Z T-2 ~> Pa].

  // defer: the other members of mech_forcing (ustar, tau_mag, p_surf, ...),
  //        with their first users.
};

/// @brief Create the mechanical forcing fields on the domain's decomposition,
/// set to zero, halos included. The analogue of MOM6's allocate_mech_forcing
/// with stress = .true.
/// @param domain The computational domain the fields are created on.
/// @return The new mechanical forcing.
/// @pre The infrastructure layer (MOM::Infra) is initialized.
MechForcing make_mech_forcing(const Domain &domain);

} // namespace MOM
