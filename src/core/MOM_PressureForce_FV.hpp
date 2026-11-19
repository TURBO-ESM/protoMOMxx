#pragma once
/// @file MOM_PressureForce_FV.hpp
/// @brief The finite volume pressure gradient acceleration, which integrates
///        the pressure analytically across each layer (Adcroft et al., 2008).

#include <AMReX_MultiFab.H>
#include <AMReX_REAL.H>

#include "MOM_file_parser.hpp"
#include "MOM_grid.hpp"
#include "MOM_vertical_grid.hpp"

namespace MOM {

/// @brief The finite volume pressure gradient: its options and the grids it
/// reads. make_pressure_force_FV creates it, and PressureForce_FV_Bouss()
/// computes the acceleration. The analogue of MOM6's PressureForce_FV_CS and
/// the routines that take it, without an equation of state.
///
/// It refers to the grids, so they must outlive it.
struct PressureForceFV {
  const Grid &G;           ///< The horizontal grid.
  const VerticalGrid &GV;  ///< The vertical grid.

  /// The reference density that is subtracted from the layer densities
  /// [R ~> kg m-3].
  amrex::Real rho_ref = 0.0;
  /// If true, interchange RHO_0 and the reference density in some places, as
  /// MOM6 does with RHO_PGF_REF_BUG.
  bool rho_ref_bug = false;
  /// The ratio of the reduced gravity at the surface to the gravitational
  /// acceleration, which scales the surface pressure gradient [nondim].
  amrex::Real GFS_scale = 1.0;

  /// @brief Compute the pressure gradient acceleration in Boussinesq mode.
  /// The analogue of MOM6's PressureForce_FV_Bouss without an equation of
  /// state, an atmospheric pressure, or the outputs for the barotropic solver
  /// (pbce, eta).
  ///
  /// For now it is a stub that sets PFu and PFv to 0.
  /// @param h The layer thickness [H ~> m], at h points.
  /// @param PFu The zonal acceleration [L T-2 ~> m s-2], at u points.
  /// @param PFv The meridional acceleration [L T-2 ~> m s-2], at v points.
  /// @throws logger::FatalError if a field has the wrong stagger, or if h has
  ///         fewer than 1 ghost cell.
  void PressureForce_FV_Bouss(const amrex::MultiFab &h, amrex::MultiFab &PFu,
                              amrex::MultiFab &PFv) const;
};

/// @brief Read the options of the finite volume pressure gradient. The
/// analogue of MOM6's PressureForce_FV_init.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param params Runtime parameters.
/// @return The module, referring to G and GV.
/// @throws logger::FatalError if TIDES, CALCULATE_SAL, USE_EOS or
///         USE_STANLEY_PGF is true, which are not implemented yet.
PressureForceFV make_pressure_force_FV(const Grid &G, const VerticalGrid &GV,
                                       RuntimeParams &params);

} // namespace MOM
