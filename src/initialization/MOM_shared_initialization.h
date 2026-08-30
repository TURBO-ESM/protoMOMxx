#pragma once
/// @file MOM_shared_initialization.h
/// @brief Initialization code shared between configurations: the planetary
///        rotation and the named analytic topographies with their depth
///        limiting. The analogue of MOM6's MOM_shared_initialization.F90.

#include <optional>
#include <string_view>

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.h"
#include "MOM_file_parser.h"
#include "MOM_grid_extents.h"

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
amrex::MultiFab initialize_rotation(const Domain &domain,
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

/// @brief The named-topography setup.
struct TopoSpec {
  amrex::Real min_depth = 0.0;      ///< The minimum depth of the ocean [Z ~> m].
  amrex::Real max_depth = 0.0;      ///< The maximum depth of the ocean [Z ~> m]. No
                                    ///< meaningful default; must be set.
  std::optional<amrex::Real> mask_depth;  ///< The depth shallower than which a point is
                                    ///< masked as land; MINIMUM_DEPTH when empty [Z ~> m].
  amrex::Real edge_depth = 100.0;   ///< The depth at the edge of one of the named
                                    ///< topographies [Z ~> m].
  amrex::Real topog_slope_scale = 400000.0;  ///< The exponential decay scale used in
                                             ///< defining some of the named topographies [L ~> m].
};

/// @brief Read the parameters of the named topographies.
/// @param params Runtime parameters.
/// @param config The named topographic configuration ("flat" or "spoon").
/// @return The topography setup read from the parameters.
TopoSpec read_topo_spec(RuntimeParams &params, const std::string_view config);

/// @brief Read MINIMUM_DEPTH from runtime parameter file(s).
/// @param params Runtime parameters.
/// @return The minimum depth of the ocean [Z ~> m].
amrex::Real read_minimum_depth(RuntimeParams &params);

/// @brief Read MASKING_DEPTH from runtime parameter file(s).
/// @param params Runtime parameters.
/// @return The masking depth, or empty when set to -9999.0 (default).
std::optional<amrex::Real> read_masking_depth(RuntimeParams &params);

/// @brief Create the bottom depth field at h points and compute the named
/// analytic topography from the h-point coordinates.
/// @param domain The computational domain the field is created on.
/// @param config The named topographic configuration ("flat" or "spoon").
/// @param extents The geographic extents (the spoon is shaped by them).
/// @param spec The topography setup.
/// @param geoLatT The geographic latitude at h points [degrees_N].
/// @param geoLonT The geographic longitude at h points [degrees_E].
/// @return The computed bottom depth field [Z ~> m].
/// @throws logger::FatalError on an unset MAXIMUM_DEPTH or an unrecognized
///         topography name.
amrex::MultiFab initialize_topography_named(const Domain &domain,
                                            const std::string_view config,
                                            const GridExtents &extents,
                                            const TopoSpec &spec,
                                            const amrex::MultiFab &geoLatT,
                                            const amrex::MultiFab &geoLonT);

/// @brief Clamp the bottom depth in place so that ocean points satisfy
/// min_depth <= D <= max_depth.
/// @param bathyT The bottom depth field to clamp [Z ~> m].
/// @param spec The topography setup (the depth extents).
void limit_topography(amrex::MultiFab &bathyT, const TopoSpec &spec);

} // namespace MOM
