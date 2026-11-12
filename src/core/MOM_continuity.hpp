#pragma once
/// @file MOM_continuity.hpp
/// @brief The continuity solver: advances the layer thicknesses by calling
///        the PPM continuity kernel on every box of the domain.

#include <AMReX_MultiFab.H>
#include <AMReX_REAL.H>

#include "MOM_file_parser.hpp"
#include "MOM_grid.hpp"
#include "MOM_vertical_grid.hpp"
#include "mom_continuity_ppm.hpp"

namespace MOM {

/// @brief The continuity solver's options. The analogue of MOM6's
/// continuity_PPM_CS.
struct ContinuityCS {
  reconstruction_CS_C reconstruction{};      ///< The edge-value reconstruction options.
  transport_adjust_CS_C transport_adjust{};  ///< The transport adjustment options.
};

/// @brief Read the continuity solver's options, and check that the grid's
/// halo is wide enough for the solver. The analogue of MOM6's
/// continuity_PPM_init.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param params Runtime parameters.
/// @return The solver's options.
/// @throws logger::FatalError if the halo is narrower than continuity_stencil.
ContinuityCS continuity_init(const Grid &G, const VerticalGrid &GV, RuntimeParams &params);

/// @brief The number of halo points the continuity solver uses: 3 for PPM, 2
/// with simple_2nd and 1 with upwind_1st. MOM6's continuity_PPM_stencil.
/// @param CS The solver's options.
/// @return The stencil width.
int continuity_stencil(const ContinuityCS &CS);

/// @brief Advance the layer thicknesses by one time step, as MOM6's
/// continuity() does in the unsplit time steppers (PPM, no open boundaries,
/// no barotropic coupling).
///
/// Exchange the halos of u, v and hin before the call. Every field needs at
/// least continuity_stencil(CS) ghost cells in i and j, because the kernel
/// fills part of the halo itself instead of exchanging between its zonal and
/// meridional steps.
/// @param u The zonal velocity [L T-1 ~> m s-1], at u points.
/// @param v The meridional velocity [L T-1 ~> m s-1], at v points.
/// @param hin The initial layer thickness [H ~> m or kg m-2], at h points.
/// @param h The final layer thickness [H ~> m or kg m-2], at h points.
/// @param uh The zonal volume flux, u*h*dy [H L2 T-1 ~> m3 s-1 or kg s-1],
///        at u points.
/// @param vh The meridional volume flux, v*h*dx [H L2 T-1 ~> m3 s-1 or
///        kg s-1], at v points.
/// @param dt The time step [T ~> s].
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param CS The solver's options.
/// @param por_face_areaU The fractional open area of the u faces [nondim].
/// @param por_face_areaV The fractional open area of the v faces [nondim].
/// @param x_first If true, advect zonally first. MOM6 takes this from
///        G%first_direction.
void continuity(const amrex::MultiFab &u, const amrex::MultiFab &v,
                const amrex::MultiFab &hin, amrex::MultiFab &h,
                amrex::MultiFab &uh, amrex::MultiFab &vh, amrex::Real dt,
                const Grid &G, const VerticalGrid &GV, const ContinuityCS &CS,
                const amrex::MultiFab &por_face_areaU,
                const amrex::MultiFab &por_face_areaV, bool x_first);

} // namespace MOM
