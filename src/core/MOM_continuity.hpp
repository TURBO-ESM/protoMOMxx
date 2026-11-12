#pragma once
/// @file MOM_continuity.hpp
/// @brief The continuity solver: advances the layer thicknesses by calling
///        the PPM continuity kernel on every box of the domain.

#include <AMReX_MultiFab.H>
#include <AMReX_REAL.H>

#include "MOM_file_parser.hpp"
#include "MOM_grid.hpp"
#include "MOM_porous_barrier.hpp"
#include "MOM_vertical_grid.hpp"
#include "mom_continuity_ppm.hpp"

namespace MOM {

/// @brief The continuity solver: its options, and the grids and open face
/// fractions it reads. make_continuity creates it, and continuity() advances
/// the layer thicknesses. The analogue of MOM6's continuity_PPM_CS and the
/// routines that take it.
///
/// It refers to the grids and the open face fractions, so they must outlive
/// it.
struct Continuity {
  const Grid &G;            ///< The horizontal grid.
  const VerticalGrid &GV;   ///< The vertical grid.
  const PorousBarrier &pbv; ///< The open fractions of the cell faces.
  reconstruction_CS_C reconstruction{};      ///< The edge-value reconstruction options.
  transport_adjust_CS_C transport_adjust{};  ///< The transport adjustment options.

  /// @brief The number of halo points the solver uses: 3 for PPM, 2 with
  /// simple_2nd and 1 with upwind_1st. MOM6's continuity_PPM_stencil.
  /// @return The stencil width.
  int stencil() const;

  /// @brief Advance the layer thicknesses by one time step, as MOM6's
  /// continuity() does in the unsplit time steppers (PPM, no open boundaries,
  /// no barotropic coupling).
  ///
  /// Exchange the halos of u, v and hin before the call. Every field needs at
  /// least stencil() ghost cells in i and j, because the kernel fills part of
  /// the halo itself instead of exchanging between its zonal and meridional
  /// steps.
  /// @param u The zonal velocity [L T-1 ~> m s-1], at u points.
  /// @param v The meridional velocity [L T-1 ~> m s-1], at v points.
  /// @param hin The initial layer thickness [H ~> m or kg m-2], at h points.
  /// @param h The final layer thickness [H ~> m or kg m-2], at h points.
  /// @param uh The zonal volume flux, u*h*dy [H L2 T-1 ~> m3 s-1 or kg s-1],
  ///        at u points.
  /// @param vh The meridional volume flux, v*h*dx [H L2 T-1 ~> m3 s-1 or
  ///        kg s-1], at v points.
  /// @param dt The time step [T ~> s].
  /// @param x_first If true, advect zonally first. MOM6 takes this from
  ///        G%first_direction.
  /// @throws logger::FatalError if a field it reads, the grid's and the open
  ///         face fractions included, has the wrong stagger or fewer than
  ///         stencil() ghost cells.
  void continuity(const amrex::MultiFab &u, const amrex::MultiFab &v,
                  const amrex::MultiFab &hin, amrex::MultiFab &h,
                  amrex::MultiFab &uh, amrex::MultiFab &vh, amrex::Real dt,
                  bool x_first) const;
};

/// @brief Read the continuity solver's options, and check that the grid's
/// halo is wide enough for the solver. The analogue of MOM6's
/// continuity_PPM_init.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param pbv The open fractions of the cell faces.
/// @param params Runtime parameters.
/// @return The solver, referring to G, GV and pbv.
/// @throws logger::FatalError if the halo is narrower than the solver's
///         stencil.
Continuity make_continuity(const Grid &G, const VerticalGrid &GV, const PorousBarrier &pbv,
                           RuntimeParams &params);

} // namespace MOM
