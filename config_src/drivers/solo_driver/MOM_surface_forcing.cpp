/// @file MOM_surface_forcing.cpp
/// @brief Implementation of the surface forcing of the solo driver.

#include "MOM_surface_forcing.hpp"

#include <cmath>
#include <numbers>

#include "MOM_grid_initialize.hpp"
#include "MOM_logger.hpp"

namespace MOM {

namespace {

// Set the steady wind stress of two idealized gyres over the computational
// domain: a latitudinally varying zonal stress, and no meridional stress.
void wind_forcing_2gyre(MechForcing &forces, const Grid &grid,
                        const amrex::Real taux_mag, const amrex::Real south_lat,
                        const amrex::Real len_lat) {

  const amrex::Real PI = std::numbers::pi_v<amrex::Real>;

  for (amrex::MFIter mfi(forces.taux); mfi.isValid(); ++mfi) {
    const amrex::Box bx = amrex::makeSlab(mfi.validbox(), 2, 0);
    const amrex::Array4<amrex::Real> taux = forces.taux.array(mfi);
    const amrex::Array4<const amrex::Real> lat = grid.geoLatCu().const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      taux(i, j, 0) = taux_mag * (1.0 - std::cos(2.0 * PI * (lat(i, j, 0) - south_lat) / len_lat));
    });
  }

  forces.tauy.setVal(0.0);

  // defer: the friction velocity (stresses_to_ustar), with its first user.
}

} // namespace

SurfaceForcing::SurfaceForcing(RuntimeParams &params) {

  params.doc_module("MOM_surface_forcing", "");

  // MOM6 also reads ENABLE_THERMODYNAMICS, BOUSSINESQ, SEMI_BOUSSINESQ and
  // INPUTDIR here, for the buoyancy forcing, the friction velocity and the
  // file-based configurations, none of which is implemented yet.

  bool adiabatic = false;
  params.get("ADIABATIC", adiabatic,
             {.default_value = false,
              .desc = "There are no diapycnal mass fluxes if ADIABATIC is true. This assumes "
                      "that KD = KDML = 0.0 and that there is no buoyancy forcing, but makes the "
                      "model faster by eliminating subroutine calls."});
  if (!adiabatic) {
    // defer: the buoyancy forcing, which set_forcing applies when ADIABATIC
    //        is false, and the forcing type it fills.
    logger::fatal("SurfaceForcing: The buoyancy forcing (ADIABATIC = False) is not implemented yet.");
  }

  params.get("VARIABLE_WINDS", variable_winds_,
             {.default_value = true,
              .desc = "If true, the winds vary in time after the initialization."});

  // Read so that it is logged. MOM6 uses it in the buoyancy forcing only.
  bool variable_buoyforce = true;
  params.get("VARIABLE_BUOYFORCE", variable_buoyforce,
             {.default_value = true,
              .desc = "If true, the buoyancy forcing varies in time after the initialization "
                      "of the model."});

  std::string buoy_config = "zero";
  params.get("BUOY_CONFIG", buoy_config,
             {.default_value = std::string("zero"),
              .desc = "The character string that indicates how buoyancy forcing is specified.  "
                      "Valid options include (file), (data_override), (zero), (const), (linear), "
                      "(MESO), (SCM_CVmix_tests), (BFB), (dumbbell), (USER) and (NONE)."});
  if (buoy_config == "zero") {
    // No parameters to read. The zero buoyancy forcing is not applied, since
    // the run is adiabatic.
  } else if (buoy_config == "file" || buoy_config == "data_override" ||
             buoy_config == "const" || buoy_config == "linear" || buoy_config == "MESO" ||
             buoy_config == "SCM_CVmix_tests" || buoy_config == "BFB" ||
             buoy_config == "dumbbell" || buoy_config == "USER" || buoy_config == "NONE") {
    // defer: all other BUOY_CONFIG options
    logger::fatal("SurfaceForcing: BUOY_CONFIG \"", buoy_config, "\" is not implemented yet.");
  } else {
    logger::fatal("SurfaceForcing: Unrecognized buoy config \"", buoy_config, "\".");
  }

  wind_config_ = "zero";
  params.get("WIND_CONFIG", wind_config_,
             {.default_value = std::string("zero"),
              .desc = "The character string that indicates how wind forcing is specified.  "
                      "Valid options include (file), (data_override), (2gyre), (1gyre), (gyres), "
                      "(zero), (const), (Neverworld), (scurves), (ideal_hurr), (SCM_CVmix_tests) "
                      "and (USER)."});
  if (wind_config_ == "2gyre") {
    params.get("TAUX_MAGNITUDE", taux_mag_,
               {.default_value = 0.1,
                .desc = "The peak zonal wind stress when WIND_CONFIG = 2gyre.",
                .units = "Pa"});
    // MOM6 copies the extents from the grid, which carries them.
    const GridExtents extents = read_grid_extents(params);
    south_lat_ = extents.south_lat;
    len_lat_ = extents.len_lat;
  } else if (wind_config_ == "SCM_ideal_hurr") {
    // Retired in MOM6 itself; carry its message.
    logger::fatal("SurfaceForcing: WIND_CONFIG = \"SCM_ideal_hurr\" is a depricated option.  "
                  "To obtain mathematically equivalent results set WIND_CONFIG = \"ideal_hurr\", "
                  "IDL_HURR_SCM = True and IDL_HURR_X0 = 6.48e+05.");
  } else if (wind_config_ == "file" || wind_config_ == "data_override" ||
             wind_config_ == "1gyre" || wind_config_ == "gyres" || wind_config_ == "zero" ||
             wind_config_ == "const" || wind_config_ == "Neverworld" ||
             wind_config_ == "Neverland" || wind_config_ == "scurves" ||
             wind_config_ == "ideal_hurr" || wind_config_ == "SCM_CVmix_tests" ||
             wind_config_ == "USER") {
    // defer: all other WIND_CONFIG options
    logger::fatal("SurfaceForcing: WIND_CONFIG \"", wind_config_, "\" is not implemented yet.");
  } else {
    logger::fatal("SurfaceForcing: Unrecognized wind config \"", wind_config_, "\".");
  }

  // MOM6 reads RHO_0 here, for the friction velocity and the buoyancy forcing.

  bool restorebuoy = false;
  params.get("RESTOREBUOY", restorebuoy,
             {.default_value = false,
              .desc = "If true, the buoyancy fluxes drive the model back toward some specified "
                      "surface state with a rate given by FLUXCONST."});
  if (restorebuoy) {
    // defer: the restoring buoyancy fluxes (FLUXCONST, ...).
    logger::fatal("SurfaceForcing: The restoring buoyancy fluxes (RESTOREBUOY = True) are "
                  "not implemented yet.");
  }

  // MOM6 reads LATENT_HEAT_FUSION, LATENT_HEAT_VAPORIZATION and G_EARTH here,
  // for the buoyancy forcing, and GUST_CONST, for the friction velocity.

  bool ustar_gustless_bug = false;
  params.get("USTAR_GUSTLESS_BUG", ustar_gustless_bug,
             {.default_value = false,
              .desc = "If true include a bug in the time-averaging of the gustless wind "
                      "friction velocity"});
  if (ustar_gustless_bug) {
    logger::fatal("SurfaceForcing: USTAR_GUSTLESS_BUG = True is not supported. "
                  "Only the bug-fixed answers are implemented.");
  }
  // MOM6 also accepts FIX_USTAR_GUSTLESS_BUG, an obsolete parameter with the
  // opposite sense.

  bool read_gust_2d = false;
  params.get("READ_GUST_2D", read_gust_2d,
             {.default_value = false,
              .desc = "If true, use a 2-dimensional gustiness supplied from an input file"});
  if (read_gust_2d) {
    // defer: the gustiness from a file (GUST_2D_FILE).
    logger::fatal("SurfaceForcing: Reading a 2-dimensional gustiness (READ_GUST_2D = True) "
                  "is not implemented yet.");
  }

  // defer: the MARBL forcing (USE_MARBL_TRACERS), the forcing restart file,
  //        and the forcing diagnostics.
}

void SurfaceForcing::set_forcing(MechForcing &forces, const Grid &grid) {

  if (variable_winds_ || first_call_) {
    if (wind_config_ == "2gyre") {
      wind_forcing_2gyre(forces, grid, taux_mag_, south_lat_, len_lat_);
    } else {
      // defer: all other WIND_CONFIG options
      logger::fatal("set_forcing: WIND_CONFIG \"", wind_config_, "\" is not implemented yet.");
    }
  }

  // The buoyancy forcing is skipped, as in MOM6 for an adiabatic run.

  first_call_ = false;
}

} // namespace MOM
