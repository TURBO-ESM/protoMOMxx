#pragma once
/// @file MOM_visc_fields.hpp
/// @brief The viscosities and bottom boundary layer properties that set_visc
///        sets and the vertical friction reads. The analogue of MOM6's
///        vertvisc_type (MOM_variables.F90).

#include <AMReX_MultiFab.H>

namespace MOM {

/// @brief The viscosities and bottom boundary layer properties, each set to 0
/// when created. make_set_visc creates the ones its options need, as MOM6's
/// set_visc_init does; the others stay empty.
struct ViscFields {
  // With BOTTOMDRAGLAW, each with one layer:
  amrex::MultiFab bbl_thick_u;  ///< The bottom boundary layer thickness at u points [Z ~> m].
  amrex::MultiFab bbl_thick_v;  ///< The bottom boundary layer thickness at v points [Z ~> m].
  amrex::MultiFab kv_bbl_u;     ///< The bottom boundary layer viscosity at u points
                                ///< [H Z T-1 ~> m2 s-1 or Pa s].
  amrex::MultiFab kv_bbl_v;     ///< The bottom boundary layer viscosity at v points
                                ///< [H Z T-1 ~> m2 s-1 or Pa s].
  amrex::MultiFab ustar_BBL;    ///< The turbulence velocity in the bottom boundary layer at
                                ///< h points [H T-1 ~> m s-1 or kg m-2 s-1].
  amrex::MultiFab BBL_meanKE_loss;  ///< The viscous loss of mean kinetic energy in the bottom
                                    ///< boundary layer [H L2 T-3 ~> m3 s-3 or W m-2].
  amrex::MultiFab BBL_meanKE_loss_sqrtCd;  ///< BBL_meanKE_loss divided by the square root of
                                           ///< the drag coefficient, kept by MOM6 only for
                                           ///< older answers [H L2 T-3 ~> m3 s-3 or W m-2].

  // With DRAG_AS_BODY_FORCE, each with nk layers:
  amrex::MultiFab Ray_u;  ///< The Rayleigh drag velocity at u points [H T-1 ~> m s-1 or Pa s m-1].
  amrex::MultiFab Ray_v;  ///< The Rayleigh drag velocity at v points [H T-1 ~> m s-1 or Pa s m-1].

  // defer: the other members of MOM6's vertvisc_type, which come with their
  //        first users: the ice shelf fields (taux_shelf, kv_tbl_shelf_u,
  //        ...), nkml_visc_u and nkml_visc_v of the dynamic viscous mixed
  //        layer, and the fields of the diabatic and mixed layer schemes
  //        (Kd_shear, Kv_shear, Kv_slow, TKE_turb, MLD, h_ML, ...).
};

} // namespace MOM
