#include <cmath>
#include <numbers>

#include "MOM_grid_initialize.h"

namespace MOM {

GridFields spherical_grid_fields(const Domain &domain, const GridSpec &spec) {

  // The metric fields are 2-D (single-level) fields on the domain's
  // horizontal decomposition, created through the domain's field factories.
  GridFields fields;

  fields.south_lat = spec.south_lat;
  fields.len_lat = spec.len_lat;
  fields.west_lon = spec.west_lon;
  fields.len_lon = spec.len_lon;
  fields.rad_earth = spec.rad_earth;

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

  // The change in longitude/latitude between successive grid points [degrees].
  const amrex::Real dLon = spec.len_lon / domain.ni_global();
  const amrex::Real dLat = spec.len_lat / domain.nj_global();
  // dLon rescaled from degrees to radians [radians]. MOM6 computes the zonal
  // spacings from this expression (rather than from dLon*PI_180 directly) to
  // reproduce the set_grid_metrics_mercator solution on a simple spherical
  // grid; kept identical here for future parity.
  const amrex::Real dL_di = (spec.len_lon * PI) / (180.0 * domain.ni_global());
  // The meridional spacing is uniform on a spherical grid [L ~> m].
  const amrex::Real dy = spec.rad_earth * dLat * PI_180;

  const amrex::Real south_lat = spec.south_lat;
  const amrex::Real west_lon = spec.west_lon;
  const amrex::Real rad_earth = spec.rad_earth;

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
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      geoLonT(i, j, k) = west_lon + dLon * (i + 0.5);
      geoLatT(i, j, k) = amrex::min(amrex::max(south_lat + dLat * (j + 0.5),
                                               amrex::Real(-90.0)), amrex::Real(90.0));
      dxT(i, j, k) = rad_earth * std::cos(geoLatT(i, j, k) * PI_180) * dL_di;
      dyT(i, j, k) = dy;
      areaT(i, j, k) = dxT(i, j, k) * dyT(i, j, k);
    });
  }

  // u points (west faces: corner longitudes, cell-center latitudes)
  for (amrex::MFIter mfi(fields.geoLatCu); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> geoLatCu = fields.geoLatCu.array(mfi);
    const amrex::Array4<amrex::Real> geoLonCu = fields.geoLonCu.array(mfi);
    const amrex::Array4<amrex::Real> dxCu = fields.dxCu.array(mfi);
    const amrex::Array4<amrex::Real> dyCu = fields.dyCu.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      geoLonCu(i, j, k) = west_lon + dLon * i;
      geoLatCu(i, j, k) = amrex::min(amrex::max(south_lat + dLat * (j + 0.5),
                                                amrex::Real(-90.0)), amrex::Real(90.0));
      dxCu(i, j, k) = rad_earth * std::cos(geoLatCu(i, j, k) * PI_180) * dL_di;
      dyCu(i, j, k) = dy;
    });
  }

  // v points (south faces: cell-center longitudes, corner latitudes)
  for (amrex::MFIter mfi(fields.geoLatCv); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> geoLatCv = fields.geoLatCv.array(mfi);
    const amrex::Array4<amrex::Real> geoLonCv = fields.geoLonCv.array(mfi);
    const amrex::Array4<amrex::Real> dxCv = fields.dxCv.array(mfi);
    const amrex::Array4<amrex::Real> dyCv = fields.dyCv.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      geoLonCv(i, j, k) = west_lon + dLon * (i + 0.5);
      geoLatCv(i, j, k) = amrex::min(amrex::max(south_lat + dLat * j,
                                                amrex::Real(-90.0)), amrex::Real(90.0));
      dxCv(i, j, k) = rad_earth * std::cos(geoLatCv(i, j, k) * PI_180) * dL_di;
      dyCv(i, j, k) = dy;
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
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      geoLonBu(i, j, k) = west_lon + dLon * i;
      geoLatBu(i, j, k) = amrex::min(amrex::max(south_lat + dLat * j,
                                                amrex::Real(-90.0)), amrex::Real(90.0));
      dxBu(i, j, k) = rad_earth * std::cos(geoLatBu(i, j, k) * PI_180) * dL_di;
      dyBu(i, j, k) = dy;
      areaBu(i, j, k) = dxBu(i, j, k) * dyBu(i, j, k);
    });
  }

  return fields;
}

} // namespace MOM
