#include <cmath>
#include <numbers>
#include <string>
#include <string_view>

#include <AMReX_GpuDevice.H>

#include "MOM_grid_initialize.h"

#include "MOM_logger.h"
#include "MOM_shared_initialization.h"

namespace MOM {

namespace {

// Check that the extents describe a usable grid.
void check_extents(const GridExtents &extents, const std::string_view caller) {
  if (!(extents.len_lat > 0.0) || !(extents.len_lon > 0.0)) {
    logger::fatal(caller, ": LENLAT and LENLON must be positive.");
  }
  if (!(extents.rad_earth > 0.0)) {
    logger::fatal(caller, ": RAD_EARTH must be positive.");
  }
}

} // namespace

GridFields spherical_grid_fields(const Domain &domain, const GridExtents &extents) {

  check_extents(extents, "spherical_grid_fields");

  // The metric fields are 2-D (single-level) fields on the domain's
  // horizontal decomposition, created through the domain's field factories.
  GridFields fields;

  fields.geoLatT = domain.make_h_field({.nk = 1});
  fields.geoLonT = domain.make_h_field({.nk = 1});
  fields.dxT = domain.make_h_field({.nk = 1});
  fields.dyT = domain.make_h_field({.nk = 1});
  fields.areaT = domain.make_h_field({.nk = 1});

  fields.geoLatCu = domain.make_u_field({.nk = 1});
  fields.geoLonCu = domain.make_u_field({.nk = 1});
  fields.dxCu = domain.make_u_field({.nk = 1});
  fields.dyCu = domain.make_u_field({.nk = 1});

  fields.geoLatCv = domain.make_v_field({.nk = 1});
  fields.geoLonCv = domain.make_v_field({.nk = 1});
  fields.dxCv = domain.make_v_field({.nk = 1});
  fields.dyCv = domain.make_v_field({.nk = 1});

  fields.geoLatBu = domain.make_q_field({.nk = 1});
  fields.geoLonBu = domain.make_q_field({.nk = 1});
  fields.dxBu = domain.make_q_field({.nk = 1});
  fields.dyBu = domain.make_q_field({.nk = 1});
  fields.areaBu = domain.make_q_field({.nk = 1});

  const amrex::Real PI = std::numbers::pi_v<amrex::Real>;
  const amrex::Real PI_180 = PI / 180.0;

  const amrex::Real south_lat = extents.south_lat;
  const amrex::Real west_lon = extents.west_lon;
  const amrex::Real rad_earth = extents.rad_earth;

  // The change in longitude/latitude between successive grid points [degrees].
  const amrex::Real dLon = extents.len_lon / domain.ni_global();
  const amrex::Real dLat = extents.len_lat / domain.nj_global();
  // dLon rescaled from degrees to radians [radians]. MOM6 computes the zonal
  // spacings from this expression (rather than from dLon*PI_180 directly) to
  // reproduce the set_grid_metrics_mercator solution on a simple spherical
  // grid; kept identical here for future parity.
  const amrex::Real dL_di = (extents.len_lon * PI) / (180.0 * domain.ni_global());
  // The meridional spacing is uniform on a spherical grid [L ~> m].
  const amrex::Real dy = rad_earth * dLat * PI_180;

  // All values are computed analytically from the global index (AMReX indices
  // are global), over the grown boxes: halo values (including those beyond
  // the global domain edges) extrapolate the same formulas, as in MOM6.
  // Cell centers sit at half-integer indices, corners at integer indices;
  // latitudes are clamped to [-90, 90] as in MOM6.

  // h points (cell centers)
  for (amrex::MFIter mfi(fields.geoLatT); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> geoLatT = fields.geoLatT.array(mfi);
    const amrex::Array4<amrex::Real> geoLonT = fields.geoLonT.array(mfi);
    const amrex::Array4<amrex::Real> dxT = fields.dxT.array(mfi);
    const amrex::Array4<amrex::Real> dyT = fields.dyT.array(mfi);
    const amrex::Array4<amrex::Real> areaT = fields.areaT.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      geoLonT(i, j, 0) = west_lon + dLon * (i + 0.5);
      geoLatT(i, j, 0) = amrex::min(amrex::max(south_lat + dLat * (j + 0.5),
                                               amrex::Real(-90.0)), amrex::Real(90.0));
      dxT(i, j, 0) = rad_earth * std::cos(geoLatT(i, j, 0) * PI_180) * dL_di;
      dyT(i, j, 0) = dy;
      areaT(i, j, 0) = dxT(i, j, 0) * dyT(i, j, 0);
    });
  }

  // u points (west faces: corner longitudes, cell-center latitudes)
  for (amrex::MFIter mfi(fields.geoLatCu); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> geoLatCu = fields.geoLatCu.array(mfi);
    const amrex::Array4<amrex::Real> geoLonCu = fields.geoLonCu.array(mfi);
    const amrex::Array4<amrex::Real> dxCu = fields.dxCu.array(mfi);
    const amrex::Array4<amrex::Real> dyCu = fields.dyCu.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      geoLonCu(i, j, 0) = west_lon + dLon * i;
      geoLatCu(i, j, 0) = amrex::min(amrex::max(south_lat + dLat * (j + 0.5),
                                                amrex::Real(-90.0)), amrex::Real(90.0));
      dxCu(i, j, 0) = rad_earth * std::cos(geoLatCu(i, j, 0) * PI_180) * dL_di;
      dyCu(i, j, 0) = dy;
    });
  }

  // v points (south faces: cell-center longitudes, corner latitudes)
  for (amrex::MFIter mfi(fields.geoLatCv); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> geoLatCv = fields.geoLatCv.array(mfi);
    const amrex::Array4<amrex::Real> geoLonCv = fields.geoLonCv.array(mfi);
    const amrex::Array4<amrex::Real> dxCv = fields.dxCv.array(mfi);
    const amrex::Array4<amrex::Real> dyCv = fields.dyCv.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      geoLonCv(i, j, 0) = west_lon + dLon * (i + 0.5);
      geoLatCv(i, j, 0) = amrex::min(amrex::max(south_lat + dLat * j,
                                                amrex::Real(-90.0)), amrex::Real(90.0));
      dxCv(i, j, 0) = rad_earth * std::cos(geoLatCv(i, j, 0) * PI_180) * dL_di;
      dyCv(i, j, 0) = dy;
    });
  }

  // q points (cell corners)
  for (amrex::MFIter mfi(fields.geoLatBu); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> geoLatBu = fields.geoLatBu.array(mfi);
    const amrex::Array4<amrex::Real> geoLonBu = fields.geoLonBu.array(mfi);
    const amrex::Array4<amrex::Real> dxBu = fields.dxBu.array(mfi);
    const amrex::Array4<amrex::Real> dyBu = fields.dyBu.array(mfi);
    const amrex::Array4<amrex::Real> areaBu = fields.areaBu.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      geoLonBu(i, j, 0) = west_lon + dLon * i;
      geoLatBu(i, j, 0) = amrex::min(amrex::max(south_lat + dLat * j,
                                                amrex::Real(-90.0)), amrex::Real(90.0));
      dxBu(i, j, 0) = rad_earth * std::cos(geoLatBu(i, j, 0) * PI_180) * dL_di;
      dyBu(i, j, 0) = dy;
      areaBu(i, j, 0) = dxBu(i, j, 0) * dyBu(i, j, 0);
    });
  }

  return fields;
}

GridExtents read_grid_extents(RuntimeParams &params) {

  GridExtents extents;

  params.get("SOUTHLAT", extents.south_lat,
             {.desc = "The southern latitude of the domain.",
              .units = "degrees_N",
              .fail_if_missing = true});

  params.get("LENLAT", extents.len_lat,
             {.desc = "The latitudinal length of the domain.",
              .units = "degrees_N",
              .fail_if_missing = true});

  params.get("WESTLON", extents.west_lon,
             {.default_value = 0.0,
              .desc = "The western longitude of the domain.",
              .units = "degrees_E"});

  params.get("LENLON", extents.len_lon,
             {.desc = "The longitudinal length of the domain.",
              .units = "degrees_E",
              .fail_if_missing = true});

  params.get("RAD_EARTH", extents.rad_earth,
             {.default_value = 6.378e6,
              .desc = "The radius of the Earth.",
              .units = "m"});

  // Validated at the read site, as make_domain does. The consumers repeat the
  // check for callers that assemble the extents from plain values.
  check_extents(extents, "read_grid_extents");

  return extents;
}

GridFields set_grid_metrics(const Domain &domain, RuntimeParams &params) {

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

  if (config == "spherical") {
    // The extents of the simple spherical grid, per set_grid_metrics_spherical.
    return spherical_grid_fields(domain, read_grid_extents(params));
  }

  if (config == "mosaic" || config == "cartesian" || config == "mercator") {
    // defer: the mosaic (file-based), cartesian, and mercator grid
    //        configurations.
    logger::fatal("set_grid_metrics: GRID_CONFIG \"", config,
                  "\" is not implemented yet.");
  } else if (config == "file") {
    // Retired in MOM6 itself; carry its message.
    logger::fatal("set_grid_metrics: GRID_CONFIG \"file\" is no longer a supported "
                  "option. Use a mosaic file (\"mosaic\") or one of the analytic "
                  "forms instead.");
  } else {
    logger::fatal("set_grid_metrics: Unrecognized grid configuration \"", config, "\".");
  }

  return {};  // Unreachable: logger::fatal throws.
}

void set_masks(const Domain &domain, GridFields &fields,
               const amrex::Real mask_depth) {

  fields.mask2dT = domain.make_h_field({.nk = 1});
  fields.mask2dCu = domain.make_u_field({.nk = 1});
  fields.mask2dCv = domain.make_v_field({.nk = 1});
  fields.mask2dBu = domain.make_q_field({.nk = 1});

  // These zeros remain as the land values in the halos beyond the closed
  // boundaries, which the exchanges below don't touch.
  {
    const amrex::Gpu::SyncAtExitOnly region;
    fields.mask2dT.setVal(0.0);
    fields.mask2dCu.setVal(0.0);
    fields.mask2dCv.setVal(0.0);
    fields.mask2dBu.setVal(0.0);
  }

  // The h-point mask
  for (amrex::MFIter mfi(fields.mask2dT); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.validbox();
    const amrex::Array4<amrex::Real> maskT = fields.mask2dT.array(mfi);
    const amrex::Array4<const amrex::Real> D = fields.bathyT.const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      maskT(i, j, 0) = (D(i, j, 0) <= mask_depth) ? 0.0 : 1.0;
    });
  }

  domain.pass_var(fields.mask2dT);

  // The face masks: a face is open when both neighboring cells are ocean.
  for (amrex::MFIter mfi(fields.mask2dCu); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.validbox();
    const amrex::Array4<amrex::Real> maskCu = fields.mask2dCu.array(mfi);
    const amrex::Array4<const amrex::Real> maskT = fields.mask2dT.const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      maskCu(i, j, 0) = maskT(i - 1, j, 0) * maskT(i, j, 0);
    });
  }

  for (amrex::MFIter mfi(fields.mask2dCv); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.validbox();
    const amrex::Array4<amrex::Real> maskCv = fields.mask2dCv.array(mfi);
    const amrex::Array4<const amrex::Real> maskT = fields.mask2dT.const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      maskCv(i, j, 0) = maskT(i, j - 1, 0) * maskT(i, j, 0);
    });
  }

  domain.pass_vars(
    fields.mask2dCu,
    fields.mask2dCv
  );

  // The vertex mask, from the masks of the four faces that meet at the vertex.
  for (amrex::MFIter mfi(fields.mask2dBu); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.validbox();
    const amrex::Array4<amrex::Real> maskBu = fields.mask2dBu.array(mfi);
    const amrex::Array4<const amrex::Real> maskCu = fields.mask2dCu.const_array(mfi);
    const amrex::Array4<const amrex::Real> maskCv = fields.mask2dCv.const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      maskBu(i, j, 0) = (maskCu(i, j - 1, 0) * maskCu(i, j, 0)) *
                        (maskCv(i - 1, j, 0) * maskCv(i, j, 0));
    });
  }

  domain.pass_var(fields.mask2dBu);

  fields.areaCu = domain.make_u_field({.nk = 1});
  fields.areaCv = domain.make_v_field({.nk = 1});

  // The u/v-cell areas, from the masked (open) face lengths.
  for (amrex::MFIter mfi(fields.areaCu); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> areaCu = fields.areaCu.array(mfi);
    const amrex::Array4<const amrex::Real> maskCu = fields.mask2dCu.const_array(mfi);
    const amrex::Array4<const amrex::Real> dxCu = fields.dxCu.const_array(mfi);
    const amrex::Array4<const amrex::Real> dyCu = fields.dyCu.const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      const amrex::Real dy_Cu = maskCu(i, j, 0) * dyCu(i, j, 0);
      areaCu(i, j, 0) = dxCu(i, j, 0) * dy_Cu;
    });
  }

  for (amrex::MFIter mfi(fields.areaCv); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> areaCv = fields.areaCv.array(mfi);
    const amrex::Array4<const amrex::Real> maskCv = fields.mask2dCv.const_array(mfi);
    const amrex::Array4<const amrex::Real> dxCv = fields.dxCv.const_array(mfi);
    const amrex::Array4<const amrex::Real> dyCv = fields.dyCv.const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      const amrex::Real dx_Cv = maskCv(i, j, 0) * dxCv(i, j, 0);
      areaCv(i, j, 0) = dyCv(i, j, 0) * dx_Cv;
    });
  }
}

void initialize_masks(const Domain &domain, GridFields &fields,
                      RuntimeParams &params) {

  // The depths, re-read where consumed as in MOM6.
  const amrex::Real min_depth = read_minimum_depth(params);

  // Points are land at MASKING_DEPTH when it is set, at MINIMUM_DEPTH
  // otherwise.
  set_masks(domain, fields, read_masking_depth(params).value_or(min_depth));
}

} // namespace MOM
