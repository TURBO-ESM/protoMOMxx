#pragma once
/// @file MOM_interface_heights.hpp
/// @brief The vertical extent of the layers.

#include <AMReX_MultiFab.H>

#include "MOM_vertical_grid.hpp"

namespace MOM {

/// @brief Convert the layer thicknesses to their vertical extents, as MOM6's
/// thickness_to_dz does without the thermodynamic state (tv), on the valid
/// points and halo_size points into the halo.
/// @param h The layer thickness [H ~> m or kg m-2], at h points.
/// @param dz The vertical extent of the layers [Z ~> m], at h points.
/// @param GV The vertical grid.
/// @param halo_size The number of halo points to fill.
/// @throws logger::FatalError if h or dz is not at h points or has fewer than
///         halo_size ghost cells, or in non-Boussinesq mode, which needs the
///         specific volume of the thermodynamic state.
void thickness_to_dz(const amrex::MultiFab &h, amrex::MultiFab &dz, const VerticalGrid &GV,
                     int halo_size);

} // namespace MOM
