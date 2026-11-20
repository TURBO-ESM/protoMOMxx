#pragma once
/// @file MOM_CoriolisAdv.hpp
/// @brief The Coriolis and momentum advection accelerations: CAu and CAv.

#include <AMReX_MultiFab.H>

#include "MOM_file_parser.hpp"
#include "MOM_grid.hpp"
#include "MOM_porous_barrier.hpp"
#include "MOM_vertical_grid.hpp"
#include "mom_coriolisadv.hpp"

namespace MOM {

/// @brief The Coriolis and momentum advection terms: their options, and the
/// grids and open face fractions they read. make_coriolis_adv creates it, and
/// CorAdCalc() computes the accelerations. The analogue of MOM6's
/// CoriolisAdv_CS and the routines that take it.
///
/// It refers to the grids and the open face fractions, so they must outlive
/// it.
struct CoriolisAdv {
  const Grid &G;            ///< The horizontal grid.
  const VerticalGrid &GV;   ///< The vertical grid.
  const PorousBarrier &pbv; ///< The open fractions of the cell faces.
  CorAdv_opts_CS_C opts{};  ///< The options of the Coriolis and advection schemes.

  /// @brief The number of halo points the scheme uses: 4 for
  /// WENOVI7TH_PV_ENSTRO, 3 for WENOVI5TH_PV_ENSTRO and 2 otherwise. MOM6's
  /// CoriolisAdv_stencil.
  /// @return The stencil width.
  int stencil() const;

  /// @brief Compute the Coriolis and momentum advection accelerations, as
  /// MOM6's CorAdCalc does without open boundaries, Stokes drift or
  /// diagnostics.
  ///
  /// Exchange the halos of the inputs before the call. On a Cartesian grid,
  /// CAu = q*vh - d(KE)/dx and CAv = -q*uh - d(KE)/dy, where q is the
  /// potential vorticity and KE the kinetic energy. The routine sets CAu and
  /// CAv at every valid point and leaves their halos unchanged.
  /// @param u The zonal velocity [L T-1 ~> m s-1], at u points.
  /// @param v The meridional velocity [L T-1 ~> m s-1], at v points.
  /// @param h The layer thickness [H ~> m or kg m-2], at h points.
  /// @param uh The zonal volume flux, u*h*dy [H L2 T-1 ~> m3 s-1 or kg s-1],
  ///        at u points.
  /// @param vh The meridional volume flux, v*h*dx [H L2 T-1 ~> m3 s-1 or
  ///        kg s-1], at v points.
  /// @param CAu The zonal acceleration [L T-2 ~> m s-2], at u points.
  /// @param CAv The meridional acceleration [L T-2 ~> m s-2], at v points.
  /// @throws logger::FatalError if a field has the wrong stagger, or if a
  ///         field it reads, the grid's and the open face fractions
  ///         included, has fewer than stencil() ghost cells.
  void CorAdCalc(const amrex::MultiFab &u, const amrex::MultiFab &v, const amrex::MultiFab &h,
                 const amrex::MultiFab &uh, const amrex::MultiFab &vh, amrex::MultiFab &CAu,
                 amrex::MultiFab &CAv) const;
};

/// @brief Read the options of the Coriolis and advection schemes. The
/// analogue of MOM6's CoriolisAdv_init.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param pbv The open fractions of the cell faces.
/// @param params Runtime parameters.
/// @return The module, referring to G, GV and pbv.
/// @throws logger::FatalError if CORIOLIS_SCHEME, KE_SCHEME or PV_ADV_SCHEME
///         is not one of MOM6's values.
CoriolisAdv make_coriolis_adv(const Grid &G, const VerticalGrid &GV, const PorousBarrier &pbv,
                              RuntimeParams &params);

} // namespace MOM
