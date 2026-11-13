#pragma once
/// @file MOM_state.hpp
/// @brief The prognostic state of a model instance: the horizontal velocities
///        and the layer thicknesses. The analogue of the u, v and h members
///        of MOM6's MOM_control_struct (MOM.F90). It also holds the volume
///        fluxes uh and vh and their sums uhtr and vhtr, which MOM6 keeps in
///        the same struct.

#include <AMReX_MultiFab.H>

namespace MOM {

// A note on unit descriptions in comments: MOM6 rescales units at runtime for
// dimensional consistency testing and annotates them like "[L T-1 ~> m s-1]".
// protoMOMxx doesn't implement a unit scaling yet, so all values are in the
// MKS units on the right-hand side of "~>".

/// @brief The prognostic ocean state: u at the u points (west faces), v at
/// the v points (south faces), and h at the h points (cell centers), each
/// with nk layers on the domain's decomposition.
///
/// State is a plain struct rather than a class: the time stepping writes u, v
/// and h every step, so there is no invariant for a class to protect. Model
/// keeps it private and exposes it read-only. make_state (src/initialization)
/// creates and fills the fields.
struct State {
  amrex::MultiFab u;     ///< The zonal velocity at u points [L T-1 ~> m s-1].
  amrex::MultiFab v;     ///< The meridional velocity at v points [L T-1 ~> m s-1].
  amrex::MultiFab h;     ///< The layer thickness at h points [H ~> m or kg m-2].
  amrex::MultiFab uh;    ///< The zonal volume flux at u points
                         ///< [H L2 T-1 ~> m3 s-1 or kg s-1].
  amrex::MultiFab vh;    ///< The meridional volume flux at v points
                         ///< [H L2 T-1 ~> m3 s-1 or kg s-1].
  amrex::MultiFab uhtr;  ///< The zonal volume flux summed over the time steps
                         ///< since the last tracer advection [H L2 ~> m3 or kg].
  amrex::MultiFab vhtr;  ///< The meridional volume flux summed over the time
                         ///< steps since the last tracer advection [H L2 ~> m3 or kg].
};

} // namespace MOM
