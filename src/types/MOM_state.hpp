#pragma once
/// @file MOM_state.hpp
/// @brief The prognostic state of a model instance: the horizontal velocities
///        and the layer thicknesses. The analogue of the u, v and h members
///        of MOM6's MOM_control_struct (MOM.F90).

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
/// State is a plain struct rather than a class. Following the C++ Core
/// Guidelines, a type is a class when it has an invariant to maintain, and a
/// struct when its members can vary independently (C.2), and a class should
/// not wrap its members in trivial getters and setters (C.131). The time
/// stepping writes u, v and h every step, so there is no invariant here, and
/// accessors returning modifiable references would hide nothing. By the same
/// rule, Grid (read-only after construction) and VerticalGrid (validated at
/// construction) are classes. Write access to the state is a matter for its
/// owner instead: Model keeps it private and exposes it read-only. make_state
/// (src/initialization) creates and fills the fields.
struct State {
  amrex::MultiFab u;  ///< The zonal velocity at u points [L T-1 ~> m s-1].
  amrex::MultiFab v;  ///< The meridional velocity at v points [L T-1 ~> m s-1].
  amrex::MultiFab h;  ///< The layer thickness at h points [H ~> m or kg m-2].
};

} // namespace MOM
