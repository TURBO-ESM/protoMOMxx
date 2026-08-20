#include <cmath>
#include <numbers>

#include "MOM_shared_initialization.h"

namespace MOM {

amrex::MultiFab planetary_rotation(const Domain &domain, const GridSpec &spec,
                                   const amrex::MultiFab &geoLatBu) {

  amrex::MultiFab CoriolisBu = domain.make_q_field(1);

  const amrex::Real PI = std::numbers::pi_v<amrex::Real>;
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
