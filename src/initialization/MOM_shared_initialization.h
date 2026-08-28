#pragma once
/// @file MOM_shared_initialization.h
/// @brief Initialization code shared between configurations: currently the
///        planetary rotation, with the topography setup to follow. The
///        analogue of MOM6's MOM_shared_initialization.F90.

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.h"
#include "MOM_file_parser.h"

namespace MOM {

/// @brief The planetary rotation setup.
/// todo: The beta-plane configuration will extend this when it is implemented.
struct RotationSpec {
  amrex::Real omega = 7.2921e-5;  ///< The rotation rate of the Earth [T-1 ~> s-1].
};

/// @brief Read ROTATION and the parameters of the selected setup.
/// The analogue of MOM6's MOM_initialize_rotation.
/// @param domain The computational domain the field is created on.
/// @param geoLatBu The geographic latitude at q points [degrees_N].
/// @param params Runtime parameters.
/// @return The computed Coriolis parameter field [T-1 ~> s-1].
/// @throws logger::FatalError on an unsupported or unrecognized ROTATION.
amrex::MultiFab MOM_initialize_rotation(const Domain &domain,
                                        const amrex::MultiFab &geoLatBu,
                                        RuntimeParams &params);

/// @brief Create the Coriolis parameter field at q points and compute
/// f = 2 OMEGA sin(latitude), over the full grown boxes (halos included).
/// The analogue of MOM6's set_rotation_planetary.
/// @param domain The computational domain the field is created on.
/// @param geoLatBu The geographic latitude at q points [degrees_N].
/// @param spec The planetary rotation setup.
/// @return The computed Coriolis parameter field [T-1 ~> s-1].
amrex::MultiFab set_rotation_planetary(const Domain &domain,
                                       const amrex::MultiFab &geoLatBu,
                                       const RotationSpec &spec);

} // namespace MOM
