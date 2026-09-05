#include <cmath>
#include <numbers>
#include <string>

#include "MOM_shared_initialization.h"

#include "MOM_logger.h"

namespace MOM {

amrex::MultiFab initialize_rotation(const Domain &domain,
                                    const amrex::MultiFab &geoLatBu,
                                    RuntimeParams &params) {

  std::string rotation = "2omegasinlat";
  params.get("ROTATION", rotation,
             {.default_value = std::string("2omegasinlat"),
              .desc = "This specifies how the Coriolis parameter is specified:\n"
                      "\t 2omegasinlat - Use twice the planetary rotation rate\n"
                      "\t\t times the sine of latitude.\n"
                      "\t betaplane - Use a beta-plane or f-plane.\n"
                      "\t USER - call a user modified routine."});

  if (rotation == "2omegasinlat") {
    RotationSpec spec;
    params.get("OMEGA", spec.omega,
               {.default_value = 7.2921e-5,
                .desc = "The rotation rate of the earth.",
                .units = "s-1"});
    return set_rotation_planetary(domain, geoLatBu, spec);
  }

  if (rotation == "beta" || rotation == "betaplane") {
    // defer: the beta-plane/f-plane rotation (set_rotation_beta_plane).
    logger::fatal("initialize_rotation: ROTATION \"", rotation,
                  "\" is not implemented yet.");
  } else {
    logger::fatal("initialize_rotation: Unrecognized rotation setup \"",
                  rotation, "\".");
  }

  return {};  // Unreachable: logger::fatal throws.
}

amrex::MultiFab set_rotation_planetary(const Domain &domain,
                                       const amrex::MultiFab &geoLatBu,
                                       const RotationSpec &spec) {

  amrex::MultiFab CoriolisBu = domain.make_q_field({.nk = 1});

  const amrex::Real PI = std::numbers::pi_v<amrex::Real>;
  const amrex::Real omega = spec.omega;

  for (amrex::MFIter mfi(CoriolisBu); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> f = CoriolisBu.array(mfi);
    const amrex::Array4<const amrex::Real> lat = geoLatBu.const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      f(i, j, 0) = (2.0 * omega) * std::sin((PI * lat(i, j, 0)) / 180.0);
    });
  }

  return CoriolisBu;
}

TopoSpec read_topo_spec(RuntimeParams &params, const std::string_view config) {

  TopoSpec spec;

  spec.min_depth = read_minimum_depth(params);

  if (config != "flat") {
    params.get("EDGE_DEPTH", spec.edge_depth,
               {.default_value = 100.0,
                .desc = "The depth at the edge of one of the named topographies.",
                .units = "m"});
    params.get("TOPOG_SLOPE_SCALE", spec.topog_slope_scale,
               {.default_value = 400000.0,
                .desc = "The exponential decay scale used in defining some of "
                        "the named topographies.",
                .units = "m"});
  }

  // MOM6 accepts an absent MAXIMUM_DEPTH and diagnoses it from the computed
  // depths, but the named topographies require it set (they abort otherwise),
  // so it is mandatory here.
  // defer: the diagnosed MAXIMUM_DEPTH (diagnoseMaximumDepth), with the
  //        topography configurations that can run without the parameter.
  params.get("MAXIMUM_DEPTH", spec.max_depth,
             {.desc = "The maximum depth of the ocean.",
              .units = "m",
              .fail_if_missing = true});

  spec.mask_depth = read_masking_depth(params);

  return spec;
}

amrex::Real read_minimum_depth(RuntimeParams &params) {

  amrex::Real min_depth = 0.0;
  params.get("MINIMUM_DEPTH", min_depth,
             {.default_value = 0.0,
              .desc = "The minimum depth of the ocean.",
              .units = "m"});

  return min_depth;
}

std::optional<amrex::Real> read_masking_depth(RuntimeParams &params) {

  constexpr amrex::Real MASK_DEPTH_UNSET = -9999.0;

  amrex::Real mask_depth = MASK_DEPTH_UNSET;
  params.get("MASKING_DEPTH", mask_depth,
             {.default_value = MASK_DEPTH_UNSET,
              .desc = "The depth below which to mask points as land points, for which "
                      "all fluxes are zeroed out. MASKING_DEPTH is ignored if it has "
                      "the special default value.",
              .units = "m"});

  if (mask_depth == MASK_DEPTH_UNSET) return std::nullopt;
  return mask_depth;
}

amrex::MultiFab initialize_topography_named(const Domain &domain,
                                            const std::string_view config,
                                            const GridExtents &extents,
                                            const TopoSpec &spec,
                                            const amrex::MultiFab &geoLatT,
                                            const amrex::MultiFab &geoLonT) {

  amrex::MultiFab bathyT = domain.make_h_field({.nk = 1});
  bathyT.setVal(0.0);

  const amrex::Real PI = std::numbers::pi_v<amrex::Real>;
  const amrex::Real min_depth = spec.min_depth;
  const amrex::Real max_depth = spec.max_depth;

  if (!(max_depth > 0.0)) {
    logger::fatal("initialize_topography_named: MAXIMUM_DEPTH has a "
                  "non-sensical value! Was it set?");
  }

  if (config == "flat") {
    // Don't set ghost values because the halo zeros become land under
    // limit_topography, which is what closes the domain boundaries.
    bathyT.setVal(max_depth, /*comp=*/0, /*ncomp=*/1, /*nghost=*/0);

  } else if (config == "spoon") {

    const amrex::Real south_lat = extents.south_lat;
    const amrex::Real len_lat = extents.len_lat;
    const amrex::Real west_lon = extents.west_lon;
    const amrex::Real len_lon = extents.len_lon;
    const amrex::Real rad_earth = extents.rad_earth;
    // The depth at the basin edge [Z ~> m] and the decay scale of the sloping
    // boundaries [L ~> m], in MOM6's local vocabulary.
    const amrex::Real Dedge = spec.edge_depth;
    const amrex::Real expdecay = spec.topog_slope_scale;

    // A constant to make the maximum basin depth MAXIMUM_DEPTH [Z ~> m].
    const amrex::Real D0 = (max_depth - Dedge) /
        ((1.0 - std::exp(-0.5 * len_lat * rad_earth * PI / (180.0 * expdecay))) *
         (1.0 - std::exp(-0.5 * len_lat * rad_earth * PI / (180.0 * expdecay))));

    // A bowl-shaped (sort of) bottom topography with a vertical wall at the
    // southern face, deepest in the south-central basin.
    for (amrex::MFIter mfi(bathyT); mfi.isValid(); ++mfi) {
      const amrex::Box bx = mfi.validbox();
      const amrex::Array4<amrex::Real> D = bathyT.array(mfi);
      const amrex::Array4<const amrex::Real> lat = geoLatT.const_array(mfi);
      const amrex::Array4<const amrex::Real> lon = geoLonT.const_array(mfi);
      amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
        D(i, j, 0) = Dedge + D0 *
            (std::sin(PI * (lon(i, j, 0) - west_lon) / len_lon) *
             (1.0 - std::exp((lat(i, j, 0) - (south_lat + len_lat)) * rad_earth * PI /
                             (180.0 * expdecay))));
      });
    }

  } else {
    logger::fatal("initialize_topography_named: Unrecognized topography name \"",
                  config, "\".");
  }

  // MOM6's "here just for safety" clamp, over the same cells the named
  // configurations fill.
  for (amrex::MFIter mfi(bathyT); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.validbox();
    const amrex::Array4<amrex::Real> D = bathyT.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      amrex::Real val = D(i, j, 0);
      val = (val > max_depth) ? max_depth : val;
      val = (val < min_depth) ? (0.5 * min_depth) : val;
      D(i, j, 0) = val;
    });
  }

  return bathyT;
}

void limit_topography(amrex::MultiFab &bathyT, const TopoSpec &spec) {

  const amrex::Real min_depth = spec.min_depth;
  const amrex::Real max_depth = spec.max_depth;

  if (!spec.mask_depth) { // legacy way

    if (min_depth < 0.0) {
      logger::fatal("limit_topography: MINIMUM_DEPTH<0 does not work as "
                    "expected unless MASKING_DEPTH has been set appropriately. "
                    "Set a meaningful MASKING_DEPTH to enabled negative depths "
                    "(land elevations) and to enable flooding.");
    }

    for (amrex::MFIter mfi(bathyT); mfi.isValid(); ++mfi) {
      const amrex::Box bx = mfi.growntilebox();
      const amrex::Array4<amrex::Real> D = bathyT.array(mfi);
      amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
        D(i, j, 0) = amrex::min(amrex::max(D(i, j, 0), 0.5 * min_depth), max_depth);
      });
    }

  } else { // preferred way

    const amrex::Real land_depth = amrex::min(min_depth, *spec.mask_depth);
    for (amrex::MFIter mfi(bathyT); mfi.isValid(); ++mfi) {
      const amrex::Box bx = mfi.growntilebox();
      const amrex::Array4<amrex::Real> D = bathyT.array(mfi);
      amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
        if (D(i, j, 0) > land_depth) {
          D(i, j, 0) = amrex::min(amrex::max(D(i, j, 0), min_depth), max_depth);
        } else {
          D(i, j, 0) = land_depth;
        }
      });
    }
  }
}

} // namespace MOM
