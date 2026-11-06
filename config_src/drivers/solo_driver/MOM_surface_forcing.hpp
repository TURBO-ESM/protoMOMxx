#pragma once
/// @file MOM_surface_forcing.hpp
/// @brief The surface forcing of the solo driver, which sets the forcing
///        fields from analytic expressions or files in place of a coupler.
///        The analogue of MOM6's solo-driver MOM_surface_forcing.

#include <string>

#include <AMReX_REAL.H>

#include "MOM_file_parser.hpp"
#include "MOM_forcing_type.hpp"
#include "MOM_grid.hpp"

namespace MOM {

/// @class SurfaceForcing
/// @brief The surface forcing of the solo driver. The constructor reads the
/// forcing configuration, as MOM6's surface_forcing_init does, and set_forcing
/// sets the forcing fields for each forcing step.
///
/// The 2-gyre winds (WIND_CONFIG = "2gyre") are the only forcing implemented
/// so far. The buoyancy forcing is not implemented, and MOM6 skips it in
/// adiabatic runs, so the constructor requires ADIABATIC to be true.
class SurfaceForcing {
public:
  /// @brief Read the surface forcing configuration.
  /// @param params Runtime parameters.
  /// @throws logger::FatalError on a configuration that is not implemented yet
  ///         or not recognized.
  explicit SurfaceForcing(RuntimeParams &params);

  /// @brief Set the wind stress.
  /// @param forces The mechanical forcing to set.
  /// @param grid The horizontal grid, whose latitudes shape the analytic winds.
  /// @throws logger::FatalError on a wind configuration that is not
  ///         implemented yet.
  void set_forcing(MechForcing &forces, const Grid &grid);

private:
  std::string wind_config_;      ///< The wind forcing configuration (WIND_CONFIG).
  bool variable_winds_ = true;   ///< If true, the winds vary in time after the initialization.
  amrex::Real taux_mag_ = 0.0;   ///< The peak zonal wind stress of the 2-gyre winds [R L Z T-2 ~> Pa].
  amrex::Real south_lat_ = 0.0;  ///< The southern latitude of the domain [degrees_N].
  amrex::Real len_lat_ = 0.0;    ///< The latitudinal length of the domain [degrees_N].
  bool first_call_ = true;       ///< True until set_forcing is first called.
};

} // namespace MOM
