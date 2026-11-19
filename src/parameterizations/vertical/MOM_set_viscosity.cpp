#include "MOM_set_viscosity.hpp"

#include <algorithm>
#include <string>

#include <AMReX_GpuDevice.H>

#include "MOM_domains.hpp"
#include "MOM_logger.hpp"

namespace MOM {

SetVisc make_set_visc(const Domain &domain, const Grid &G, const VerticalGrid &GV,
                      const PorousBarrier &pbv, RuntimeParams &params, ViscFields &visc) {

  params.doc_module("MOM_set_visc", "");

  SetVisc sv{.G = G, .GV = GV, .pbv = pbv};

  // defer: INPUTDIR, which MOM6 reads here for the files of CDRAG_MAP and
  //        BBL_USE_TIDAL_BG.
  int default_answer_date = 99991231;
  params.get("DEFAULT_ANSWER_DATE", default_answer_date,
             {.default_value = 99991231,
              .desc = "This sets the default value for the various _ANSWER_DATE parameters.",
              .do_not_log = true});  // MOM6 logs it from MOM.F90.
  params.get("SET_VISC_ANSWER_DATE", sv.answer_date,
             {.default_value = default_answer_date,
              .desc = "The vintage of the order of arithmetic and expressions in the set viscosity "
                      "calculations.  Values below 20190101 recover the answers from the end of 2018, "
                      "while higher values use updated and more robust forms of the same expressions.",
              .do_not_log = !GV.Boussinesq()});
  if (!GV.Boussinesq()) sv.answer_date = std::max(sv.answer_date, 20230701);
  params.get("BOTTOMDRAGLAW", sv.bottomdraglaw,
             {.default_value = true,
              .desc = "If true, the bottom stress is calculated with a drag "
                      "law of the form c_drag*|u|*u. The velocity magnitude "
                      "may be an assumed value or it may be based on the "
                      "actual velocity in the bottommost HBBL, depending on "
                      "LINEAR_DRAG."});
  params.get("DRAG_AS_BODY_FORCE", sv.body_force_drag,
             {.default_value = false,
              .desc = "If true, the bottom stress is imposed as an explicit body force "
                      "applied over a fixed distance from the bottom, rather than as an "
                      "implicit calculation based on an enhanced near-bottom viscosity. "
                      "The thickness of the bottom boundary layer is HBBL.",
              .do_not_log = !sv.bottomdraglaw});
  bool channel_drag = false;
  params.get("CHANNEL_DRAG", channel_drag,
             {.default_value = false,
              .desc = "If true, the bottom drag is exerted directly on each "
                      "layer proportional to the fraction of the bottom it overlies."});
  if (channel_drag) {
    // defer: the channel drag, with SMAG_CONST_CHANNEL and
    //        TRIG_CHANNEL_DRAG_WIDTHS, and the grid's Z_ref, which MOM6
    //        subtracts from CHANNEL_DRAG_SHELFBREAK_DEPTH.
    logger::fatal("make_set_visc: The channel drag (CHANNEL_DRAG = True) is not implemented yet.");
  }
  amrex::Real shelfbreak_depth = 0.0;
  params.get("CHANNEL_DRAG_SHELFBREAK_DEPTH", shelfbreak_depth,
             {.default_value = 0.0,
              .desc = "When CHANNEL_DRAG is true, the bathymetric depth interpolated to the "
                      "vorticity point is a combination of the harmonic mean of the adjacent "
                      "velocity point depths below this depth and the arithmetic mean of the "
                      "depths above it, to roughly mimic a continental shelf break profile.  "
                      "Setting this to exceed MAXIMUM_DEPTH leads to linear interpolation of "
                      "the topography between velocity points.",
              .units = "m",
              .do_not_log = !channel_drag});

  params.get("LINEAR_DRAG", sv.linear_drag,
             {.default_value = false,
              .desc = "If LINEAR_DRAG and BOTTOMDRAGLAW are defined the drag "
                      "law is cdrag*DRAG_BG_VEL*u."});
  // MOM6 logs ADIABATIC here again, which its parameter doc shows only once,
  // in the MOM block.
  bool adiabatic = false;
  params.get("ADIABATIC", adiabatic, {.default_value = false, .do_not_log = true});
  if (!adiabatic) {
    // defer: the shear mixing of the bottom boundary layer (kappa_shear_is_used).
    logger::fatal("make_set_visc: The diapycnal mixing (ADIABATIC = False) is not "
                  "implemented yet.");
  }

  bool debug = false;
  params.get("DEBUG", debug, {.default_value = false});
  if (debug) {
    // defer: the DEBUG checksums of set_viscous_BBL.
    logger::fatal("make_set_visc: The debugging checksums (DEBUG = True) are not "
                  "implemented yet.");
  }

  bool dynamic_viscous_ML = false;
  params.get("DYNAMIC_VISCOUS_ML", dynamic_viscous_ML,
             {.default_value = false,
              .desc = "If true, use a bulk Richardson number criterion to "
                      "determine the mixed layer thickness for viscosity."});
  if (dynamic_viscous_ML) {
    // defer: the dynamic viscous mixed layer of set_viscous_ML, with
    //        BULK_RI_ML_VISC, TKE_DECAY_VISC, ML_OMEGA_FRAC, nkml_visc_u and
    //        nkml_visc_v.
    logger::fatal("make_set_visc: The dynamic viscous mixed layer (DYNAMIC_VISCOUS_ML = True) "
                  "is not implemented yet.");
  }
  amrex::Real omega = 7.2921e-5;
  params.get("OMEGA", omega,
             {.default_value = 7.2921e-5,
              .desc = "The rotation rate of the earth.",
              .units = "s-1"});

  params.get("HBBL", sv.dz_bbl,
             {.desc = "The thickness of a bottom boundary layer with a viscosity increased by "
                      "KV_EXTRA_BBL if BOTTOMDRAGLAW is not defined, or the thickness over which "
                      "near-bottom velocities are averaged for the drag law if BOTTOMDRAGLAW is "
                      "defined but LINEAR_DRAG is not.",
              .units = "m",
              .fail_if_missing = true});
  if (sv.bottomdraglaw) {
    params.get("CDRAG", sv.cdrag,
               {.default_value = 0.003,
                .desc = "CDRAG is the drag coefficient relating the magnitude of "
                        "the velocity field to the bottom stress. CDRAG is only "
                        "used if BOTTOMDRAGLAW is defined.",
                .units = "nondim"});
    bool bottomdragmap = false;
    params.get("CDRAG_MAP", bottomdragmap,
               {.default_value = false,
                .desc = "If true, apply a spatially varying scaling factor to CDRAG, "
                        "specified by CDRAG_VAR in CDRAG_FILE."});
    if (bottomdragmap) {
      // defer: the drag coefficient read from a file, with CDRAG_FILE and
      //        CDRAG_VAR.
      logger::fatal("make_set_visc: A spatially varying drag coefficient (CDRAG_MAP = True) "
                    "is not implemented yet.");
    }
    std::string cdrag_file;
    params.get("CDRAG_FILE", cdrag_file,
               {.default_value = std::string(""),
                .desc = "The name of the file with the spatially varying bottom drag "
                        "scaling factor.",
                .do_not_log = !bottomdragmap});
    std::string cdrag_var;
    params.get("CDRAG_VAR", cdrag_var,
               {.default_value = std::string(""),
                .desc = "The name of the variable in CDRAG_FILE with the spatially "
                        "varying bottom drag scaling factor at h points.",
                .do_not_log = !bottomdragmap});
    bool BBL_use_tidal_bg = false;
    params.get("BBL_USE_TIDAL_BG", BBL_use_tidal_bg,
               {.default_value = false,
                .desc = "Flag to use the tidal RMS amplitude in place of constant "
                        "background velocity for computing u* in the BBL. "
                        "This flag is only used when BOTTOMDRAGLAW is true and "
                        "LINEAR_DRAG is false."});
    if (BBL_use_tidal_bg) {
      // defer: the tidal amplitudes read from a file, with TIDEAMP_FILE,
      //        TIDEAMP_VARNAME and TIDEAMP_FACTOR.
      logger::fatal("make_set_visc: The tidal background velocity (BBL_USE_TIDAL_BG = True) "
                    "is not implemented yet.");
    }
    params.get("DRAG_BG_VEL", sv.drag_bg_vel,
               {.default_value = 0.0,
                .desc = "DRAG_BG_VEL is either the assumed bottom velocity (with "
                        "LINEAR_DRAG) or an unresolved  velocity that is "
                        "combined with the resolved velocity to estimate the "
                        "velocity magnitude.  DRAG_BG_VEL is only used when "
                        "BOTTOMDRAGLAW is defined.",
                .units = "m s-1"});
    bool use_regridding = false;
    params.get("USE_REGRIDDING", use_regridding, {.default_value = false, .do_not_log = true});
    bool use_temperature = true;
    params.get("ENABLE_THERMODYNAMICS", use_temperature,
               {.default_value = true, .do_not_log = true});
    bool use_EOS = use_temperature;
    params.get("USE_EOS", use_EOS, {.default_value = use_temperature, .do_not_log = true});
    bool BBL_use_EOS = use_EOS;
    params.get("BBL_USE_EOS", BBL_use_EOS,
               {.default_value = use_EOS,
                .desc = "If true, use the equation of state in determining the properties of the "
                        "bottom boundary layer.  Otherwise use the layer target potential densities.  "
                        "The default of this parameter is the value of USE_EOS.",
                .do_not_log = !use_temperature});
    if (use_regridding && !BBL_use_EOS) {
      logger::fatal("When using MOM6 in ALE mode it is required to set BBL_USE_EOS to True.");
    }
    if (BBL_use_EOS) {
      // defer: the bottom boundary layer densities from the equation of state.
      logger::fatal("make_set_visc: The bottom boundary layer from the equation of state "
                    "(BBL_USE_EOS = True) is not implemented yet.");
    }
  }
  params.get("BBL_THICK_MIN", sv.BBL_thick_min,
             {.default_value = 0.0,
              .desc = "The minimum bottom boundary layer thickness that can be "
                      "used with BOTTOMDRAGLAW. This might be "
                      "Kv/(cdrag*drag_bg_vel) to give Kv as the minimum "
                      "near-bottom viscosity.",
              .units = "m"});
  // The thickness and viscosity of the top boundary layer under ice shelves.
  amrex::Real Htbl_shelf_min = 0.0;
  params.get("HTBL_SHELF_MIN", Htbl_shelf_min,
             {.default_value = sv.BBL_thick_min,
              .desc = "The minimum top boundary layer thickness that can be "
                      "used with BOTTOMDRAGLAW. This might be "
                      "Kv/(cdrag*drag_bg_vel) to give Kv as the minimum "
                      "near-top viscosity.",
              .units = "m"});
  amrex::Real Htbl_shelf = 0.0;
  params.get("HTBL_SHELF", Htbl_shelf,
             {.default_value = sv.dz_bbl,
              .desc = "The thickness over which near-surface velocities are "
                      "averaged for the drag law under an ice shelf.  By "
                      "default this is the same as HBBL",
              .units = "m"});

  amrex::Real Kv_background = 0.0;
  params.get("KV", Kv_background,
             {.desc = "The background kinematic viscosity in the interior. "
                      "The molecular value, ~1e-6 m2 s-1, may be used.",
              .units = "m2 s-1",
              .fail_if_missing = true});

  params.get("KV_BBL_MIN", sv.Kv_BBL_min,
             {.default_value = Kv_background,
              .desc = "The minimum viscosities in the bottom boundary layer.",
              .units = "m2 s-1"});
  amrex::Real Kv_TBL_min = 0.0;
  params.get("KV_TBL_MIN", Kv_TBL_min,
             {.default_value = Kv_background,
              .desc = "The minimum viscosities in the top boundary layer.",
              .units = "m2 s-1"});
  params.get("CORRECT_BBL_BOUNDS", sv.correct_BBL_bounds,
             {.default_value = false,
              .desc = "If true, uses the correct bounds on the BBL thickness and "
                      "viscosity so that the bottom layer feels the intended drag."});

  amrex::Real Chan_max_thick_dflt = -1.0;
  if (sv.body_force_drag) Chan_max_thick_dflt = sv.dz_bbl;
  amrex::Real Chan_drag_max_vol = 0.0;
  params.get("CHANNEL_DRAG_MAX_BBL_THICK", Chan_drag_max_vol,
             {.default_value = Chan_max_thick_dflt,
              .desc = "The maximum bottom boundary layer thickness over which the channel drag is "
                      "exerted, or a negative value for no fixed limit, instead basing the BBL "
                      "thickness on the bottom stress, rotation and stratification.  The default is "
                      "proportional to HBBL if USE_JACKSON_PARAM or DRAG_AS_BODY_FORCE is true.",
              .units = "m",
              .do_not_log = !channel_drag});

  // In Boussinesq mode, the thicknesses are in meters, as the heights are.
  sv.Hbbl = sv.dz_bbl;

  if (sv.bottomdraglaw) {
    visc.bbl_thick_u = domain.make_u_field({.nk = 1});
    visc.bbl_thick_v = domain.make_v_field({.nk = 1});
    visc.kv_bbl_u = domain.make_u_field({.nk = 1});
    visc.kv_bbl_v = domain.make_v_field({.nk = 1});
    visc.ustar_BBL = domain.make_h_field({.nk = 1});
    visc.BBL_meanKE_loss = domain.make_h_field({.nk = 1});
    visc.BBL_meanKE_loss_sqrtCd = domain.make_h_field({.nk = 1});
  }
  if (sv.body_force_drag) {
    visc.Ray_u = domain.make_u_field({.nk = GV.nk()});
    visc.Ray_v = domain.make_v_field({.nk = GV.nk()});
  }

  // MOM6 sets these fields to 0 when it allocates them.
  {
    const amrex::Gpu::SyncAtExitOnly region;
    if (sv.bottomdraglaw) {
      visc.bbl_thick_u.setVal(0.0);
      visc.bbl_thick_v.setVal(0.0);
      visc.kv_bbl_u.setVal(0.0);
      visc.kv_bbl_v.setVal(0.0);
      visc.ustar_BBL.setVal(0.0);
      visc.BBL_meanKE_loss.setVal(0.0);
      visc.BBL_meanKE_loss_sqrtCd.setVal(0.0);
    }
    if (sv.body_force_drag) {
      visc.Ray_u.setVal(0.0);
      visc.Ray_v.setVal(0.0);
    }
  }

  return sv;
}

void SetVisc::set_viscous_BBL(const amrex::MultiFab &u, const amrex::MultiFab &v,
                              const amrex::MultiFab &h, ViscFields & /*visc*/) const {
  // MOM6 reads the velocities and thicknesses one point into the halo.
  check_field(u, Stagger::XFace, 1, "set_viscous_BBL", "u");
  check_field(v, Stagger::YFace, 1, "set_viscous_BBL", "v");
  check_field(h, Stagger::Cell, 1, "set_viscous_BBL", "h");

  // todo: call the kernel of MOM6's set_viscous_BBL once it is ported. It
  //       sets bbl_thick_u/v, kv_bbl_u/v, ustar_BBL and BBL_meanKE_loss in
  //       visc, and Ray_u/v with DRAG_AS_BODY_FORCE, and reads the open
  //       interface fractions of pbv. The call would look like this:
  //
  // for (amrex::MFIter mfi(h); mfi.isValid(); ++mfi) {
  //   MOM::set_viscous_bbl_tr(mfi.validbox(), mom_const_array(u, mfi),
  //                           mom_const_array(v, mfi), mom_const_array(h, mfi),
  //                           /* the grid and pbv fields, GV and the options */,
  //                           mom_array(visc.bbl_thick_u, mfi), ...);
  // }
}

void SetVisc::set_viscous_ML(const amrex::MultiFab & /*u*/, const amrex::MultiFab & /*v*/,
                             const amrex::MultiFab & /*h*/, const MechForcing & /*forces*/,
                             const amrex::Real /*dt*/, ViscFields & /*visc*/) const {
  // MOM6 returns here without DYNAMIC_VISCOUS_ML or ice shelves.
}

} // namespace MOM
