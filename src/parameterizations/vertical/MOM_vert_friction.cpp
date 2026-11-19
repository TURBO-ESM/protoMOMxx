#include "MOM_vert_friction.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <AMReX_GpuDevice.H>
#include <AMReX_MFIter.H>

#include "MOM_domains.hpp"
#include "MOM_logger.hpp"

namespace MOM {

namespace {

/// @brief Check a bottom boundary layer field that BOTTOMDRAGLAW needs: it
/// must exist and be at the velocity points.
void check_bbl_field(const amrex::MultiFab &mf, const Stagger stagger, const std::string &name) {
  if (mf.empty()) {
    logger::fatal("vertvisc_coef: BOTTOMDRAGLAW needs ", name,
                  ", which make_set_visc did not create.");
  }
  check_field(mf, stagger, 0, "vertvisc_coef", name);
}

} // namespace

ViscCoupling make_visc_coupling(const Domain &domain, const int nk) {
  ViscCoupling coupling;
  coupling.a_u = domain.make_u_field({.nk = nk + 1});
  coupling.h_u = domain.make_u_field({.nk = nk});
  coupling.a_v = domain.make_v_field({.nk = nk + 1});
  coupling.h_v = domain.make_v_field({.nk = nk});
  {
    const amrex::Gpu::SyncAtExitOnly region;
    coupling.a_u.setVal(0.0);
    coupling.h_u.setVal(0.0);
    coupling.a_v.setVal(0.0);
    coupling.h_v.setVal(0.0);
  }
  return coupling;
}

VertVisc make_vertvisc(const Grid &G, const VerticalGrid &GV, RuntimeParams &params) {

  params.doc_module("MOM_vert_friction", "");

  VertVisc vv{.G = G, .GV = GV};
  coupling_CS_C &cpl = vv.coef_opts.coupling_CS;

  int default_answer_date = 99991231;
  params.get("DEFAULT_ANSWER_DATE", default_answer_date,
             {.default_value = 99991231,
              .desc = "This sets the default value for the various _ANSWER_DATE parameters.",
              .do_not_log = true});  // MOM6 logs it from MOM.F90.
  params.get("VERT_FRICTION_ANSWER_DATE", cpl.answer_date,
             {.default_value = default_answer_date,
              .desc = "The vintage of the order of arithmetic and expressions in the viscous "
                      "calculations.  Values below 20190101 recover the answers from the end of 2018, "
                      "while higher values use expressions that do not use an arbitrary hard-coded "
                      "maximum viscous coupling coefficient between layers.  Values below 20230601 "
                      "recover a form of the viscosity within the mixed layer that breaks up the "
                      "magnitude of the wind stress in some non-Boussinesq cases.",
              .do_not_log = !GV.Boussinesq()});
  if (!GV.Boussinesq()) cpl.answer_date = std::max(cpl.answer_date, 20230701);

  params.get("BOTTOMDRAGLAW", cpl.bottomdraglaw,
             {.default_value = true,
              .desc = "If true, the bottom stress is calculated with a drag "
                      "law of the form c_drag*|u|*u. The velocity magnitude "
                      "may be an assumed value or it may be based on the "
                      "actual velocity in the bottommost HBBL, depending on "
                      "LINEAR_DRAG."});
  params.get("DIRECT_STRESS", vv.opts.direct_stress,
             {.default_value = false,
              .desc = "If true, the wind stress is distributed over the topmost HMIX_STRESS of fluid "
                      "(like in HYCOM), and an added mixed layer viscosity or a physically based "
                      "boundary layer turbulence parameterization is not needed for stability."});
  params.get("DYNAMIC_VISCOUS_ML", cpl.dynamic_viscous_ML,
             {.default_value = false,
              .desc = "If true, use a bulk Richardson number criterion to "
                      "determine the mixed layer thickness for viscosity."});
  if (cpl.dynamic_viscous_ML) {
    // defer: the dynamic viscous mixed layer (nkml_visc_u and nkml_visc_v),
    //        which make_set_visc also rejects.
    logger::fatal("make_vertvisc: The dynamic viscous mixed layer (DYNAMIC_VISCOUS_ML = True) "
                  "is not implemented yet.");
  }
  params.get("FIXED_DEPTH_LOTW_ML", cpl.fixed_LOTW_ML,
             {.default_value = false,
              .desc = "If true, use a Law-of-the-wall prescription for the mixed layer viscosity "
                      "within a boundary layer that is the lesser of HMIX_FIXED and the total "
                      "depth of the ocean in a column."});
  params.get("LOTW_VISCOUS_ML_FLOOR", cpl.apply_LOTW_floor,
             {.default_value = false,
              .desc = "If true, use a Law-of-the-wall prescription to set a lower bound on the "
                      "viscous coupling between layers within the surface boundary layer, based "
                      "the distance of interfaces from the surface.  This only acts when there "
                      "are large changes in the thicknesses of successive layers or when the "
                      "viscosity is set externally and the wind stress has subsequently increased."});
  if (cpl.fixed_LOTW_ML || cpl.apply_LOTW_floor) {
    // defer: the Law-of-the-wall mixed layer viscosity, with the friction
    //        velocity (forces%ustar or tau_mag, through find_ustar).
    logger::fatal("make_vertvisc: The Law-of-the-wall mixed layer viscosity "
                  "(FIXED_DEPTH_LOTW_ML or LOTW_VISCOUS_ML_FLOOR = True) is not implemented yet.");
  }
  params.get("VON_KARMAN_CONST", cpl.vonKar,
             {.default_value = 0.41,
              .desc = "The value the von Karman constant as used for mixed layer viscosity.",
              .units = "nondim"});
  std::string u_trunc_file;
  params.get("U_TRUNC_FILE", u_trunc_file,
             {.default_value = std::string(""),
              .desc = "The absolute path to a file into which the accelerations "
                      "leading to zonal velocity truncations are written. "
                      "Undefine this for efficiency if this diagnostic is not needed.",
              .debugging_param = true});
  std::string v_trunc_file;
  params.get("V_TRUNC_FILE", v_trunc_file,
             {.default_value = std::string(""),
              .desc = "The absolute path to a file into which the accelerations "
                      "leading to meridional velocity truncations are written. "
                      "Undefine this for efficiency if this diagnostic is not needed.",
              .debugging_param = true});
  if (!u_trunc_file.empty() || !v_trunc_file.empty()) {
    // defer: the reports of the velocity truncations (PointAccel_init), and
    //        the do_report_u and do_report_v options of the kernel.
    logger::fatal("make_vertvisc: The velocity truncation reports (U_TRUNC_FILE, V_TRUNC_FILE) "
                  "are not implemented yet.");
  }
  params.get("HARMONIC_VISC", vv.coef_opts.harmonic_visc,
             {.default_value = false,
              .desc = "If true, use the harmonic mean thicknesses for "
                      "calculating the vertical viscosity."});
  params.get("HARMONIC_BL_SCALE", vv.coef_opts.harm_BL_val,
             {.default_value = 0.0,
              .desc = "A scale to determine when water is in the boundary "
                      "layers based solely on harmonic mean thicknesses for "
                      "the purpose of determining the extent to which the "
                      "thicknesses used in the viscosities are upwinded.",
              .units = "nondim"});
  bool debug = false;
  params.get("DEBUG", debug, {.default_value = false});
  if (debug) {
    // defer: the DEBUG checksums of the vertical viscosity.
    logger::fatal("make_vertvisc: The debugging checksums (DEBUG = True) are not "
                  "implemented yet.");
  }

  // MOM6 reads HMIX_FIXED and KV_ML_INVZ2 when GV%nkml < 1, i.e. without the
  // bulk mixed layer, which VerticalGrid defers. Its nkml is 0 here.
  params.get("HMIX_FIXED", cpl.Hmix,
             {.desc = "The prescribed depth over which the near-surface viscosity and "
                      "diffusivity are elevated when the bulk mixed layer is not used.",
              .units = "m",
              .fail_if_missing = true});
  if (vv.opts.direct_stress) {
    params.get("HMIX_STRESS", vv.opts.Hmix_stress,
               {.default_value = cpl.Hmix,
                .desc = "The depth over which the wind stress is applied if DIRECT_STRESS is true.",
                .units = "m"});
    if (vv.opts.Hmix_stress <= 0.0) {
      logger::fatal("vertvisc_init: HMIX_STRESS must be set to a positive value if "
                    "DIRECT_STRESS is true.");
    }
  }
  params.get("KV", cpl.Kv,
             {.desc = "The background kinematic viscosity in the interior. "
                      "The molecular value, ~1e-6 m2 s-1, may be used.",
              .units = "m2 s-1",
              .fail_if_missing = true});

  params.get("USE_GL90_IN_SSW", vv.coef_opts.use_GL90_in_SSW,
             {.default_value = false,
              .desc = "If true, use simpler method to calculate 1/N^2 in GL90 vertical "
                      "viscosity coefficient. This method is valid in stacked shallow water mode."});
  if (vv.coef_opts.use_GL90_in_SSW) {
    // defer: the GL90 vertical viscosity in stacked shallow water mode, with
    //        its coupling coefficients (a_u_gl90, a_v_gl90) and g_prime.
    logger::fatal("make_vertvisc: The GL90 vertical viscosity (USE_GL90_IN_SSW = True) "
                  "is not implemented yet.");
  }
  GL90_opts &gl90 = vv.coef_opts.GL90_CS;
  params.get("KD_GL90", gl90.kappa_gl90,
             {.default_value = 0.0,
              .desc = "The scalar diffusivity used in GL90 vertical viscosity scheme.",
              .units = "m2 s-1",
              .do_not_log = !vv.coef_opts.use_GL90_in_SSW});
  params.get("READ_KD_GL90", gl90.read_kappa_gl90,
             {.default_value = false,
              .desc = "If true, read a file (given by KD_GL90_FILE) containing the "
                      "spatially varying diffusivity KD_GL90 used in the GL90 scheme.",
              .do_not_log = !vv.coef_opts.use_GL90_in_SSW});
  if (gl90.read_kappa_gl90) {
    // defer: the GL90 diffusivity read from a file, with KD_GL90_FILE and
    //        KD_GL90_VARIABLE.
    logger::fatal("make_vertvisc: The GL90 diffusivity from a file (READ_KD_GL90 = True) "
                  "is not implemented yet.");
  }
  params.get("USE_GL90_N2", gl90.use_GL90_N2,
             {.default_value = false,
              .desc = "If true, use GL90 vertical viscosity coefficient that is depth-independent; "
                      "this corresponds to a kappa_GM that scales as N^2 with depth.",
              .do_not_log = !vv.coef_opts.use_GL90_in_SSW});
  if (gl90.use_GL90_N2 && !vv.coef_opts.use_GL90_in_SSW) {
    logger::fatal("MOM_vert_friction.F90, vertvisc_init: "
                  "When USE_GL90_N2=True, USE_GL90_in_SSW must also be True.");
  }
  params.get("HBBL_GL90", vv.coef_opts.Hbbl_gl90,
             {.default_value = 5.0,
              .desc = "The thickness of the GL90 bottom boundary layer, "
                      "which defines the range over which the GL90 coupling "
                      "coefficient is zeroed out, in order to avoid fluxing "
                      "momentum into vanished layers over steep topography.",
              .units = "m",
              .do_not_log = !vv.coef_opts.use_GL90_in_SSW});

  amrex::Real Kv_mks = -1.0;
  params.get("KVML", Kv_mks,
             {.default_value = -1.0,
              .desc = "The scale for an extra kinematic viscosity in the mixed layer",
              .units = "m2 s-1",
              .do_not_log = true});
  if (Kv_mks >= 0.0) {
    logger::warning("KVML is a deprecated parameter. Use KV_ML_INVZ2 instead.");
  } else {
    Kv_mks = 0.0;
  }
  params.get("KV_ML_INVZ2", cpl.Kvml_invZ2,
             {.default_value = Kv_mks,
              .desc = "An extra kinematic viscosity in a mixed layer of thickness HMIX_FIXED, "
                      "with the actual viscosity scaling as 1/(z*HMIX_FIXED)^2, where z is the "
                      "distance from the surface, to allow for finite wind stresses to be "
                      "transmitted through infinitesimally thin surface layers.  This is an "
                      "older option for numerical convenience without a strong physical basis, "
                      "and its use is now discouraged.",
              .units = "m2 s-1"});

  if (!cpl.bottomdraglaw) {
    params.get("KV_EXTRA_BBL", cpl.Kv_extra_bbl,
               {.default_value = 0.0,
                .desc = "An extra kinematic viscosity in the benthic boundary layer. "
                        "KV_EXTRA_BBL is not used if BOTTOMDRAGLAW is true.",
                .units = "m2 s-1",
                .do_not_log = true});
    if (cpl.Kv_extra_bbl == 0.0) {
      amrex::Real Kv_BBL = cpl.Kv;
      params.get("KVBBL", Kv_BBL,
                 {.default_value = cpl.Kv,
                  .desc = "An extra kinematic viscosity in the benthic boundary layer. "
                          "KV_EXTRA_BBL is not used if BOTTOMDRAGLAW is true.",
                  .units = "m2 s-1",
                  .do_not_log = true});
      if (std::abs(Kv_BBL - cpl.Kv) > 1.0e-15 * std::abs(cpl.Kv)) {
        logger::warning("KVBBL is a deprecated parameter. Use KV_EXTRA_BBL instead.");
        cpl.Kv_extra_bbl = Kv_BBL - cpl.Kv;
      }
    }
    // MOM6 logs the value it settled on (log_param).
    params.get("KV_EXTRA_BBL", cpl.Kv_extra_bbl,
               {.default_value = 0.0,
                .desc = "An extra kinematic viscosity in the benthic boundary layer. "
                        "KV_EXTRA_BBL is not used if BOTTOMDRAGLAW is true.",
                .units = "m2 s-1",
                .do_not_read = true});
  }
  params.get("HBBL", vv.coef_opts.Hbbl,
             {.desc = "The thickness of a bottom boundary layer with a viscosity increased by "
                      "KV_EXTRA_BBL if BOTTOMDRAGLAW is not defined, or the thickness over which "
                      "near-bottom velocities are averaged for the drag law if BOTTOMDRAGLAW is "
                      "defined but LINEAR_DRAG is not.",
              .units = "m",
              .fail_if_missing = true});
  params.get("CFL_TRUNCATE", vv.opts.CFL_trunc,
             {.default_value = 0.5,
              .desc = "The value of the CFL number that will cause velocity "
                      "components to be truncated; instability can occur past 0.5.",
              .units = "nondim"});
  params.get("CFL_REPORT", vv.opts.CFL_report,
             {.default_value = vv.opts.CFL_trunc,
              .desc = "The value of the CFL number that causes accelerations "
                      "to be reported; the default is CFL_TRUNCATE.",
              .units = "nondim"});
  amrex::Real trunc_ramp_time = 0.0;
  params.get("CFL_TRUNCATE_RAMP_TIME", trunc_ramp_time,
             {.default_value = 0.,
              .desc = "The time over which the CFL truncation value is ramped "
                      "up at the beginning of the run.",
              .units = "s"});
  if (trunc_ramp_time > 0.0) {
    // defer: the ramp of the CFL truncation value (updateCFLtruncationValue),
    //        which the time step calls with the model time.
    logger::fatal("make_vertvisc: The ramp of the CFL truncation value "
                  "(CFL_TRUNCATE_RAMP_TIME > 0) is not implemented yet.");
  }
  amrex::Real CFL_truncS = 0.0;
  params.get("CFL_TRUNCATE_START", CFL_truncS,
             {.default_value = 0.,
              .desc = "The start value of the truncation CFL number used when "
                      "ramping up CFL_TRUNC.",
              .units = "nondim"});
  params.get("STOKES_MIXING_COMBINED", vv.opts.StokesMixing,
             {.default_value = false,
              .desc = "Flag to use Stokes drift Mixing via the Lagrangian "
                      " current (Eulerian plus Stokes drift). "
                      " Still needs work and testing, so not recommended for use."});
  if (vv.opts.StokesMixing) {
    logger::fatal("Stokes mixing requires user intervention in the code.\n"
                  "  Model now exiting.  See MOM_vert_friction.F90 for \n"
                  "  details (search 'BGR 04/04/2018' to locate comment).");
  }
  params.get("VEL_UNDERFLOW", vv.opts.vel_underflow,
             {.default_value = 0.0,
              .desc = "A negligibly small velocity magnitude below which velocity "
                      "components are set to 0.  A reasonable value might be "
                      "1e-30 m/s, which is less than an Angstrom divided by "
                      "the age of the universe.",
              .units = "m s-1"});

  return vv;
}

void VertVisc::vertvisc_coef(const amrex::MultiFab &u, const amrex::MultiFab &v,
                             const amrex::MultiFab &h, const amrex::MultiFab &dz,
                             const ViscFields &visc, const amrex::Real dt,
                             ViscCoupling &coupling) const {
  // The thicknesses at a velocity point come from the h points on both sides.
  check_field(u, Stagger::XFace, 0, "vertvisc_coef", "u");
  check_field(v, Stagger::YFace, 0, "vertvisc_coef", "v");
  check_field(h, Stagger::Cell, 1, "vertvisc_coef", "h");
  check_field(dz, Stagger::Cell, 1, "vertvisc_coef", "dz");
  check_field(G.mask2dT(), Stagger::Cell, 1, "vertvisc_coef", "the grid");
  check_field(coupling.a_u, Stagger::XFace, 0, "vertvisc_coef", "a_u");
  check_field(coupling.h_u, Stagger::XFace, 0, "vertvisc_coef", "h_u");
  check_field(coupling.a_v, Stagger::YFace, 0, "vertvisc_coef", "a_v");
  check_field(coupling.h_v, Stagger::YFace, 0, "vertvisc_coef", "h_v");
  if (coef_opts.coupling_CS.bottomdraglaw) {
    check_bbl_field(visc.bbl_thick_u, Stagger::XFace, "bbl_thick_u");
    check_bbl_field(visc.bbl_thick_v, Stagger::YFace, "bbl_thick_v");
    check_bbl_field(visc.kv_bbl_u, Stagger::XFace, "kv_bbl_u");
    check_bbl_field(visc.kv_bbl_v, Stagger::YFace, "kv_bbl_v");
  }

  // defer: the open boundaries, which MOM6 passes to the kernel as OBC.

  // The thickness-unit conversion factors at their Boussinesq values without
  // rescaling. VerticalGrid defers them to the thickness/units layer.
  constexpr amrex::Real H_to_m = 1.0;
  constexpr amrex::Real m_to_H = 1.0;
  constexpr amrex::Real Z_to_H = 1.0;
  constexpr amrex::Real T_to_s = 1.0;
  const amrex::Real RZ_to_H = 1.0 / GV.Rho0();
  const amrex::Real dZ_subroundoff = 1e-20 * std::max(GV.Angstrom_Z(), 1e-17);
  // MOM6's GV%nkml, the number of layers in the bulk mixed layer, which
  // VerticalGrid defers.
  constexpr int nkml = 0;

  // The inputs and outputs the kernel takes for options that are off here.
  const amrex::Array4<const amrex::Real> none{};
  const amrex::Array4<amrex::Real> none_out{};

  for (amrex::MFIter mfi(h); mfi.isValid(); ++mfi) {
    // MOM6's IscB:IecB, jsc:jec and isc:iec, JscB:JecB.
    const amrex::Box bxU0 = amrex::growLo(mfi.validbox(), 0, 1);
    const amrex::Box bxV0 = amrex::growLo(mfi.validbox(), 1, 1);

    // The ice shelf fields, the mixed layer fields of the dynamic viscous
    // mixed layer and the diabatic schemes, and SpV_avg are left out, as
    // make_set_visc doesn't create them.
    MOM::vertvisc_coef_tr(
        bxU0, bxV0, mom_const_array(u, mfi), mom_const_array(v, mfi), mom_const_array(h, mfi),
        mom_const_array(dz, mfi), /*frac_shelf_u=*/none, /*frac_shelf_v=*/none,
        /*Ustar_2d=*/none, mom_const_array_or_empty(visc.kv_bbl_u, mfi),
        mom_const_array_or_empty(visc.kv_bbl_v, mfi),
        mom_const_array_or_empty(visc.bbl_thick_u, mfi),
        mom_const_array_or_empty(visc.bbl_thick_v, mfi), /*tbl_thick_shelf_u=*/none,
        /*tbl_thick_shelf_v=*/none, /*Kv_tbl_shelf_u=*/none, /*Kv_tbl_shelf_v=*/none,
        /*nkml_visc_u=*/none, /*nkml_visc_v=*/none, /*Kv_shear=*/none, /*Kv_shear_Bu=*/none,
        /*SpV_avg=*/none, dt, mom_const_array(G.mask2dCu(), mfi),
        mom_const_array(G.mask2dCv(), mfi), mom_const_array(G.bathyT(), mfi),
        mom_const_array(G.CoriolisBu(), mfi), GV.nk(), GV.Boussinesq(), nkml, dZ_subroundoff,
        GV.H_subroundoff(), H_to_m, m_to_H, RZ_to_H, Z_to_H, T_to_s, /*g_prime=*/none,
        coef_opts, /*kappa_gl90_2d=*/none, mom_array(coupling.a_u, mfi), /*a_u_gl90=*/none_out,
        mom_array(coupling.h_u, mfi), mom_array(coupling.a_v, mfi), /*a_v_gl90=*/none_out,
        mom_array(coupling.h_v, mfi), /*a1_shelf_u=*/none_out, /*a1_shelf_v=*/none_out,
        /*OBC=*/nullptr, /*kdgl90_struct=*/none, /*hML_u=*/none_out, /*hML_v=*/none_out,
        /*Kv_u=*/none_out, /*Kv_v=*/none_out, /*Kv_gl90_u=*/none_out, /*Kv_gl90_v=*/none_out);
  }
}

void VertVisc::vertvisc(amrex::MultiFab &u, amrex::MultiFab &v, const amrex::MultiFab &h,
                        const MechForcing &forces, const ViscFields &visc, const amrex::Real dt,
                        const ViscCoupling &coupling) const {
  check_field(u, Stagger::XFace, 0, "vertvisc", "u");
  check_field(v, Stagger::YFace, 0, "vertvisc", "v");
  check_field(h, Stagger::Cell, 0, "vertvisc", "h");
  check_field(forces.taux, Stagger::XFace, 0, "vertvisc", "taux");
  check_field(forces.tauy, Stagger::YFace, 0, "vertvisc", "tauy");
  // The velocity truncation reads the cell areas next to each velocity point.
  check_field(G.mask2dT(), Stagger::Cell, 1, "vertvisc", "the grid");
  check_field(coupling.a_u, Stagger::XFace, 0, "vertvisc", "a_u");
  check_field(coupling.h_u, Stagger::XFace, 0, "vertvisc", "h_u");
  check_field(coupling.a_v, Stagger::YFace, 0, "vertvisc", "a_v");
  check_field(coupling.h_v, Stagger::YFace, 0, "vertvisc", "h_v");

  // defer: the open boundaries, which MOM6 passes to the kernel as OBC.

  // The thickness-unit conversion factors at their Boussinesq values without
  // rescaling. VerticalGrid defers them to the thickness/units layer.
  const amrex::Real H_to_RZ = GV.Rho0();
  constexpr amrex::Real m_s_to_L_T = 1.0;

  // The inputs and outputs the kernel takes for options that are off here.
  const amrex::Array4<const amrex::Real> none{};
  const amrex::Array4<amrex::Real> none_out{};
  const amrex::Array4<int> none_int{};

  // defer: the count of truncated velocities, which MOM6 keeps (ntrunc) for
  //        MOM_sum_output to report.
  int ntrunc = 0;

  for (amrex::MFIter mfi(h); mfi.isValid(); ++mfi) {
    // MOM6's IscB:IecB, jsc:jec and isc:iec, JscB:JecB.
    const amrex::Box bxU0 = amrex::growLo(mfi.validbox(), 0, 1);
    const amrex::Box bxV0 = amrex::growLo(mfi.validbox(), 1, 1);

    MOM::vertvisc_tr(
        bxU0, bxV0, mom_array(u, mfi), mom_array(v, mfi), mom_const_array(h, mfi),
        mom_const_array(forces.taux, mfi), mom_const_array(forces.tauy, mfi),
        mom_const_array_or_empty(visc.Ray_u, mfi), mom_const_array_or_empty(visc.Ray_v, mfi),
        /*taux_shelf=*/none_out, /*tauy_shelf=*/none_out, dt, /*OBC=*/nullptr,
        /*du_dt_visc=*/none_out, /*dv_dt_visc=*/none_out, /*du_dt_visc_gl90=*/none_out,
        /*dv_dt_visc_gl90=*/none_out, /*du_dt_str=*/none_out, /*dv_dt_str=*/none_out,
        mom_const_array(G.mask2dCu(), mfi), mom_const_array(G.mask2dCv(), mfi),
        mom_const_array(G.areaCu(), mfi), mom_const_array(G.areaCv(), mfi),
        mom_const_array(G.areaT(), mfi), mom_const_array(G.IareaT(), mfi),
        mom_const_array(G.dx_Cv(), mfi), mom_const_array(G.dy_Cu(), mfi), GV.nk(),
        GV.H_subroundoff(), GV.Angstrom_H(), H_to_RZ, m_s_to_L_T, opts,
        mom_const_array(coupling.a_u, mfi), /*a_u_gl90=*/none, mom_const_array(coupling.h_u, mfi),
        mom_const_array(coupling.a_v, mfi), /*a_v_gl90=*/none, mom_const_array(coupling.h_v, mfi),
        /*a1_shelf_u=*/none, /*a1_shelf_v=*/none, /*taux_bot=*/none_out, /*tauy_bot=*/none_out,
        /*KE_u=*/none_out, /*KE_v=*/none_out, /*u_old=*/none_out, /*v_old=*/none_out,
        /*vel_report_u=*/none_out, /*vel_report_v=*/none_out, /*dowrite_u=*/none_int,
        /*dowrite_v=*/none_int, ntrunc, /*fpmix=*/false, /*Us_x=*/none, /*Us_y=*/none);
  }
}

} // namespace MOM
