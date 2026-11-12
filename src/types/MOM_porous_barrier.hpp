#pragma once
/// @file MOM_porous_barrier.hpp
/// @brief The fractions of the cell faces that are open to the flow. The
///        analogue of MOM6's porous_barrier_type (MOM_variables.F90).

#include <AMReX_MultiFab.H>

#include "MOM_domain_infra.hpp"

namespace MOM {

/// @brief The fractions of the u and v faces that are open to the flow, each
/// with nk layers. Without porous barriers (USE_POROUS_BARRIER = False), they
/// are 1 everywhere. make_porous_barrier creates the fields.
struct PorousBarrier {
  amrex::MultiFab por_face_areaU;  ///< The open fraction of the u faces [nondim].
  amrex::MultiFab por_face_areaV;  ///< The open fraction of the v faces [nondim].

  // todo: por_layer_widthU and por_layer_widthV, the open fractions at the
  //       nk+1 interfaces, which come with set_visc, their first user.
};

/// @brief Create the open fractions of the cell faces on the domain's
/// decomposition, set to 1, halos included, as MOM6's initialize_MOM does.
/// @param domain The computational domain the fields are created on.
/// @param nk The number of layers.
/// @return The new open fractions.
/// @pre The infrastructure layer (MOM::Infra) is initialized.
PorousBarrier make_porous_barrier(const Domain &domain, int nk);

} // namespace MOM
