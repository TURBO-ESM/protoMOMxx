#pragma once
/// @file MOM_hor_visc.hpp
/// @brief The horizontal viscosity: diffu and diffv.

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_grid.hpp"
#include "MOM_vertical_grid.hpp"
#include "mom_hor_visc.hpp"

namespace MOM {

/// @brief The horizontal viscosity: its options, the metric fields computed
/// from them and the grid, and the grids it reads. make_hor_visc creates it,
/// and horizontal_viscosity() computes the accelerations. The analogue of
/// MOM6's hor_visc_CS and the routines that take it, for the Laplacian
/// viscosity.
///
/// It refers to the grids, so they must outlive it.
struct HorVisc {
  const Grid &G;           ///< The horizontal grid.
  const VerticalGrid &GV;  ///< The vertical grid.
  hv_opts_CS_C opts{};     ///< The options, in the kernel's struct.

  // todo: consider grouping the metric fields in a struct held in a
  //       std::optional, set when there is any viscosity (LAPLACIAN or
  //       BIHARMONIC). Only inviscid runs, such as single-column ones, would
  //       leave it unset, so we opt for the simpler design of always creating
  //       the metric fields for the time being.
  // The metric fields of MOM6's hor_visc_init, each with one layer. They
  // exist only with LAPLACIAN, Kh_Max_xx and Kh_Max_xy only with BOUND_KH,
  // and Laplac2_const_xx and Laplac2_const_xy only with SMAGORINSKY_KH.
  amrex::MultiFab dx2h;          ///< dxT squared [L2 ~> m2].
  amrex::MultiFab dy2h;          ///< dyT squared [L2 ~> m2].
  amrex::MultiFab dx2q;          ///< dxBu squared [L2 ~> m2].
  amrex::MultiFab dy2q;          ///< dyBu squared [L2 ~> m2].
  amrex::MultiFab dx_dyT;        ///< dxT/dyT [nondim].
  amrex::MultiFab dy_dxT;        ///< dyT/dxT [nondim].
  amrex::MultiFab dx_dyBu;       ///< dxBu/dyBu [nondim].
  amrex::MultiFab dy_dxBu;       ///< dyBu/dxBu [nondim].
  amrex::MultiFab reduction_xx;  ///< The factor that reduces the stresses through
                                 ///< partly blocked faces, at h points [nondim].
  amrex::MultiFab reduction_xy;  ///< The same factor at q points [nondim].
  amrex::MultiFab grid_sp_h2;    ///< The harmonic mean of the squared grid spacings
                                 ///< at h points [L2 ~> m2].
  amrex::MultiFab Kh_bg_xx;      ///< The background Laplacian viscosity at h points
                                 ///< [L2 T-1 ~> m2 s-1].
  amrex::MultiFab Kh_bg_xy;      ///< The same at q points [L2 T-1 ~> m2 s-1].
  amrex::MultiFab Kh_Max_xx;     ///< The largest stable Laplacian viscosity at h
                                 ///< points [L2 T-1 ~> m2 s-1].
  amrex::MultiFab Kh_Max_xy;     ///< The same at q points [L2 T-1 ~> m2 s-1].
  amrex::MultiFab Laplac2_const_xx;  ///< The Smagorinsky constant times the squared
                                     ///< grid spacing at h points [L2 ~> m2].
  amrex::MultiFab Laplac2_const_xy;  ///< The same at q points [L2 ~> m2].

  /// @brief Compute the convergence of the horizontal viscous stresses, as
  /// MOM6's horizontal_viscosity does without open boundaries, MEKE, the
  /// thicknesses from the barotropic solver, stochastic terms or diagnostics.
  ///
  /// Exchange the halos of u, v and h before the call. The routine sets
  /// diffu and diffv at every valid point and leaves their halos unchanged.
  /// Without LAPLACIAN or BIHARMONIC, it leaves diffu and diffv unchanged, as
  /// MOM6 does.
  /// @param u The zonal velocity [L T-1 ~> m s-1], at u points.
  /// @param v The meridional velocity [L T-1 ~> m s-1], at v points.
  /// @param h The layer thickness [H ~> m or kg m-2], at h points.
  /// @param uh The zonal volume flux [H L2 T-1 ~> m3 s-1 or kg s-1], at u
  ///        points.
  /// @param vh The meridional volume flux [H L2 T-1 ~> m3 s-1 or kg s-1], at
  ///        v points.
  /// @param diffu The zonal acceleration [L T-2 ~> m s-2], at u points.
  /// @param diffv The meridional acceleration [L T-2 ~> m s-2], at v points.
  /// @throws logger::FatalError if a field has the wrong stagger, if u, v, h
  ///         or a grid field the kernel reads has fewer than 2 ghost cells, or
  ///         if a metric field that the options need was not created.
  void horizontal_viscosity(const amrex::MultiFab &u, const amrex::MultiFab &v,
                            const amrex::MultiFab &h, const amrex::MultiFab &uh,
                            const amrex::MultiFab &vh, amrex::MultiFab &diffu,
                            amrex::MultiFab &diffv) const;
};

/// @brief Read the parameters of the horizontal viscosity and compute its
/// metric fields. The analogue of MOM6's hor_visc_init, with ZB2020_init
/// before it.
/// @param domain The domain the metric fields are created on.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param params Runtime parameters.
/// @return The module, referring to G and GV.
/// @throws logger::FatalError for options that are not implemented yet:
///         USE_ZB2020, USE_MEKE, DEBUG, LEITH_KH, ANISOTROPIC_VISCOSITY,
///         BIHARMONIC, USE_KH_BG_2D and USE_GME. Also, as in MOM6, if
///         USE_QG_LEITH_VISC is true without a Leith viscosity, or
///         USE_LEITHY is true without both LAPLACIAN and BIHARMONIC.
HorVisc make_hor_visc(const Domain &domain, const Grid &G, const VerticalGrid &GV,
                      RuntimeParams &params);

} // namespace MOM
