#include <string>
#include <utility>

#include "MOM_fixed_initialization.h"
#include "MOM_grid_initialize.h"
#include "MOM_logger.h"
#include "MOM_shared_initialization.h"

namespace MOM {

namespace {

// Read the extents of a simple spherical grid into the spec. The analogue of
// the parameter reads of MOM6's set_grid_metrics_spherical; the metric
// computation itself lives in MOM_grid_initialize.
void read_spherical_grid_params(RuntimeParams &params, GridSpec &spec) {

  params.get("SOUTHLAT", spec.south_lat,
             {.desc = "The southern latitude of the domain.",
              .units = "degrees_N",
              .fail_if_missing = true});

  params.get("LENLAT", spec.len_lat,
             {.desc = "The latitudinal length of the domain.",
              .units = "degrees_N",
              .fail_if_missing = true});

  params.get("WESTLON", spec.west_lon,
             {.default_value = 0.0,
              .desc = "The western longitude of the domain.",
              .units = "degrees_E"});

  params.get("LENLON", spec.len_lon,
             {.desc = "The longitudinal length of the domain.",
              .units = "degrees_E",
              .fail_if_missing = true});

  params.get("RAD_EARTH", spec.rad_earth,
             {.default_value = 6.378e6,
              .desc = "The radius of the Earth.",
              .units = "m"});

  if (!(spec.len_lat > 0.0) || !(spec.len_lon > 0.0)) {
    logger::fatal("make_grid: LENLAT and LENLON must be positive.");
  }
  if (!(spec.rad_earth > 0.0)) {
    logger::fatal("make_grid: RAD_EARTH must be positive.");
  }
}

// Read the planetary rotation configuration into the spec. The analogue of
// the parameter reads of MOM6's MOM_initialize_rotation
// (MOM_shared_initialization.F90).
void read_rotation_params(RuntimeParams &params, GridSpec &spec) {

  std::string rotation = "2omegasinlat";
  params.get("ROTATION", rotation,
             {.default_value = std::string("2omegasinlat"),
              .desc = "This specifies how the Coriolis parameter is specified:\n"
                      "\t 2omegasinlat - Use twice the planetary rotation rate\n"
                      "\t\t times the sine of latitude.\n"
                      "\t betaplane - Use a beta-plane or f-plane.\n"
                      "\t USER - call a user modified routine."});

  if (rotation == "2omegasinlat") {
    params.get("OMEGA", spec.omega,
               {.default_value = 7.2921e-5,
                .desc = "The rotation rate of the earth.",
                .units = "s-1"});
  } else if (rotation == "beta" || rotation == "betaplane") {
    // defer: the beta-plane/f-plane rotation (set_rotation_beta_plane).
    logger::fatal("make_grid: ROTATION \"", rotation,
                  "\" is not implemented yet.");
  } else {
    logger::fatal("make_grid: Unrecognized rotation setup \"", rotation, "\".");
  }
}

} // namespace

Grid make_grid(const Domain &domain, RuntimeParams &params) {

  params.doc_module("MOM_grid_init", "");

  std::string config;
  params.get("GRID_CONFIG", config,
             {.desc = "A character string that determines the method for defining the horizontal "
                      "grid. Current options are:\n"
                      "\t mosaic - read the grid from a mosaic (supergrid)\n"
                      "\t\t file set by GRID_FILE.\n"
                      "\t cartesian - use a (flat) Cartesian grid.\n"
                      "\t spherical - use a simple spherical grid.\n"
                      "\t mercator - use a Mercator spherical grid.",
              .fail_if_missing = true});

  GridSpec spec;
  GridFields fields;
  if (config == "spherical") {
    read_spherical_grid_params(params, spec);
    fields = spherical_grid_fields(domain, spec);
  } else if (config == "mosaic" || config == "cartesian" || config == "mercator") {
    // defer: the mosaic (file-based), cartesian, and mercator grid
    //        configurations.
    logger::fatal("make_grid: GRID_CONFIG \"", config,
                  "\" is not implemented yet.");
  } else if (config == "file") {
    // Retired in MOM6 itself; carry its message.
    logger::fatal("make_grid: GRID_CONFIG \"file\" is no longer a supported "
                  "option. Use a mosaic file (\"mosaic\") or one of the analytic "
                  "forms instead.");
  } else {
    logger::fatal("make_grid: Unrecognized grid configuration \"", config, "\".");
  }

  // todo: topography (TOPO_CONFIG, MINIMUM_DEPTH, MAXIMUM_DEPTH) and the
  //       land/sea masks are read and set here, between the metrics and the
  //       rotation, matching MOM6's MOM_initialize_fixed order.

  read_rotation_params(params, spec);
  fields.CoriolisBu = planetary_rotation(domain, spec, fields.geoLatBu);

  // defer: the reciprocals (IdxT, IdyCu, IareaT, ...) of MOM6's
  //        set_derived_dyn_horgrid, until their first user (the dynamics
  //        kernels), and the u/v-cell areas (areaCu/areaCv), which are
  //        mask-dependent (areaCu = dxCu * dy_Cu with dy_Cu = mask2dCu * dyCu
  //        in MOM6's initialize_masks), until the masks are introduced.

  return Grid(std::move(fields));
}

} // namespace MOM
