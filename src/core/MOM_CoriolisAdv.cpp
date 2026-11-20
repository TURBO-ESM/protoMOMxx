#include "MOM_CoriolisAdv.hpp"

#include <algorithm>
#include <string>

#include <AMReX_MFIter.H>

#include "MOM_domain_infra.hpp"
#include "MOM_domains.hpp"
#include "MOM_logger.hpp"
#include "MOM_string_utils.hpp"

namespace MOM {

CoriolisAdv make_coriolis_adv(const Grid &G, const VerticalGrid &GV, const PorousBarrier &pbv,
                              RuntimeParams &params) {

  params.doc_module("MOM_CoriolisAdv", "");

  CoriolisAdv cor_adv{.G = G, .GV = GV, .pbv = pbv};
  CorAdv_opts_CS_C &opts = cor_adv.opts;
  Coriolis_scheme_CS_C &scheme = opts.Coriolis_scheme_CS;

  params.get("NOSLIP", opts.no_slip,
             {.default_value = false,
              .desc = "If true, no slip boundary conditions are used; otherwise "
                      "free slip boundary conditions are assumed. The "
                      "implementation of the free slip BCs on a C-grid is much "
                      "cleaner than the no slip BCs. The use of free slip BCs "
                      "is strongly encouraged, and no slip BCs are not used with "
                      "the biharmonic viscosity."});

  params.get("CORIOLIS_EN_DIS", scheme.Coriolis_En_Dis,
             {.default_value = false,
              .desc = "If true, two estimates of the thickness fluxes are used "
                      "to estimate the Coriolis term, and the one that "
                      "dissipates energy relative to the other one is used."});

  std::string tmpstr;
  params.get("CORIOLIS_SCHEME", tmpstr,
             {.default_value = std::string("SADOURNY75_ENERGY"),
              .desc = "CORIOLIS_SCHEME selects the discretization for the "
                      "Coriolis terms. Valid values are: \n"
                      "\t SADOURNY75_ENERGY - Sadourny, 1975; energy cons. \n"
                      "\t ARAKAWA_HSU90     - Arakawa & Hsu, 1990 \n"
                      "\t SADOURNY75_ENSTRO - Sadourny, 1975; enstrophy cons. \n"
                      "\t ARAKAWA_LAMB81    - Arakawa & Lamb, 1981; En. + Enst.\n"
                      "\t ARAKAWA_LAMB_BLEND - A blend of Arakawa & Lamb with \n"
                      "\t                      Arakawa & Hsu and Sadourny energy \n"
                      "\t WENOVI5TH_PV_ENSTRO   - 5th-order WENO PV enstrophy \n"
                      "\t WENOVI3RD_PV_ENSTRO   - 3rd-order WENO PV enstrophy \n"
                      "\t WENOVI7TH_PV_ENSTRO  - 7th-order WENO PV enstrophy \n"});
  tmpstr = string_utils::uppercase(tmpstr);
  if (tmpstr == "SADOURNY75_ENERGY") {
    scheme.Coriolis_Scheme = SADOURNY75_ENERGY;
  } else if (tmpstr == "ARAKAWA_HSU90") {
    scheme.Coriolis_Scheme = ARAKAWA_HSU90;
  } else if (tmpstr == "SADOURNY75_ENSTRO") {
    scheme.Coriolis_Scheme = SADOURNY75_ENSTRO;
  } else if (tmpstr == "ARAKAWA_LAMB81") {
    scheme.Coriolis_Scheme = ARAKAWA_LAMB81;
  } else if (tmpstr == "ARAKAWA_LAMB_BLEND") {
    scheme.Coriolis_Scheme = AL_BLEND;
  } else if (tmpstr == "ROBUST_ENSTRO") {
    scheme.Coriolis_Scheme = ROBUST_ENSTRO;
    scheme.Coriolis_En_Dis = false;
  } else if (tmpstr == "WENOVI7TH_PV_ENSTRO") {
    scheme.Coriolis_Scheme = wenovi7th_PV_ENSTRO;
  } else if (tmpstr == "WENOVI5TH_PV_ENSTRO") {
    scheme.Coriolis_Scheme = wenovi5th_PV_ENSTRO;
  } else if (tmpstr == "WENOVI3RD_PV_ENSTRO") {
    scheme.Coriolis_Scheme = wenovi3rd_PV_ENSTRO;
  } else {
    logger::fatal("CoriolisAdv_init: Unrecognized setting #define CORIOLIS_SCHEME ", tmpstr,
                  " found in input file.");
  }

  const bool use_weno = scheme.Coriolis_Scheme == wenovi7th_PV_ENSTRO ||
                        scheme.Coriolis_Scheme == wenovi5th_PV_ENSTRO ||
                        scheme.Coriolis_Scheme == wenovi3rd_PV_ENSTRO;
  if (use_weno) {
    params.get("WENO_VELOCITY_SMOOTH", opts.weno_velocity_smooth,
               {.default_value = false,
                .desc = "If true, use velocity to compute weighting for WENO. "});
  }

  if (scheme.Coriolis_Scheme == AL_BLEND) {
    params.get("CORIOLIS_BLEND_WT_LIN", opts.wt_lin_blend,
               {.default_value = 0.125,
                .desc = "A weighting value for the ratio of inverse thicknesses, "
                        "beyond which the blending between Sadourny Energy and "
                        "Arakawa & Hsu goes linearly to 0 when CORIOLIS_SCHEME "
                        "is ARAWAKA_LAMB_BLEND. This must be between 1 and 1e-16.",
                .units = "nondim"});
    params.get("CORIOLIS_BLEND_F_EFF_MAX", opts.F_eff_max_blend,
               {.default_value = 4.0,
                .desc = "The factor by which the maximum effective Coriolis "
                        "acceleration from any point can be increased when "
                        "blending different discretizations with the "
                        "ARAKAWA_LAMB_BLEND Coriolis scheme.  This must be "
                        "greater than 2.0 (the max value for Sadourny energy).",
                .units = "nondim"});
    opts.wt_lin_blend = std::min(1.0, std::max(opts.wt_lin_blend, 1e-16));
    if (opts.F_eff_max_blend < 2.0) {
      logger::warning("CoriolisAdv_init: CORIOLIS_BLEND_F_EFF_MAX should be at least 2.");
    }
  }

  // The description depends on the scheme, as in MOM6.
  std::string mesg = "If true, the Coriolis terms at u-points are bounded by "
                     "the four estimates of (f+rv)v from the four neighboring "
                     "v-points, and similarly at v-points.";
  if (scheme.Coriolis_En_Dis && scheme.Coriolis_Scheme == SADOURNY75_ENERGY) {
    mesg += "  This option is "
            "always effectively false with CORIOLIS_EN_DIS defined and "
            "CORIOLIS_SCHEME set to SADOURNY75_ENERGY.";
  } else {
    mesg += "  This option would "
            "have no effect on the SADOURNY Coriolis scheme if it "
            "were possible to use centered difference thickness fluxes.";
  }
  params.get("BOUND_CORIOLIS", opts.bound_Coriolis, {.default_value = false, .desc = mesg});
  if ((scheme.Coriolis_En_Dis && scheme.Coriolis_Scheme == SADOURNY75_ENERGY) ||
      scheme.Coriolis_Scheme == ROBUST_ENSTRO) {
    opts.bound_Coriolis = false;
  }

  params.get("KE_SCHEME", tmpstr,
             {.default_value = std::string("KE_ARAKAWA"),
              .desc = "KE_SCHEME selects the discretization for acceleration "
                      "due to the kinetic energy gradient. Valid values are: \n"
                      "\t KE_ARAKAWA, KE_SIMPLE_GUDONOV, KE_GUDONOV, KE_UP3"});
  tmpstr = string_utils::uppercase(tmpstr);
  if (tmpstr == "KE_ARAKAWA") {
    opts.KE_Scheme = KE_ARAKAWA;
  } else if (tmpstr == "KE_SIMPLE_GUDONOV") {
    opts.KE_Scheme = KE_SIMPLE_GUDONOV;
  } else if (tmpstr == "KE_GUDONOV") {
    opts.KE_Scheme = KE_GUDONOV;
  } else if (tmpstr == "KE_UP3") {
    opts.KE_Scheme = KE_UP3;
  } else {
    logger::fatal("CoriolisAdv_init: #define KE_SCHEME ", tmpstr, " in input file is invalid.");
  }

  if (opts.KE_Scheme == KE_UP3) {
    params.get("KE_USE_LIMITER", opts.KE_use_limiter,
               {.default_value = true,
                .desc = "If true, use Koren limiter for KE_UP3 scheme"});
  }

  params.get("PV_ADV_SCHEME", tmpstr,
             {.default_value = std::string("PV_ADV_CENTERED"),
              .desc = "PV_ADV_SCHEME selects the discretization for PV "
                      "advection. Valid values are: \n"
                      "\t PV_ADV_CENTERED - centered (aka Sadourny, 75) \n"
                      "\t PV_ADV_UPWIND1  - upwind, first order"});
  tmpstr = string_utils::uppercase(tmpstr);
  if (tmpstr == "PV_ADV_CENTERED") {
    opts.PV_Adv_Scheme = PV_ADV_CENTERED;
  } else if (tmpstr == "PV_ADV_UPWIND1") {
    opts.PV_Adv_Scheme = PV_ADV_UPWIND1;
  } else {
    logger::fatal("CoriolisAdv_init: #DEFINE PV_ADV_SCHEME in input file is invalid.");
  }

  // The kernel works on all layers at once, as MOM6 does, when nkblock is 0.
  opts.nkblock = 0;
  // defer: the diagnostics of the Stokes term, the relative vorticity and the
  //        potential vorticity.
  opts.do_CAS_diag = false;
  opts.do_RV_diag = false;
  opts.do_PV_diag = false;

  return cor_adv;
}

int CoriolisAdv::stencil() const {
  int stencil = 2;
  if (opts.Coriolis_scheme_CS.Coriolis_Scheme == wenovi7th_PV_ENSTRO) stencil = 4;
  if (opts.Coriolis_scheme_CS.Coriolis_Scheme == wenovi5th_PV_ENSTRO) stencil = 3;
  return stencil;
}

void CoriolisAdv::CorAdCalc(const amrex::MultiFab &u, const amrex::MultiFab &v,
                            const amrex::MultiFab &h, const amrex::MultiFab &uh,
                            const amrex::MultiFab &vh, amrex::MultiFab &CAu,
                            amrex::MultiFab &CAv) const {
  // Check every field the kernel reads. The grid's fields all have the same
  // halo and always have the right stagger, so mask2dT stands for them. The
  // kernel writes CAu and CAv at the valid points only.
  check_field(u, Stagger::XFace, stencil(), "CorAdCalc", "u");
  check_field(v, Stagger::YFace, stencil(), "CorAdCalc", "v");
  check_field(h, Stagger::Cell, stencil(), "CorAdCalc", "h");
  check_field(uh, Stagger::XFace, stencil(), "CorAdCalc", "uh");
  check_field(vh, Stagger::YFace, stencil(), "CorAdCalc", "vh");
  check_field(CAu, Stagger::XFace, 0, "CorAdCalc", "CAu");
  check_field(CAv, Stagger::YFace, 0, "CorAdCalc", "CAv");
  check_field(G.mask2dT(), Stagger::Cell, stencil(), "CorAdCalc", "the grid");
  check_field(pbv.por_face_areaU, Stagger::XFace, stencil(), "CorAdCalc", "por_face_areaU");
  check_field(pbv.por_face_areaV, Stagger::YFace, stencil(), "CorAdCalc", "por_face_areaV");

  // defer: the open boundaries, which MOM6 passes to the kernel as OBC.

  // The kernel treats an empty Array4 as an absent diagnostic.
  const amrex::Array4<amrex::Real> RV{}, PV{}, CAuS{}, CAvS{};
  const amrex::Array4<amrex::Real> gradKEu{}, gradKEv{}, rv_x_v{}, rv_x_u{};
  // There is no unit scaling yet, so the conversion factors are 1.
  const amrex::Real m_to_L = 1.0;
  const amrex::Real m_s_to_L_T = 1.0;

  for (amrex::MFIter mfi(h); mfi.isValid(); ++mfi) {
    // h is an h field, so its boxes already use MOM6's numbering. The box's
    // cells, as in MOM6's computational domain.
    const amrex::Box bxH0 = mfi.validbox();
    // MOM6's IscB:IecB, JscB:JecB: the corners of those cells, the first
    // ones on their southwest sides.
    const amrex::Box bxQ0 = mom_grow(bxH0, 1, 0);

    MOM::coradcalc_tr(
        bxH0, bxQ0, mom_const_array(u, mfi), mom_const_array(v, mfi), mom_const_array(h, mfi),
        mom_const_array(uh, mfi), mom_const_array(vh, mfi), mom_array(CAu, mfi),
        mom_array(CAv, mfi), RV, PV, CAuS, CAvS, /*OBC=*/nullptr, gradKEu, gradKEv, rv_x_v,
        rv_x_u, mom_const_array(G.mask2dT(), mfi), mom_const_array(G.areaT(), mfi),
        mom_const_array(G.areaCu(), mfi), mom_const_array(G.areaCv(), mfi),
        mom_const_array(G.IareaT(), mfi), mom_const_array(G.mask2dCu(), mfi),
        mom_const_array(G.mask2dCv(), mfi), mom_const_array(G.dyCv(), mfi),
        mom_const_array(G.dxCu(), mfi), mom_const_array(G.dy_Cu(), mfi),
        mom_const_array(G.dx_Cv(), mfi), mom_const_array(G.dyBu(), mfi),
        mom_const_array(G.dxBu(), mfi), mom_const_array(G.mask2dBu(), mfi),
        mom_const_array(G.IareaBu(), mfi), mom_const_array(G.CoriolisBu(), mfi),
        mom_const_array(G.IdxCu(), mfi), mom_const_array(G.IdyCv(), mfi),
        mom_const_array(G.IdxCv(), mfi), mom_const_array(G.IdyCu(), mfi),
        mom_const_array(G.IdxCu_OBCmask(), mfi), mom_const_array(G.IdyCv_OBCmask(), mfi),
        GV.nk(), GV.H_subroundoff(), GV.Angstrom_H(), m_to_L, m_s_to_L_T, opts,
        mom_const_array(pbv.por_face_areaU, mfi), mom_const_array(pbv.por_face_areaV, mfi),
        /*Waves=*/nullptr, /*Stokes_VF=*/false);
  }
}

} // namespace MOM
