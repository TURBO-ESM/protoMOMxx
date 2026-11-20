#include "MOM_interface_heights.hpp"

#include <AMReX_MFIter.H>

#include "MOM_domain_infra.hpp"
#include "MOM_domains.hpp"
#include "MOM_logger.hpp"
#include "mom_interface_heights.hpp" // this is the header for the ported kernels

namespace MOM {

void thickness_to_dz(const amrex::MultiFab &h, amrex::MultiFab &dz, const VerticalGrid &GV,
                     const int halo_size) {
  check_field(h, Stagger::Cell, halo_size, "thickness_to_dz", "h");
  check_field(dz, Stagger::Cell, halo_size, "thickness_to_dz", "dz");
  if (!GV.Boussinesq()) {
    // defer: the non-Boussinesq conversion, with the specific volume of the
    //        thermodynamic state (tv%SpV_avg).
    logger::fatal("thickness_to_dz: The non-Boussinesq mode is not implemented yet.");
  }

  // The thickness-unit conversion factors at their Boussinesq values without
  // rescaling. VerticalGrid defers them to the thickness/units layer.
  constexpr amrex::Real H_to_Z = 1.0;
  const amrex::Real H_to_RZ = GV.Rho0();

  for (amrex::MFIter mfi(h); mfi.isValid(); ++mfi) {
    MOM::thickness_to_dz_3d(mom_grow(mfi.validbox(), halo_size, halo_size),
                            mom_const_array(h, mfi), mom_array(dz, mfi),
                            /*spv_avg=*/amrex::Array4<const amrex::Real>{},
                            /*boussinesq=*/true, H_to_Z, H_to_RZ, /*has_spv=*/false);
  }
}

} // namespace MOM
