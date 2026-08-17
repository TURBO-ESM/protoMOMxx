#include <cmath>

#include "MOM_shared_initialization.h"

namespace MOM {

amrex::MultiFab planetary_rotation(const Domain &domain, const GridSpec &spec,
                                   const amrex::MultiFab &geoLatBu) {

  const int n_levels = 1;
  const int ncomp = 1;
  amrex::MultiFab CoriolisBu = domain.make_field(Stagger::Node, n_levels, ncomp);

  const amrex::Real PI = 4.0 * std::atan(1.0);
  const amrex::Real omega = spec.omega;

  for (amrex::MFIter mfi(CoriolisBu); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mfi.growntilebox();
    const amrex::Array4<amrex::Real> f = CoriolisBu.array(mfi);
    const amrex::Array4<const amrex::Real> lat = geoLatBu.const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      f(i, j, k) = (2.0 * omega) * std::sin((PI * lat(i, j, k)) / 180.0);
    });
  }

  return CoriolisBu;
}

} // namespace MOM
