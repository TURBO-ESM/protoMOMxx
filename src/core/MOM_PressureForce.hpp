#pragma once
/// @file MOM_PressureForce.hpp
/// @brief The pressure gradient acceleration: PFu and PFv.

#include <AMReX_MultiFab.H>

#include "MOM_PressureForce_FV.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_grid.hpp"
#include "MOM_vertical_grid.hpp"

namespace MOM {

/// @brief The pressure gradient acceleration, which hands the work to the
/// finite volume form. make_pressure_force creates it, and pressure_force()
/// computes the acceleration. The analogue of MOM6's PressureForce_CS and the
/// routines that take it.
///
/// It refers to the grids, so they must outlive it.
struct PressureForce {
  const VerticalGrid &GV;            ///< The vertical grid.
  PressureForceFV PressureForce_FV;  ///< The finite volume form.

  /// @brief Compute the pressure gradient acceleration. The analogue of
  /// MOM6's PressureForce without an equation of state, an atmospheric
  /// pressure, or the outputs for the barotropic solver (pbce, eta).
  /// @param h The layer thickness [H ~> m], at h points.
  /// @param PFu The zonal acceleration [L T-2 ~> m s-2], at u points.
  /// @param PFv The meridional acceleration [L T-2 ~> m s-2], at v points.
  /// @throws logger::FatalError in non-Boussinesq mode, which is not
  ///         implemented yet.
  void pressure_force(const amrex::MultiFab &h, amrex::MultiFab &PFu,
                      amrex::MultiFab &PFv) const;
};

/// @brief Read the choice of the pressure gradient form and the options of
/// that form. The analogue of MOM6's PressureForce_init.
/// @param G The horizontal grid.
/// @param GV The vertical grid.
/// @param params Runtime parameters.
/// @return The module, referring to G and GV.
/// @throws logger::FatalError if ANALYTIC_FV_PGF is false, which selects the
///         Montgomery potential form that is not implemented yet.
PressureForce make_pressure_force(const Grid &G, const VerticalGrid &GV, RuntimeParams &params);

} // namespace MOM
