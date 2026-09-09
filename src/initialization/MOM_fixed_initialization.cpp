#include <string>
#include <utility>

#include "MOM_fixed_initialization.h"
#include "MOM_grid_initialize.h"
#include "MOM_logger.h"
#include "MOM_shared_initialization.h"

namespace MOM {

amrex::MultiFab initialize_topography(const Domain &domain,
                                      const amrex::MultiFab &geoLatT,
                                      const amrex::MultiFab &geoLonT,
                                      RuntimeParams &params) {

  std::string config;
  params.get("TOPO_CONFIG", config,
             {.desc = "This specifies how bathymetry is specified:\n"
                      "\t file - read bathymetric information from the file\n"
                      "\t\t specified by (TOPO_FILE).\n"
                      "\t flat - flat bottom set to MAXIMUM_DEPTH.\n"
                      "\t bowl - an analytically specified bowl-shaped basin\n"
                      "\t\t ranging between MAXIMUM_DEPTH and MINIMUM_DEPTH.\n"
                      "\t spoon - a similar shape to 'bowl', but with an vertical\n"
                      "\t\t wall at the southern face.\n"
                      "\t halfpipe - a zonally uniform channel with a half-sine\n"
                      "\t\t profile in the meridional direction.\n"
                      "\t bbuilder - build topography from list of functions.\n"
                      "\t benchmark - use the benchmark test case topography.\n"
                      "\t Neverworld - use the Neverworld test case topography.\n"
                      "\t DOME - use a slope and channel configuration for the\n"
                      "\t\t DOME sill-overflow test case.\n"
                      "\t ISOMIP - use a slope and channel configuration for the\n"
                      "\t\t ISOMIP test case.\n"
                      "\t DOME2D - use a shelf and slope configuration for the\n"
                      "\t\t DOME2D gravity current/overflow test case.\n"
                      "\t Kelvin - flat but with rotated land mask.\n"
                      "\t seamount - Gaussian bump for spontaneous motion test case.\n"
                      "\t dumbbell - Sloshing channel with reservoirs on both ends.\n"
                      "\t shelfwave - exponential slope for shelfwave test case.\n"
                      "\t Phillips - ACC-like idealized topography used in the Phillips config.\n"
                      "\t dense - Denmark Strait-like dense water formation and overflow.\n"
                      "\t USER - call a user modified routine.",
              .fail_if_missing = true});

  amrex::MultiFab bathyT;

  if (config == "flat" || config == "spoon") {
    const GridExtents extents = read_grid_extents(params);
    const TopoSpec spec = read_topo_spec(params, config);

    bathyT = initialize_topography_named(domain, config, extents, spec, geoLatT, geoLonT);
    limit_topography(bathyT, spec);
  } else if (config == "file" || config == "bowl" || config == "halfpipe" ||
      config == "bbuilder" || config == "benchmark" ||
      config == "Neverworld" || config == "Neverland" || config == "DOME" ||
      config == "ISOMIP" || config == "DOME2D" || config == "Kelvin" ||
      config == "sloshing" || config == "seamount" || config == "dumbbell" ||
      config == "shelfwave" || config == "Phillips" || config == "dense" ||
      config == "USER") {
    // defer: all other topo options
    logger::fatal("initialize_topography: TOPO_CONFIG \"", config,
                  "\" is not implemented yet.");
  } else {
    logger::fatal("initialize_topography: Unrecognized topography setup \"",
                  config, "\".");
  }

  domain.pass_var(bathyT);

  return bathyT;
}

Grid make_grid(const Domain &domain, RuntimeParams &params) {

  params.doc_module("MOM_grid_init", "");

  GridFields fields = set_grid_metrics(domain, params);

  fields.bathyT = initialize_topography(domain, fields.geoLatT,
                                        fields.geoLonT, params);

  initialize_masks(domain, fields, params);

  fields.CoriolisBu = initialize_rotation(domain, fields.geoLatBu, params);

  // defer: the reciprocals (IdxT, IdyCu, IareaT, ...) of MOM6's
  //        set_derived_dyn_horgrid and initialize_masks, until their first
  //        user (the dynamics kernels).

  return Grid(std::move(fields));
}

} // namespace MOM
