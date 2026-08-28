#include <cmath>
#include <numbers>
#include <string>

#include "MOM_shared_initialization.h"

#include "MOM_logger.h"

namespace MOM {

amrex::MultiFab MOM_initialize_rotation(const Domain &domain,
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
    logger::fatal("MOM_initialize_rotation: ROTATION \"", rotation,
                  "\" is not implemented yet.");
  } else {
    logger::fatal("MOM_initialize_rotation: Unrecognized rotation setup \"",
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
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      f(i, j, k) = (2.0 * omega) * std::sin((PI * lat(i, j, k)) / 180.0);
    });
  }

  return CoriolisBu;
}

} // namespace MOM
