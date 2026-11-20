#pragma once
/// @file MOM_vert_friction.hpp
/// @brief The vertical viscosity and the wind stress: the implicit vertical
///        friction applied to u and v.

#include <AMReX_MultiFab.H>
#include <AMReX_REAL.H>

#include "MOM_domain_infra.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_forcing_type.hpp"
#include "MOM_grid.hpp"
#include "MOM_vertical_grid.hpp"
#include "MOM_visc_fields.hpp"
#include "mom_vert_friction.hpp"

namespace MOM {

/// @brief The coupling coefficients across the interfaces and the effective
/// thicknesses at the velocity points, which vertvisc_coef sets and vertvisc
/// reads. MOM6 keeps them in vertvisc_CS as a_u, h_u, a_v and h_v.
struct ViscCoupling {
  amrex::MultiFab a_u;  ///< The u-drag coefficient across each interface (nk+1 layers)
                        ///< [H T-1 ~> m s-1 or Pa s m-1].
  amrex::MultiFab h_u;  ///< The effective layer thickness at u points [H ~> m or kg m-2].
  amrex::MultiFab a_v;  ///< The v-drag coefficient across each interface (nk+1 layers)
                        ///< [H T-1 ~> m s-1 or Pa s m-1].
  amrex::MultiFab h_v;  ///< The effective layer thickness at v points [H ~> m or kg m-2].
};

/// @brief Create the coupling coefficients and effective thicknesses, set to
/// 0, as MOM6's vertvisc_init allocates them.
/// @param domain The domain the fields are created on.
/// @param nk The number of layers.
/// @return The new fields.
ViscCoupling make_visc_coupling(const Domain &domain, int nk);

/// @brief The vertical viscosity: its options and the grids it reads.
/// make_vertvisc creates it, vertvisc_coef() sets the coupling coefficients,
/// and vertvisc() applies the viscosity and the wind stress. The analogue of
/// MOM6's vertvisc_CS and the routines that take it.
///
/// It refers to the grids, so they must outlive it.
struct VertVisc {
  const Grid &G;              ///< The horizontal grid.
  const VerticalGrid &GV;     ///< The vertical grid.
  coef_opts_CS coef_opts{};   ///< The options of vertvisc_coef, in the kernel's struct.
  vertvisc_opts_CS_C opts{};  ///< The options of vertvisc, in the kernel's struct.

  /// @brief Set the coupling coefficients across the interfaces and the
  /// effective thicknesses at the velocity points, as MOM6's vertvisc_coef
  /// does without open boundaries, ice shelves, GL90 or diagnostics.
  /// @param u The zonal velocity [L T-1 ~> m s-1], at u points.
  /// @param v The meridional velocity [L T-1 ~> m s-1], at v points.
  /// @param h The layer thickness [H ~> m or kg m-2], at h points, valid one
  ///        point into the halo.
  /// @param dz The vertical extent of the layers [Z ~> m], at h points, valid
  ///        one point into the halo.
  /// @param visc The viscosities and bottom boundary layer properties.
  /// @param dt The time step [T ~> s].
  /// @param coupling The coupling coefficients and effective thicknesses, set
  ///        at the valid u and v points.
  /// @throws logger::FatalError if a field has the wrong stagger or too few
  ///         ghost cells, or if a bottom boundary layer field that
  ///         BOTTOMDRAGLAW needs was not created.
  void vertvisc_coef(const amrex::MultiFab &u, const amrex::MultiFab &v,
                     const amrex::MultiFab &h, const amrex::MultiFab &dz,
                     const ViscFields &visc, amrex::Real dt, ViscCoupling &coupling) const;

  /// @brief Apply the vertical viscosity and the wind stress to u and v, with
  /// an implicit solve in each column, as MOM6's vertvisc does without open
  /// boundaries, ice shelves, GL90, Stokes drift or diagnostics. Velocities
  /// whose CFL number exceeds CFL_TRUNCATE are truncated.
  /// @param u The zonal velocity [L T-1 ~> m s-1], at u points, updated at
  ///        the valid points.
  /// @param v The meridional velocity [L T-1 ~> m s-1], at v points, updated
  ///        at the valid points.
  /// @param h The layer thickness [H ~> m or kg m-2], at h points, valid one
  ///        point into the halo.
  /// @param forces The mechanical forcing, for the wind stress.
  /// @param visc The viscosities and bottom boundary layer properties.
  /// @param dt The time step [T ~> s].
  /// @param coupling The coupling coefficients from vertvisc_coef.
  /// @throws logger::FatalError if a field has the wrong stagger or too few
  ///         ghost cells.
  void vertvisc(amrex::MultiFab &u, amrex::MultiFab &v, const amrex::MultiFab &h,
                const MechForcing &forces, const ViscFields &visc, amrex::Real dt,
                const ViscCoupling &coupling) const;
};

/// @brief Read the options of the vertical viscosity. The analogue of MOM6's
/// vertvisc_init.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param params Runtime parameters.
/// @return The module, referring to G and GV.
/// @throws logger::FatalError for options that are not implemented yet:
///         DYNAMIC_VISCOUS_ML, FIXED_DEPTH_LOTW_ML, LOTW_VISCOUS_ML_FLOOR,
///         U_TRUNC_FILE, V_TRUNC_FILE, DEBUG, USE_GL90_IN_SSW, READ_KD_GL90
///         and CFL_TRUNCATE_RAMP_TIME. Also, as in MOM6, if HMIX_STRESS is not
///         positive with DIRECT_STRESS, if USE_GL90_N2 is true without
///         USE_GL90_IN_SSW, or if STOKES_MIXING_COMBINED is true.
VertVisc make_vertvisc(const Grid &G, const VerticalGrid &GV, RuntimeParams &params);

} // namespace MOM
