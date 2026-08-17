#include <cmath>

#include "MOM_hor_grid.h"

namespace MOM {

HorGrid::HorGrid(const Domain &domain, const HorGridSpec &spec)
  : south_lat_(spec.south_lat), len_lat_(spec.len_lat),
    west_lon_(spec.west_lon), len_lon_(spec.len_lon),
    rad_earth_(spec.rad_earth) {

  // The metric fields are 2-D (single-level) fields on the domain's
  // horizontal decomposition, created through the domain's field factory,
  // which expresses the staggering as AMReX index-type nodality and carries
  // the domain's halo widths as ghost cells.
  const int n_levels = 1;
  const int ncomp = 1;

  geoLatT_ = domain.make_field(Stagger::Cell, n_levels, ncomp);
  geoLonT_ = domain.make_field(Stagger::Cell, n_levels, ncomp);
  dxT_ = domain.make_field(Stagger::Cell, n_levels, ncomp);
  dyT_ = domain.make_field(Stagger::Cell, n_levels, ncomp);
  areaT_ = domain.make_field(Stagger::Cell, n_levels, ncomp);

  geoLatCu_ = domain.make_field(Stagger::XFace, n_levels, ncomp);
  geoLonCu_ = domain.make_field(Stagger::XFace, n_levels, ncomp);
  dxCu_ = domain.make_field(Stagger::XFace, n_levels, ncomp);
  dyCu_ = domain.make_field(Stagger::XFace, n_levels, ncomp);

  geoLatCv_ = domain.make_field(Stagger::YFace, n_levels, ncomp);
  geoLonCv_ = domain.make_field(Stagger::YFace, n_levels, ncomp);
  dxCv_ = domain.make_field(Stagger::YFace, n_levels, ncomp);
  dyCv_ = domain.make_field(Stagger::YFace, n_levels, ncomp);

  geoLatBu_ = domain.make_field(Stagger::Node, n_levels, ncomp);
  geoLonBu_ = domain.make_field(Stagger::Node, n_levels, ncomp);
  dxBu_ = domain.make_field(Stagger::Node, n_levels, ncomp);
  dyBu_ = domain.make_field(Stagger::Node, n_levels, ncomp);

  CoriolisBu_ = domain.make_field(Stagger::Node, n_levels, ncomp);

  set_grid_metrics_spherical(domain, spec);

  // todo: init_topography (TOPO_CONFIG: bathyT, MINIMUM_DEPTH/MAXIMUM_DEPTH)
  //       and init_masks (land/sea masks at h/q/u/v points).

  set_rotation_planetary(spec);

  // defer: the derived metrics -- the reciprocals (IdxT, IdyCu, IareaT, ...),
  //        the q-cell area (areaBu) and the u/v-cell area averages
  //        (areaCu/areaCv) of MOM6's set_derived_dyn_horgrid, until their
  //        first consumer (the dynamics kernels).
}

void HorGrid::set_grid_metrics_spherical(const Domain &domain, const HorGridSpec &spec) {

  const amrex::Real PI = 4.0 * std::atan(1.0);
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
  for (amrex::MFIter mfi(geoLatT_); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> geoLatT = geoLatT_.array(mfi);
    const amrex::Array4<amrex::Real> geoLonT = geoLonT_.array(mfi);
    const amrex::Array4<amrex::Real> dxT = dxT_.array(mfi);
    const amrex::Array4<amrex::Real> dyT = dyT_.array(mfi);
    const amrex::Array4<amrex::Real> areaT = areaT_.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      geoLonT(i, j, k) = west_lon + dLon * (i + 0.5);
      geoLatT(i, j, k) = amrex::min(amrex::max(south_lat + dLat * (j + 0.5),
                                               amrex::Real(-90.0)), amrex::Real(90.0));
      dxT(i, j, k) = rad_earth * std::cos(geoLatT(i, j, k) * PI_180) * dL_di;
      dyT(i, j, k) = dy;
      areaT(i, j, k) = dxT(i, j, k) * dyT(i, j, k);
    });
  }

  // u points (east faces: corner longitudes, cell-center latitudes)
  for (amrex::MFIter mfi(geoLatCu_); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> geoLatCu = geoLatCu_.array(mfi);
    const amrex::Array4<amrex::Real> geoLonCu = geoLonCu_.array(mfi);
    const amrex::Array4<amrex::Real> dxCu = dxCu_.array(mfi);
    const amrex::Array4<amrex::Real> dyCu = dyCu_.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      geoLonCu(i, j, k) = west_lon + dLon * i;
      geoLatCu(i, j, k) = amrex::min(amrex::max(south_lat + dLat * (j + 0.5),
                                                amrex::Real(-90.0)), amrex::Real(90.0));
      dxCu(i, j, k) = rad_earth * std::cos(geoLatCu(i, j, k) * PI_180) * dL_di;
      dyCu(i, j, k) = dy;
    });
  }

  // v points (north faces: cell-center longitudes, corner latitudes)
  for (amrex::MFIter mfi(geoLatCv_); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> geoLatCv = geoLatCv_.array(mfi);
    const amrex::Array4<amrex::Real> geoLonCv = geoLonCv_.array(mfi);
    const amrex::Array4<amrex::Real> dxCv = dxCv_.array(mfi);
    const amrex::Array4<amrex::Real> dyCv = dyCv_.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      geoLonCv(i, j, k) = west_lon + dLon * (i + 0.5);
      geoLatCv(i, j, k) = amrex::min(amrex::max(south_lat + dLat * j,
                                                amrex::Real(-90.0)), amrex::Real(90.0));
      dxCv(i, j, k) = rad_earth * std::cos(geoLatCv(i, j, k) * PI_180) * dL_di;
      dyCv(i, j, k) = dy;
    });
  }

  // q points (cell corners)
  for (amrex::MFIter mfi(geoLatBu_); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> geoLatBu = geoLatBu_.array(mfi);
    const amrex::Array4<amrex::Real> geoLonBu = geoLonBu_.array(mfi);
    const amrex::Array4<amrex::Real> dxBu = dxBu_.array(mfi);
    const amrex::Array4<amrex::Real> dyBu = dyBu_.array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      geoLonBu(i, j, k) = west_lon + dLon * i;
      geoLatBu(i, j, k) = amrex::min(amrex::max(south_lat + dLat * j,
                                                amrex::Real(-90.0)), amrex::Real(90.0));
      dxBu(i, j, k) = rad_earth * std::cos(geoLatBu(i, j, k) * PI_180) * dL_di;
      dyBu(i, j, k) = dy;
    });
  }
}

void HorGrid::set_rotation_planetary(const HorGridSpec &spec) {

  const amrex::Real PI = 4.0 * std::atan(1.0);
  const amrex::Real omega = spec.omega;

  for (amrex::MFIter mfi(CoriolisBu_); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> f = CoriolisBu_.array(mfi);
    const amrex::Array4<const amrex::Real> geoLatBu = geoLatBu_.const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      f(i, j, k) = (2.0 * omega) * std::sin((PI * geoLatBu(i, j, k)) / 180.0);
    });
  }
}

} // namespace MOM
