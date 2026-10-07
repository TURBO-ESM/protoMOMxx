#pragma once
/// @file MOM_state_initialization.hpp
/// @brief Construction of the prognostic state of a new run: the
///        THICKNESS_CONFIG and VELOCITY_CONFIG dispatches and the setups
///        they select. The analogue of MOM6's MOM_initialize_state
///        (MOM_state_initialization.F90).

#include "MOM_domain_infra.hpp"
#include "MOM_file_parser.hpp"
#include "MOM_grid.hpp"
#include "MOM_state.hpp"
#include "MOM_vertical_grid.hpp"

namespace MOM {

/// @brief Create the prognostic fields and set the initial layer thicknesses
/// and velocities of a new run. The analogue of MOM6's MOM_initialize_state,
/// together with the allocation of u, v and h.
/// @param domain The computational domain the fields are created on.
/// @param grid The horizontal grid, which supplies the bottom depth.
/// @param vgrid The vertical grid, which supplies the layer count, the
///        maximum depth, and the minimum layer thickness.
/// @param params Runtime parameters.
/// @return The initialized State.
/// @pre The infrastructure layer (MOM::Infra) is initialized.
/// @throws logger::FatalError on an unsupported or unrecognized configuration.
State make_state(const Domain &domain, const Grid &grid,
                 const VerticalGrid &vgrid, RuntimeParams &params);

} // namespace MOM
