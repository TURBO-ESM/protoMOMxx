#pragma once
/// @file MOM_set_viscosity.hpp
/// @brief The setup of the vertical viscosities: the bottom boundary layer and
///        the viscous surface mixed layer.

#include <AMReX_MultiFab.H>
#include <AMReX_REAL.H>

#include "MOM_domain_infra.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_forcing_type.hpp"
#include "MOM_grid.hpp"
#include "MOM_porous_barrier.hpp"
#include "MOM_vertical_grid.hpp"
#include "MOM_visc_fields.hpp"

namespace MOM {

/// @brief The setup of the vertical viscosities: its options, and the grids
/// and open face fractions it reads. make_set_visc creates it,
/// set_viscous_BBL() sets the bottom boundary layer properties, and
/// set_viscous_ML() the viscous surface mixed layer. The analogue of MOM6's
/// set_visc_CS and the routines that take it.
///
/// It refers to the grids and the open face fractions, so they must outlive
/// it.
struct SetVisc {
  const Grid &G;            ///< The horizontal grid.
  const VerticalGrid &GV;   ///< The vertical grid.
  const PorousBarrier &pbv; ///< The open fractions of the cell faces.

  int answer_date = 0;            ///< The vintage of the expressions (SET_VISC_ANSWER_DATE).
  bool bottomdraglaw = true;      ///< If true, the bottom stress follows a drag law.
  bool body_force_drag = false;   ///< If true, the bottom drag is an explicit body force
                                  ///< over the bottom HBBL.
  bool linear_drag = false;       ///< If true, the drag law is cdrag*DRAG_BG_VEL*u.
  amrex::Real dz_bbl = 0.0;       ///< The thickness of the bottom boundary layer (HBBL) [Z ~> m].
  amrex::Real Hbbl = 0.0;         ///< The same thickness in thickness units [H ~> m or kg m-2].
  amrex::Real cdrag = 0.0;        ///< The bottom drag coefficient [nondim].
  amrex::Real drag_bg_vel = 0.0;  ///< The assumed or unresolved bottom velocity [L T-1 ~> m s-1].
  amrex::Real BBL_thick_min = 0.0;  ///< The minimum bottom boundary layer thickness [Z ~> m].
  amrex::Real Kv_BBL_min = 0.0;   ///< The minimum bottom boundary layer viscosity
                                  ///< [H Z T-1 ~> m2 s-1 or Pa s].
  bool correct_BBL_bounds = false;  ///< If true, use the correct bounds on the bottom
                                    ///< boundary layer thickness and viscosity.

  /// @brief Set the thickness, viscosity and turbulence velocity of the
  /// bottom boundary layer, as MOM6's set_viscous_BBL does.
  ///
  /// For now it is a stub that leaves visc unchanged, so the bottom boundary
  /// layer fields stay at 0.
  /// @param u The zonal velocity [L T-1 ~> m s-1], at u points.
  /// @param v The meridional velocity [L T-1 ~> m s-1], at v points.
  /// @param h The layer thickness [H ~> m or kg m-2], at h points.
  /// @param visc The viscosities and bottom boundary layer properties.
  /// @throws logger::FatalError if a field has the wrong stagger, or if u, v
  ///         or h has fewer than 1 ghost cell.
  void set_viscous_BBL(const amrex::MultiFab &u, const amrex::MultiFab &v,
                       const amrex::MultiFab &h, ViscFields &visc) const;

  /// @brief Set the viscous surface mixed layer, as MOM6's set_viscous_ML
  /// does. MOM6 returns at once without DYNAMIC_VISCOUS_ML or ice shelves.
  /// make_set_visc rejects the first, and the forcing has no ice shelves yet,
  /// so this does nothing.
  /// @param u The zonal velocity [L T-1 ~> m s-1], at u points.
  /// @param v The meridional velocity [L T-1 ~> m s-1], at v points.
  /// @param h The layer thickness [H ~> m or kg m-2], at h points.
  /// @param forces The mechanical forcing.
  /// @param dt The time step [T ~> s].
  /// @param visc The viscosities and bottom boundary layer properties.
  void set_viscous_ML(const amrex::MultiFab &u, const amrex::MultiFab &v,
                      const amrex::MultiFab &h, const MechForcing &forces, amrex::Real dt,
                      ViscFields &visc) const;
};

/// @brief Read the options of the vertical viscosity setup and create the
/// fields of visc they need, set to 0. The analogue of MOM6's set_visc_init.
/// @param domain The domain the fields are created on.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param pbv The open fractions of the cell faces.
/// @param params Runtime parameters.
/// @param visc The viscosities and bottom boundary layer properties. The
///        fields the options need are created in it.
/// @return The module, referring to G, GV and pbv.
/// @throws logger::FatalError for options that are not implemented yet:
///         DEBUG, ADIABATIC = False, CHANNEL_DRAG, DYNAMIC_VISCOUS_ML,
///         CDRAG_MAP, BBL_USE_TIDAL_BG and BBL_USE_EOS. Also, as in MOM6, if
///         USE_REGRIDDING is true without BBL_USE_EOS.
SetVisc make_set_visc(const Domain &domain, const Grid &G, const VerticalGrid &GV,
                      const PorousBarrier &pbv, RuntimeParams &params, ViscFields &visc);

} // namespace MOM
