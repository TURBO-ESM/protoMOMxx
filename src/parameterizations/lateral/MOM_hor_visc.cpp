#include "MOM_hor_visc.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string_view>
#include <vector>

#include <AMReX_GpuDevice.H>
#include <AMReX_MFIter.H>

#include "MOM_domains.hpp"
#include "MOM_logger.hpp"

namespace MOM {

namespace {

/// @brief The parameters that set the Laplacian metric fields. MOM6 reads
/// them in hor_visc_init and does not keep them.
struct LaplacianParams {
  amrex::Real Kh = 0.0;              ///< KH [L2 T-1 ~> m2 s-1].
  amrex::Real Kh_vel_scale = 0.0;    ///< KH_VEL_SCALE [L T-1 ~> m s-1].
  amrex::Real Kh_sin_lat = 0.0;      ///< KH_SIN_LAT [L2 T-1 ~> m2 s-1].
  amrex::Real Kh_pwr_of_sine = 0.0;  ///< KH_PWR_OF_SINE [nondim].
  amrex::Real Smag_Lap_const = 0.0;  ///< SMAG_LAP_CONST [nondim].
  amrex::Real bound_coef = 0.0;      ///< HORVISC_BOUND_COEF [nondim].
  amrex::Real Idt = 0.0;             ///< The inverse of DT [T-1 ~> s-1].
};

/// @brief Create the metric fields of the Laplacian viscosity in hv and
/// compute them, as MOM6's hor_visc_init does, over the same index ranges.
void set_laplacian_metrics(const Domain &domain, const LaplacianParams &p, HorVisc &hv) {
  const Grid &G = hv.G;
  const hv_opts_CS_C &opts = hv.opts;
  hv.dx2h = domain.make_h_field({.nk = 1});
  hv.dy2h = domain.make_h_field({.nk = 1});
  hv.dx2q = domain.make_q_field({.nk = 1});
  hv.dy2q = domain.make_q_field({.nk = 1});
  hv.dx_dyT = domain.make_h_field({.nk = 1});
  hv.dy_dxT = domain.make_h_field({.nk = 1});
  hv.dx_dyBu = domain.make_q_field({.nk = 1});
  hv.dy_dxBu = domain.make_q_field({.nk = 1});
  hv.grid_sp_h2 = domain.make_h_field({.nk = 1});
  hv.Kh_bg_xx = domain.make_h_field({.nk = 1});
  hv.Kh_bg_xy = domain.make_q_field({.nk = 1});
  if (opts.bound_Kh) {
    hv.Kh_Max_xx = domain.make_h_field({.nk = 1});
    hv.Kh_Max_xy = domain.make_q_field({.nk = 1});
  }
  if (opts.Smagorinsky_Kh) {
    hv.Laplac2_const_xx = domain.make_h_field({.nk = 1});
    hv.Laplac2_const_xy = domain.make_q_field({.nk = 1});
  }
  hv.reduction_xx = domain.make_h_field({.nk = 1});
  hv.reduction_xy = domain.make_q_field({.nk = 1});

  {
    const amrex::Gpu::SyncAtExitOnly region;
    hv.dx2h.setVal(0.0);
    hv.dy2h.setVal(0.0);
    hv.dx2q.setVal(0.0);
    hv.dy2q.setVal(0.0);
    hv.dx_dyT.setVal(0.0);
    hv.dy_dxT.setVal(0.0);
    hv.dx_dyBu.setVal(0.0);
    hv.dy_dxBu.setVal(0.0);
    hv.grid_sp_h2.setVal(0.0);
    hv.Kh_bg_xx.setVal(0.0);
    hv.Kh_bg_xy.setVal(0.0);
    if (opts.bound_Kh) {
      hv.Kh_Max_xx.setVal(0.0);
      hv.Kh_Max_xy.setVal(0.0);
    }
    if (opts.Smagorinsky_Kh) {
      hv.Laplac2_const_xx.setVal(0.0);
      hv.Laplac2_const_xy.setVal(0.0);
    }
    hv.reduction_xx.setVal(0.0);
    hv.reduction_xy.setVal(0.0);
  }

  const bool bound_Kh = opts.bound_Kh;
  const bool Smagorinsky_Kh = opts.Smagorinsky_Kh;
  const amrex::Real deg2rad = std::numbers::pi_v<amrex::Real> / 180.0;

  for (amrex::MFIter mfi(hv.dx2h); mfi.isValid(); ++mfi) {
    // The loops below take MOM6's index ranges, with is:ie and js:je the
    // valid h points and Ieq = ie, Jeq = je in symmetric memory.
    const amrex::Box bx = mfi.validbox();

    const amrex::Array4<const amrex::Real> dxT = mom_const_array(G.dxT(), mfi);
    const amrex::Array4<const amrex::Real> dyT = mom_const_array(G.dyT(), mfi);
    const amrex::Array4<const amrex::Real> IdxT = mom_const_array(G.IdxT(), mfi);
    const amrex::Array4<const amrex::Real> IdyT = mom_const_array(G.IdyT(), mfi);
    const amrex::Array4<const amrex::Real> geoLatT = mom_const_array(G.geoLatT(), mfi);
    const amrex::Array4<const amrex::Real> dyCu = mom_const_array(G.dyCu(), mfi);
    const amrex::Array4<const amrex::Real> dy_Cu = mom_const_array(G.dy_Cu(), mfi);
    const amrex::Array4<const amrex::Real> IdxCu = mom_const_array(G.IdxCu(), mfi);
    const amrex::Array4<const amrex::Real> IdyCu = mom_const_array(G.IdyCu(), mfi);
    const amrex::Array4<const amrex::Real> IareaCu = mom_const_array(G.IareaCu(), mfi);
    const amrex::Array4<const amrex::Real> dxCv = mom_const_array(G.dxCv(), mfi);
    const amrex::Array4<const amrex::Real> dx_Cv = mom_const_array(G.dx_Cv(), mfi);
    const amrex::Array4<const amrex::Real> IdxCv = mom_const_array(G.IdxCv(), mfi);
    const amrex::Array4<const amrex::Real> IdyCv = mom_const_array(G.IdyCv(), mfi);
    const amrex::Array4<const amrex::Real> IareaCv = mom_const_array(G.IareaCv(), mfi);
    const amrex::Array4<const amrex::Real> dxBu = mom_const_array(G.dxBu(), mfi);
    const amrex::Array4<const amrex::Real> dyBu = mom_const_array(G.dyBu(), mfi);
    const amrex::Array4<const amrex::Real> IdxBu = mom_const_array(G.IdxBu(), mfi);
    const amrex::Array4<const amrex::Real> IdyBu = mom_const_array(G.IdyBu(), mfi);
    const amrex::Array4<const amrex::Real> geoLatBu = mom_const_array(G.geoLatBu(), mfi);

    const amrex::Array4<amrex::Real> dx2h = mom_array(hv.dx2h, mfi);
    const amrex::Array4<amrex::Real> dy2h = mom_array(hv.dy2h, mfi);
    const amrex::Array4<amrex::Real> dx2q = mom_array(hv.dx2q, mfi);
    const amrex::Array4<amrex::Real> dy2q = mom_array(hv.dy2q, mfi);
    const amrex::Array4<amrex::Real> DX_dyT = mom_array(hv.dx_dyT, mfi);
    const amrex::Array4<amrex::Real> DY_dxT = mom_array(hv.dy_dxT, mfi);
    const amrex::Array4<amrex::Real> DX_dyBu = mom_array(hv.dx_dyBu, mfi);
    const amrex::Array4<amrex::Real> DY_dxBu = mom_array(hv.dy_dxBu, mfi);
    const amrex::Array4<amrex::Real> reduction_xx = mom_array(hv.reduction_xx, mfi);
    const amrex::Array4<amrex::Real> reduction_xy = mom_array(hv.reduction_xy, mfi);
    const amrex::Array4<amrex::Real> grid_sp_h2_a = mom_array(hv.grid_sp_h2, mfi);
    const amrex::Array4<amrex::Real> Kh_bg_xx = mom_array(hv.Kh_bg_xx, mfi);
    const amrex::Array4<amrex::Real> Kh_bg_xy = mom_array(hv.Kh_bg_xy, mfi);
    const amrex::Array4<amrex::Real> Kh_Max_xx =
        bound_Kh ? mom_array(hv.Kh_Max_xx, mfi) : amrex::Array4<amrex::Real>{};
    const amrex::Array4<amrex::Real> Kh_Max_xy =
        bound_Kh ? mom_array(hv.Kh_Max_xy, mfi) : amrex::Array4<amrex::Real>{};
    const amrex::Array4<amrex::Real> Laplac2_const_xx =
        Smagorinsky_Kh ? mom_array(hv.Laplac2_const_xx, mfi) : amrex::Array4<amrex::Real>{};
    const amrex::Array4<amrex::Real> Laplac2_const_xy =
        Smagorinsky_Kh ? mom_array(hv.Laplac2_const_xy, mfi) : amrex::Array4<amrex::Real>{};

    // J=js-2:Jeq+1, I=is-2:Ieq+1
    amrex::ParallelFor(mom_grow(bx, 2, 1), [=] AMREX_GPU_DEVICE(int I, int J, int) {
      dx2q(I, J, 0) = dxBu(I, J, 0) * dxBu(I, J, 0);
      dy2q(I, J, 0) = dyBu(I, J, 0) * dyBu(I, J, 0);
      DX_dyBu(I, J, 0) = dxBu(I, J, 0) * IdyBu(I, J, 0);
      DY_dxBu(I, J, 0) = dyBu(I, J, 0) * IdxBu(I, J, 0);
    });

    // j=js-2:Jeq+2, i=is-2:Ieq+2
    amrex::ParallelFor(mom_grow(bx, 2, 2), [=] AMREX_GPU_DEVICE(int i, int j, int) {
      dx2h(i, j, 0) = dxT(i, j, 0) * dxT(i, j, 0);
      dy2h(i, j, 0) = dyT(i, j, 0) * dyT(i, j, 0);
      DX_dyT(i, j, 0) = dxT(i, j, 0) * IdyT(i, j, 0);
      DY_dxT(i, j, 0) = dyT(i, j, 0) * IdxT(i, j, 0);
    });

    // The stresses through partly blocked faces are reduced by the open
    // fraction of the face. j=Jsq:Jeq+1, i=Isq:Ieq+1
    amrex::ParallelFor(mom_grow(bx, 1, 1), [=] AMREX_GPU_DEVICE(int i, int j, int) {
      reduction_xx(i, j, 0) = 1.0;
      if ((dy_Cu(i, j, 0) > 0.0) && (dy_Cu(i, j, 0) < dyCu(i, j, 0)) &&
          (dy_Cu(i, j, 0) < dyCu(i, j, 0) * reduction_xx(i, j, 0)))
        reduction_xx(i, j, 0) = dy_Cu(i, j, 0) / (dyCu(i, j, 0));
      if ((dy_Cu(i - 1, j, 0) > 0.0) && (dy_Cu(i - 1, j, 0) < dyCu(i - 1, j, 0)) &&
          (dy_Cu(i - 1, j, 0) < dyCu(i - 1, j, 0) * reduction_xx(i, j, 0)))
        reduction_xx(i, j, 0) = dy_Cu(i - 1, j, 0) / (dyCu(i - 1, j, 0));
      if ((dx_Cv(i, j, 0) > 0.0) && (dx_Cv(i, j, 0) < dxCv(i, j, 0)) &&
          (dx_Cv(i, j, 0) < dxCv(i, j, 0) * reduction_xx(i, j, 0)))
        reduction_xx(i, j, 0) = dx_Cv(i, j, 0) / (dxCv(i, j, 0));
      if ((dx_Cv(i, j - 1, 0) > 0.0) && (dx_Cv(i, j - 1, 0) < dxCv(i, j - 1, 0)) &&
          (dx_Cv(i, j - 1, 0) < dxCv(i, j - 1, 0) * reduction_xx(i, j, 0)))
        reduction_xx(i, j, 0) = dx_Cv(i, j - 1, 0) / (dxCv(i, j - 1, 0));
    });

    // J=js-1:Jeq, I=is-1:Ieq
    amrex::ParallelFor(mom_grow(bx, 1, 0), [=] AMREX_GPU_DEVICE(int I, int J, int) {
      reduction_xy(I, J, 0) = 1.0;
      if ((dy_Cu(I, J, 0) > 0.0) && (dy_Cu(I, J, 0) < dyCu(I, J, 0)) &&
          (dy_Cu(I, J, 0) < dyCu(I, J, 0) * reduction_xy(I, J, 0)))
        reduction_xy(I, J, 0) = dy_Cu(I, J, 0) / (dyCu(I, J, 0));
      if ((dy_Cu(I, J + 1, 0) > 0.0) && (dy_Cu(I, J + 1, 0) < dyCu(I, J + 1, 0)) &&
          (dy_Cu(I, J + 1, 0) < dyCu(I, J + 1, 0) * reduction_xy(I, J, 0)))
        reduction_xy(I, J, 0) = dy_Cu(I, J + 1, 0) / (dyCu(I, J + 1, 0));
      if ((dx_Cv(I, J, 0) > 0.0) && (dx_Cv(I, J, 0) < dxCv(I, J, 0)) &&
          (dx_Cv(I, J, 0) < dxCv(I, J, 0) * reduction_xy(I, J, 0)))
        reduction_xy(I, J, 0) = dx_Cv(I, J, 0) / (dxCv(I, J, 0));
      if ((dx_Cv(I + 1, J, 0) > 0.0) && (dx_Cv(I + 1, J, 0) < dxCv(I + 1, J, 0)) &&
          (dx_Cv(I + 1, J, 0) < dxCv(I + 1, J, 0) * reduction_xy(I, J, 0)))
        reduction_xy(I, J, 0) = dx_Cv(I + 1, J, 0) / (dxCv(I + 1, J, 0));
    });

    // The background viscosity and the static factors of the Smagorinsky
    // viscosity. j=js-1:Jeq+1, i=is-1:Ieq+1
    amrex::ParallelFor(mom_grow(bx, 1, 1), [=] AMREX_GPU_DEVICE(int i, int j, int) {
      const amrex::Real grid_sp_h2 =
          (2.0 * dx2h(i, j, 0) * dy2h(i, j, 0)) / (dx2h(i, j, 0) + dy2h(i, j, 0));
      grid_sp_h2_a(i, j, 0) = grid_sp_h2;
      if (Smagorinsky_Kh) Laplac2_const_xx(i, j, 0) = p.Smag_Lap_const * grid_sp_h2;
      Kh_bg_xx(i, j, 0) = std::max(p.Kh, p.Kh_vel_scale * std::sqrt(grid_sp_h2));
      if (p.Kh_sin_lat > 0.) {
        const amrex::Real slat_fn =
            std::pow(std::abs(std::sin(deg2rad * geoLatT(i, j, 0))), p.Kh_pwr_of_sine);
        Kh_bg_xx(i, j, 0) = std::max(p.Kh_sin_lat * slat_fn, Kh_bg_xx(i, j, 0));
      }
    });

    // J=js-1:Jeq, I=is-1:Ieq
    amrex::ParallelFor(mom_grow(bx, 1, 0), [=] AMREX_GPU_DEVICE(int I, int J, int) {
      const amrex::Real grid_sp_q2 =
          (2.0 * dx2q(I, J, 0) * dy2q(I, J, 0)) / (dx2q(I, J, 0) + dy2q(I, J, 0));
      if (Smagorinsky_Kh) Laplac2_const_xy(I, J, 0) = p.Smag_Lap_const * grid_sp_q2;
      Kh_bg_xy(I, J, 0) = std::max(p.Kh, p.Kh_vel_scale * std::sqrt(grid_sp_q2));
      if (p.Kh_sin_lat > 0.) {
        const amrex::Real slat_fn =
            std::pow(std::abs(std::sin(deg2rad * geoLatBu(I, J, 0))), p.Kh_pwr_of_sine);
        Kh_bg_xy(I, J, 0) = std::max(p.Kh_sin_lat * slat_fn, Kh_bg_xy(I, J, 0));
      }
    });

    // The largest Laplacian viscosities that are stable at this time step.
    // They should avoid overshoots when HORVISC_BOUND_COEF < 1.
    if (bound_Kh) {
      // j=js-1:Jeq+1, i=is-1:Ieq+1
      amrex::ParallelFor(mom_grow(bx, 1, 1), [=] AMREX_GPU_DEVICE(int i, int j, int) {
        const amrex::Real denom = std::max(
            (dy2h(i, j, 0) * DY_dxT(i, j, 0) * (IdyCu(i, j, 0) + IdyCu(i - 1, j, 0)) *
             std::max(IdyCu(i, j, 0) * IareaCu(i, j, 0),
                        IdyCu(i - 1, j, 0) * IareaCu(i - 1, j, 0))),
            (dx2h(i, j, 0) * DX_dyT(i, j, 0) * (IdxCv(i, j, 0) + IdxCv(i, j - 1, 0)) *
             std::max(IdxCv(i, j, 0) * IareaCv(i, j, 0),
                        IdxCv(i, j - 1, 0) * IareaCv(i, j - 1, 0))));
        Kh_Max_xx(i, j, 0) = 0.0;
        if (denom > 0.0) Kh_Max_xx(i, j, 0) = p.bound_coef * 0.25 * p.Idt / denom;
      });
      // J=js-1:Jeq, I=is-1:Ieq
      amrex::ParallelFor(mom_grow(bx, 1, 0), [=] AMREX_GPU_DEVICE(int I, int J, int) {
        const amrex::Real denom = std::max(
            (dx2q(I, J, 0) * DX_dyBu(I, J, 0) * (IdxCu(I, J + 1, 0) + IdxCu(I, J, 0)) *
             std::max(IdxCu(I, J, 0) * IareaCu(I, J, 0),
                        IdxCu(I, J + 1, 0) * IareaCu(I, J + 1, 0))),
            (dy2q(I, J, 0) * DY_dxBu(I, J, 0) * (IdyCv(I + 1, J, 0) + IdyCv(I, J, 0)) *
             std::max(IdyCv(I, J, 0) * IareaCv(I, J, 0),
                        IdyCv(I + 1, J, 0) * IareaCv(I + 1, J, 0))));
        Kh_Max_xy(I, J, 0) = 0.0;
        if (denom > 0.0) Kh_Max_xy(I, J, 0) = p.bound_coef * 0.25 * p.Idt / denom;
      });
    }
  }
}

/// @brief Check a metric field that an option needs: it must exist, and have
/// the stagger and the 2 ghost cells that make_hor_visc fills and the kernel
/// reads.
void check_metric(const amrex::MultiFab &mf, const Stagger stagger, const std::string_view name,
                  const std::string_view option) {
  if (mf.empty()) {
    logger::fatal("horizontal_viscosity: ", option, " needs ", name,
                  ", which make_hor_visc did not create.");
  }
  check_field(mf, stagger, 2, "horizontal_viscosity", name);
}

} // namespace

HorVisc make_hor_visc(const Domain &domain, const Grid &G, const VerticalGrid &GV,
                      RuntimeParams &params) {

  HorVisc hv{.G = G, .GV = GV};
  hv_opts_CS_C &opts = hv.opts;

  params.doc_module("MOM_Zanna_Bolton", "");
  bool use_ZB2020 = false;
  params.get("USE_ZB2020", use_ZB2020,
             {.default_value = false,
              .desc = "If true, turns on Zanna-Bolton-2020 (ZB) "
                      "subgrid momentum parameterization of mesoscale eddies."});
  if (use_ZB2020) {
    // defer: ZB2020_init and the Zanna-Bolton parameterization.
    logger::fatal("make_hor_visc: The Zanna-Bolton parameterization (USE_ZB2020 = True) "
                  "is not implemented yet.");
  }

  params.doc_module("MOM_hor_visc", "");

  params.get("USE_CIRCULATION_IN_HORVISC", opts.use_circulation,
             {.default_value = false,
              .desc = "Use circulation theorem to compute vorticity in horvisc module (for ZB20 or "
                      "Leith)"});

  int default_answer_date = 99991231;
  params.get("DEFAULT_ANSWER_DATE", default_answer_date,
             {.default_value = 99991231,
              .desc = "This sets the default value for the various _ANSWER_DATE parameters.",
              .do_not_log = true});  // MOM6 logs it from MOM.F90.

  bool use_MEKE = false;
  params.get("USE_MEKE", use_MEKE, {.default_value = false, .do_not_log = true});
  if (use_MEKE) {
    // defer: MEKE, with MEKE_BACKSCAT_RO_C, RES_SCALE_MEKE_VISC and the
    //        backscatter of EY24_EBT_BS.
    logger::fatal("make_hor_visc: MEKE (USE_MEKE = True) is not implemented yet.");
  }
  // MOM6 logs HOR_VISC_ANSWER_DATE only with MEKE_BACKSCAT_RO_C, which needs MEKE.
  params.get("HOR_VISC_ANSWER_DATE", opts.answer_date,
             {.default_value = default_answer_date,
              .desc = "The vintage of the order of arithmetic and expressions in the horizontal "
                      "viscosity calculations.  Values between 20190102 and 20241201 recover the "
                      "answers from the end of 2018, while higher values use updated and more robust "
                      "forms of the same expressions.",
              .do_not_log = true});
  if (!GV.Boussinesq()) opts.answer_date = std::max(opts.answer_date, 20241201);

  bool debug = false;
  params.get("DEBUG", debug, {.default_value = false});
  if (debug) {
    // defer: the DEBUG checksums of the viscosity bounds and the viscosities.
    logger::fatal("make_hor_visc: The debugging checksums (DEBUG = True) are not implemented yet.");
  }
  params.get("USE_CONT_THICKNESS", opts.use_cont_thick,
             {.default_value = false,
              .desc = "If true, use thickness at velocity points from continuity solver. This option "
                      "currently only works with split mode."});
  bool use_cont_thick_bug = false;
  params.get("USE_CONT_THICKNESS_BUG", use_cont_thick_bug,
             {.default_value = false,
              .desc = "If true, retain an answer-changing halo update bug when "
                      "USE_CONT_THICKNESS=True.  This is not recommended.",
              .do_not_log = !opts.use_cont_thick});

  params.get("LAPLACIAN", opts.Laplacian,
             {.default_value = false,
              .desc = "If true, use a Laplacian horizontal viscosity."});

  LaplacianParams lap;
  params.get("KH", lap.Kh,
             {.default_value = 0.0,
              .desc = "The background Laplacian horizontal viscosity.",
              .units = "m2 s-1",
              .do_not_log = !opts.Laplacian});
  params.get("KH_BG_MIN", opts.Kh_bg_min,
             {.default_value = 0.0,
              .desc = "The minimum value allowed for Laplacian horizontal viscosity, KH.",
              .units = "m2 s-1",
              .do_not_log = !opts.Laplacian});
  params.get("KH_VEL_SCALE", lap.Kh_vel_scale,
             {.default_value = 0.0,
              .desc = "The velocity scale which is multiplied by the grid "
                      "spacing to calculate the Laplacian viscosity. "
                      "The final viscosity is the largest of this scaled "
                      "viscosity, the Smagorinsky and Leith viscosities, and KH.",
              .units = "m s-1",
              .do_not_log = !opts.Laplacian});
  params.get("KH_SIN_LAT", lap.Kh_sin_lat,
             {.default_value = 0.0,
              .desc = "The amplitude of a latitudinally-dependent background "
                      "viscosity of the form KH_SIN_LAT*(SIN(LAT)**KH_PWR_OF_SINE).",
              .units = "m2 s-1",
              .do_not_log = !opts.Laplacian});
  params.get("KH_PWR_OF_SINE", lap.Kh_pwr_of_sine,
             {.default_value = 4.0,
              .desc = "The power used to raise SIN(LAT) when using a latitudinally "
                      "dependent background viscosity.",
              .units = "nondim",
              .do_not_log = !(opts.Laplacian && (lap.Kh_sin_lat > 0.))});
  params.get("SMAGORINSKY_KH", opts.Smagorinsky_Kh,
             {.default_value = false,
              .desc = "If true, use a Smagorinsky nonlinear eddy viscosity.",
              .do_not_log = !opts.Laplacian});
  if (!opts.Laplacian) opts.Smagorinsky_Kh = false;
  params.get("SMAG_LAP_CONST", lap.Smag_Lap_const,
             {.default_value = 0.0,
              .desc = "The nondimensional Laplacian Smagorinsky constant, "
                      "often 0.15.",
              .units = "nondim",
              .fail_if_missing = opts.Smagorinsky_Kh,
              .do_not_log = !opts.Smagorinsky_Kh});
  params.get("LEITH_KH", opts.Leith_Kh,
             {.default_value = false,
              .desc = "If true, use a Leith nonlinear eddy viscosity.",
              .do_not_log = !opts.Laplacian});
  if (!opts.Laplacian) opts.Leith_Kh = false;
  if (opts.Leith_Kh) {
    // defer: the Leith viscosity, with LEITH_LAP_CONST, its metric fields, the
    //        grid's df_dx and df_dy, and a halo of 3.
    logger::fatal("make_hor_visc: The Leith viscosity (LEITH_KH = True) is not implemented yet.");
  }
  amrex::Real Leith_Lap_const = 0.0;
  params.get("LEITH_LAP_CONST", Leith_Lap_const,
             {.default_value = 0.0,
              .desc = "The nondimensional Laplacian Leith constant, "
                      "often set to 1.0",
              .units = "nondim",
              .do_not_log = !opts.Leith_Kh});
  params.get("RES_SCALE_MEKE_VISC", opts.res_scale_MEKE,
             {.default_value = false,
              .desc = "If true, the viscosity contribution from MEKE is scaled by "
                      "the resolution function.",
              .do_not_log = !(opts.Laplacian && use_MEKE)});
  if (!(opts.Laplacian && use_MEKE)) opts.res_scale_MEKE = false;

  params.get("BOUND_KH", opts.bound_Kh,
             {.default_value = true,
              .desc = "If true, the Laplacian coefficient is locally limited "
                      "to be stable.",
              .do_not_log = !opts.Laplacian});
  params.get("EY24_EBT_BS", opts.EY24_EBT_BS,
             {.default_value = false,
              .desc = "If true, use the backscatter scheme (EBT mode with kill switch) "
                      "developed by Yankovsky et al. (2024). ",
              .do_not_log = !opts.Laplacian});
  if (!opts.Laplacian) opts.bound_Kh = false;
  if (!(opts.Laplacian && use_MEKE)) opts.EY24_EBT_BS = false;
  params.get("ANISOTROPIC_VISCOSITY", opts.anisotropic,
             {.default_value = false,
              .desc = "If true, allow anistropic viscosity in the Laplacian "
                      "horizontal viscosity.",
              .do_not_log = !opts.Laplacian});
  if (!opts.Laplacian) opts.anisotropic = false;
  params.get("ADD_LES_VISCOSITY", opts.add_LES_viscosity,
             {.default_value = false,
              .desc = "If true, adds the viscosity from Smagorinsky and Leith to the "
                      "background viscosity instead of taking the maximum.",
              .do_not_log = !opts.Laplacian});

  params.get("KH_ANISO", opts.Kh_aniso,
             {.default_value = 0.0,
              .desc = "The background Laplacian anisotropic horizontal viscosity.",
              .units = "m2 s-1",
              .do_not_log = !opts.anisotropic});
  int aniso_mode = 0;
  params.get("ANISOTROPIC_MODE", aniso_mode,
             {.default_value = 0,
              .desc = "Selects the mode for setting the direction of anisotropy.\n"
                      "\t 0 - Points along the grid i-direction.\n"
                      "\t 1 - Points towards East.\n"
                      "\t 2 - Points along the flow direction, U/|U|.",
              .do_not_log = !opts.anisotropic});
  std::vector<double> aniso_grid_dir;
  params.get("ANISO_GRID_DIR", aniso_grid_dir,
             {.default_value = std::vector<double>{}, .do_not_log = true});
  if (opts.anisotropic) {
    // defer: the anisotropic viscosity (align_aniso_tensor_to_grid), with
    //        the descriptions of ANISO_GRID_DIR for each ANISOTROPIC_MODE.
    logger::fatal("make_hor_visc: The anisotropic viscosity (ANISOTROPIC_VISCOSITY = True) "
                  "is not implemented yet.");
  }

  params.get("BIHARMONIC", opts.biharmonic,
             {.default_value = true,
              .desc = "If true, use a biharmonic horizontal viscosity. "
                      "BIHARMONIC may be used with LAPLACIAN."});
  if (opts.biharmonic) {
    // defer: the biharmonic viscosity: its metric fields and bounds, with
    //        AH, AH_VEL_SCALE, AH_TIME_SCALE, SMAGORINSKY_AH, LEITH_AH,
    //        BOUND_AH, RE_AH, SMAG_BI_CONST, BOUND_CORIOLIS_BIHARM,
    //        BOUND_CORIOLIS_VEL and LEITH_BI_CONST.
    logger::fatal("make_hor_visc: The biharmonic viscosity (BIHARMONIC = True) "
                  "is not implemented yet.");
  }
  // Without BIHARMONIC, MOM6 reads the options of the biharmonic viscosity
  // without logging them, and turns off SMAGORINSKY_AH, LEITH_AH and BOUND_AH.
  amrex::Real Ah = 0.0;
  params.get("AH", Ah,
             {.default_value = 0.0,
              .desc = "The background biharmonic horizontal viscosity.",
              .units = "m4 s-1",
              .do_not_log = true});
  amrex::Real Ah_vel_scale = 0.0;
  params.get("AH_VEL_SCALE", Ah_vel_scale,
             {.default_value = 0.0,
              .desc = "The velocity scale which is multiplied by the cube of "
                      "the grid spacing to calculate the biharmonic viscosity. "
                      "The final viscosity is the largest of this scaled "
                      "viscosity, the Smagorinsky and Leith viscosities, and AH.",
              .units = "m s-1",
              .do_not_log = true});
  amrex::Real Ah_time_scale = 0.0;
  params.get("AH_TIME_SCALE", Ah_time_scale,
             {.default_value = 0.0,
              .desc = "A time scale whose inverse is multiplied by the fourth "
                      "power of the grid spacing to calculate biharmonic viscosity. "
                      "The final viscosity is the largest of all viscosity "
                      "formulations in use. 0.0 means that it's not used.",
              .units = "s",
              .do_not_log = true});
  params.get("SMAGORINSKY_AH", opts.Smagorinsky_Ah,
             {.default_value = false,
              .desc = "If true, use a biharmonic Smagorinsky nonlinear eddy "
                      "viscosity.",
              .do_not_log = true});
  opts.Smagorinsky_Ah = false;
  params.get("LEITH_AH", opts.Leith_Ah,
             {.default_value = false,
              .desc = "If true, use a biharmonic Leith nonlinear eddy "
                      "viscosity.",
              .do_not_log = true});
  opts.Leith_Ah = false;
  params.get("USE_LEITHY", opts.use_Leithy,
             {.default_value = false,
              .desc = "If true, use a biharmonic Leith nonlinear eddy "
                      "viscosity together with a harmonic backscatter."});
  params.get("BOUND_AH", opts.bound_Ah,
             {.default_value = true,
              .desc = "If true, the biharmonic coefficient is locally limited "
                      "to be stable.",
              .do_not_log = true});
  opts.bound_Ah = false;
  params.get("RE_AH", opts.Re_Ah,
             {.default_value = 0.0,
              .desc = "If nonzero, the biharmonic coefficient is scaled "
                      "so that the biharmonic Reynolds number is equal to this.",
              .units = "nondim",
              .do_not_log = true});

  params.get("BACKSCATTER_UNDERBOUND", opts.backscatter_underbound,
             {.default_value = false,
              .desc = "If true, the bounds on the biharmonic viscosity are allowed to "
                      "increase where the Laplacian viscosity is negative (due to backscatter "
                      "parameterizations) beyond the largest timestep-dependent stable values of "
                      "biharmonic viscosity when no Laplacian viscosity is applied.  The default "
                      "is true for historical reasons, but this option probably should not be used "
                      "because it can contribute to numerical instabilities.",
              .do_not_log = !(opts.bound_Kh && opts.bound_Ah)});

  amrex::Real Smag_bi_const = 0.0;
  params.get("SMAG_BI_CONST", Smag_bi_const,
             {.default_value = 0.0,
              .desc = "The nondimensional biharmonic Smagorinsky constant, "
                      "typically 0.015 - 0.06.",
              .units = "nondim",
              .do_not_log = true});

  params.get("USE_BETA_IN_LEITH", opts.use_beta_in_Leith,
             {.default_value = opts.Leith_Kh,
              .desc = "If true, include the beta term in the Leith nonlinear eddy viscosity.",
              .do_not_log = !(opts.Leith_Kh || opts.Leith_Ah)});
  params.get("MODIFIED_LEITH", opts.Modified_Leith,
             {.default_value = false,
              .desc = "If true, add a term to Leith viscosity which is "
                      "proportional to the gradient of divergence.",
              .do_not_log = !(opts.Leith_Kh || opts.Leith_Ah)});
  params.get("USE_QG_LEITH_VISC", opts.use_QG_Leith_visc,
             {.default_value = false,
              .desc = "If true, use QG Leith nonlinear eddy viscosity.",
              .do_not_log = !(opts.Leith_Kh || opts.Leith_Ah)});
  if (opts.use_QG_Leith_visc && !(opts.Leith_Kh || opts.Leith_Ah)) {
    logger::fatal("MOM_hor_visc.F90, hor_visc_init: "
                  "LEITH_KH or LEITH_AH must be True when USE_QG_LEITH_VISC=True.");
  }

  bool bound_Cor_def = false;
  params.get("BOUND_CORIOLIS", bound_Cor_def, {.default_value = false});
  params.get("BOUND_CORIOLIS_BIHARM", opts.bound_Coriolis,
             {.default_value = bound_Cor_def,
              .desc = "If true use a viscosity that increases with the square "
                      "of the velocity shears, so that the resulting viscous "
                      "drag is of comparable magnitude to the Coriolis terms "
                      "when the velocity differences between adjacent grid "
                      "points is 0.5*BOUND_CORIOLIS_VEL.  The default is the "
                      "value of BOUND_CORIOLIS (or false).",
              .do_not_log = !opts.Smagorinsky_Ah});
  if (!opts.Smagorinsky_Ah) opts.bound_Coriolis = false;
  amrex::Real maxvel = 3.0e8;
  params.get("MAXVEL", maxvel, {.default_value = 3.0e8, .units = "m s-1"});
  amrex::Real bound_Cor_vel = maxvel;
  params.get("BOUND_CORIOLIS_VEL", bound_Cor_vel,
             {.default_value = maxvel,
              .desc = "The velocity scale at which BOUND_CORIOLIS_BIHARM causes "
                      "the biharmonic drag to have comparable magnitude to the "
                      "Coriolis acceleration.  The default is set by MAXVEL.",
              .units = "m s-1",
              .do_not_log = !(opts.Smagorinsky_Ah && opts.bound_Coriolis)});
  amrex::Real Leith_bi_const = 0.0;
  params.get("LEITH_BI_CONST", Leith_bi_const,
             {.default_value = 0.0,
              .desc = "The nondimensional biharmonic Leith constant, "
                      "typical values are thus far undetermined.",
              .units = "nondim",
              .fail_if_missing = opts.Leith_Ah || opts.use_Leithy,
              .do_not_log = !(opts.Leith_Ah || opts.use_Leithy)});
  params.get("USE_LAND_MASK_FOR_HVISC", opts.use_land_mask,
             {.default_value = true,
              .desc = "If true, use the land mask for the computation of thicknesses "
                      "at velocity locations. This eliminates the dependence on arbitrary "
                      "values over land or outside of the domain."});
  params.get("HORVISC_BOUND_COEF", lap.bound_coef,
             {.default_value = 0.8,
              .desc = "The nondimensional coefficient of the ratio of the "
                      "viscosity bounds to the theoretical maximum for "
                      "stability without considering other terms.",
              .units = "nondim",
              .do_not_log = !(opts.bound_Ah || opts.bound_Kh)});
  params.get("KILL_SWITCH_COEF", opts.KS_coef,
             {.default_value = 1.0,
              .desc = "A nondimensional coefficient on the biharmonic viscosity that "
                      "sets the kill switch for backscatter. Default is 1.0.",
              .units = "nondim",
              .do_not_log = !opts.EY24_EBT_BS});
  params.get("NOSLIP", opts.no_slip,
             {.default_value = false,
              .desc = "If true, no slip boundary conditions are used; otherwise "
                      "free slip boundary conditions are assumed. The "
                      "implementation of the free slip BCs on a C-grid is much "
                      "cleaner than the no slip BCs. The use of free slip BCs "
                      "is strongly encouraged, and no slip BCs are not used with "
                      "the biharmonic viscosity."});
  bool use_Kh_bg_2d = false;
  params.get("USE_KH_BG_2D", use_Kh_bg_2d,
             {.default_value = false,
              .desc = "If true, read a file containing 2-d background harmonic "
                      "viscosities. The final viscosity is the maximum of the other "
                      "terms and this background value.",
              .do_not_log = !opts.Laplacian});
  if (!opts.Laplacian) use_Kh_bg_2d = false;
  if (use_Kh_bg_2d) {
    // defer: the background viscosity read from a file, with KH_BG_2D_BUG,
    //        KH_BG_2D_FILENAME and KH_BG_2D_VARNAME.
    logger::fatal("make_hor_visc: The 2-d background viscosity (USE_KH_BG_2D = True) "
                  "is not implemented yet.");
  }
  bool Kh_bg_2d_bug = false;
  params.get("KH_BG_2D_BUG", Kh_bg_2d_bug,
             {.default_value = false,
              .desc = "If true, retain an answer-changing horizontal indexing bug in setting "
                      "the corner-point viscosities when USE_KH_BG_2D=True.  This is "
                      "not recommended.",
              .do_not_log = !use_Kh_bg_2d});
  bool enable_bugs = true;
  params.get("ENABLE_BUGS_BY_DEFAULT", enable_bugs,
             {.default_value = true, .do_not_log = true});  // MOM6 logs it from MOM.F90.
  params.get("FRICTWORK_BUG", opts.FrictWork_bug,
             {.default_value = false,
              .desc = "If true, retain an answer-changing bug in calculating the FrictWork, "
                      "which cancels the h in thickness flux and the h at velocity point. This is "
                      "not recommended."});
  params.get("OBC_SPECIFIED_STRAIN_BUG", opts.OBC_strain_bug,
             {.default_value = true,
              .desc = "If true, recover a bug that specified shear strain option at open boundaries "
                      "cannot be applied."});
  params.get("USE_GME", opts.use_GME,
             {.default_value = false,
              .desc = "If true, use the GM+E backscatter scheme in association \n"
                      "with the Gent and McWilliams parameterization.",
              .do_not_log = true});  // MOM6 logs it from MOM_thickness_diffuse.
  bool split = true;
  params.get("SPLIT", split,
             {.default_value = true,
              .desc = "Use the split time stepping if true.",
              .do_not_log = true});
  if (opts.use_Leithy && !(opts.biharmonic && opts.Laplacian)) {
    logger::fatal("MOM_hor_visc.F90, hor_visc_init: "
                  "LAPLACIAN and BIHARMONIC must both be True when USE_LEITHY=True.");
  }
  params.get("LEITHY_CK", opts.c_K,
             {.default_value = 1.0,
              .desc = "Fraction of biharmonic dissipation that gets backscattered, "
                      "in Leith+E.",
              .units = "nondim",
              .do_not_log = !opts.use_Leithy});
  params.get("SMOOTH_AH", opts.smooth_Ah,
             {.default_value = true,
              .desc = "If true, Ah and m_leithy are smoothed within Leith+E.  This requires "
                      "lots of blocking communications, which can be expensive",
              .do_not_log = !opts.use_Leithy});

  if (opts.use_GME && !split) {
    logger::fatal("ERROR: Currently, USE_GME = True cannot be used with SPLIT=False.");
  }
  if (opts.use_GME) {
    // defer: the GME backscatter, with GME_NUM_SMOOTHINGS, GME_H0,
    //        GME_EFFICIENCY and GME_LIMITER.
    logger::fatal("make_hor_visc: The GME backscatter (USE_GME = True) is not implemented yet.");
  }

  amrex::Real dt = 0.0;
  if (opts.Laplacian || opts.biharmonic) {
    params.get("DT", dt,
               {.desc = "The (baroclinic) dynamics time step.",
                .units = "s",
                .fail_if_missing = true});
    lap.Idt = 1.0 / dt;
  }
  amrex::Real KS_timescale = dt;
  params.get("KILL_SWITCH_TIMESCALE", KS_timescale,
             {.default_value = dt,
              .desc = "A timescale for computing the CFL limit for viscosity "
                      "that determines when backscatter is shut off. Default is DT.",
              .units = "s",
              .do_not_log = !opts.EY24_EBT_BS});

  if (opts.no_slip && opts.biharmonic) {
    logger::fatal("ERROR: NOSLIP and BIHARMONIC cannot be defined "
                  "at the same time in MOM.");
  }
  if (!(opts.Laplacian || opts.biharmonic)) {
    // Only issue inviscid warning if not in single column mode (usually 2x2 domain)
    if (std::max(domain.ni_global(), domain.nj_global()) > 2) {
      logger::warning("hor_visc_init:  It is usually a very bad idea not to use either "
                      "LAPLACIAN or BIHARMONIC viscosity.");
    }
    return hv;  // Neither a Laplacian nor a biharmonic viscosity is used.
  }

  // The metric loops read the grid up to 2 points into the halo.
  check_field(G.mask2dT(), Stagger::Cell, 2, "make_hor_visc", "the grid");
  set_laplacian_metrics(domain, lap, hv);
  return hv;
}

void HorVisc::horizontal_viscosity(const amrex::MultiFab &u, const amrex::MultiFab &v,
                                   const amrex::MultiFab &h, const amrex::MultiFab &uh,
                                   const amrex::MultiFab &vh, amrex::MultiFab &diffu,
                                   amrex::MultiFab &diffv) const {
  if (!(opts.Laplacian || opts.biharmonic)) return;

  // The strain and stress loops reach 2 points into the halo.
  constexpr int stencil = 2;
  check_field(u, Stagger::XFace, stencil, "horizontal_viscosity", "u");
  check_field(v, Stagger::YFace, stencil, "horizontal_viscosity", "v");
  check_field(h, Stagger::Cell, stencil, "horizontal_viscosity", "h");
  check_field(uh, Stagger::XFace, 0, "horizontal_viscosity", "uh");
  check_field(vh, Stagger::YFace, 0, "horizontal_viscosity", "vh");
  check_field(diffu, Stagger::XFace, 0, "horizontal_viscosity", "diffu");
  check_field(diffv, Stagger::YFace, 0, "horizontal_viscosity", "diffv");
  check_field(G.mask2dT(), Stagger::Cell, stencil, "horizontal_viscosity", "the grid");

  // The metric fields that the options need.
  check_metric(dx2h, Stagger::Cell, "dx2h", "LAPLACIAN");
  check_metric(dy2h, Stagger::Cell, "dy2h", "LAPLACIAN");
  check_metric(dx2q, Stagger::Node, "dx2q", "LAPLACIAN");
  check_metric(dy2q, Stagger::Node, "dy2q", "LAPLACIAN");
  check_metric(dx_dyT, Stagger::Cell, "dx_dyT", "LAPLACIAN");
  check_metric(dy_dxT, Stagger::Cell, "dy_dxT", "LAPLACIAN");
  check_metric(dx_dyBu, Stagger::Node, "dx_dyBu", "LAPLACIAN");
  check_metric(dy_dxBu, Stagger::Node, "dy_dxBu", "LAPLACIAN");
  check_metric(reduction_xx, Stagger::Cell, "reduction_xx", "LAPLACIAN");
  check_metric(reduction_xy, Stagger::Node, "reduction_xy", "LAPLACIAN");
  check_metric(grid_sp_h2, Stagger::Cell, "grid_sp_h2", "LAPLACIAN");
  check_metric(Kh_bg_xx, Stagger::Cell, "Kh_bg_xx", "LAPLACIAN");
  check_metric(Kh_bg_xy, Stagger::Node, "Kh_bg_xy", "LAPLACIAN");
  if (opts.bound_Kh) {
    check_metric(Kh_Max_xx, Stagger::Cell, "Kh_Max_xx", "BOUND_KH");
    check_metric(Kh_Max_xy, Stagger::Node, "Kh_Max_xy", "BOUND_KH");
  }
  if (opts.Smagorinsky_Kh) {
    check_metric(Laplac2_const_xx, Stagger::Cell, "Laplac2_const_xx", "SMAGORINSKY_KH");
    check_metric(Laplac2_const_xy, Stagger::Node, "Laplac2_const_xy", "SMAGORINSKY_KH");
  }

  // defer: the open boundaries, which MOM6 passes to the kernel as OBC.

  // The inputs and outputs the kernel takes for options that are off here.
  const amrex::Array4<const amrex::Real> none{};
  const amrex::Array4<amrex::Real> none_out{};
  // MEKE, variable mixing and stochastic terms are off.
  const hv_mixing_dev X{};
  // H_to_RZ is Rho0 in Boussinesq mode. The kernel uses it only for
  // FrictWork and the MEKE source, which are off.
  const amrex::Real H_to_RZ = GV.Rho0();
  constexpr amrex::Real s_to_T = 1.0;

  for (amrex::MFIter mfi(h); mfi.isValid(); ++mfi) {
    const amrex::Box bxH0 = mfi.validbox();
    const amrex::Box bxQ0 = mom_grow(bxH0, 1, 0);

    // df_dx and df_dy are read only by the Leith viscosity.
    const hv_grid_dev Gd{.mask2dT = mom_const_array(G.mask2dT(), mfi),
                         .IareaT = mom_const_array(G.IareaT(), mfi),
                         .IdxT = mom_const_array(G.IdxT(), mfi),
                         .IdyT = mom_const_array(G.IdyT(), mfi),
                         .mask2dCu = mom_const_array(G.mask2dCu(), mfi),
                         .IdxCu = mom_const_array(G.IdxCu(), mfi),
                         .dxCu = mom_const_array(G.dxCu(), mfi),
                         .mask2dCv = mom_const_array(G.mask2dCv(), mfi),
                         .IdyCv = mom_const_array(G.IdyCv(), mfi),
                         .dyCv = mom_const_array(G.dyCv(), mfi),
                         .CoriolisBu = mom_const_array(G.CoriolisBu(), mfi),
                         .IdyCu = mom_const_array(G.IdyCu(), mfi),
                         .IdxCv = mom_const_array(G.IdxCv(), mfi),
                         .mask2dBu = mom_const_array(G.mask2dBu(), mfi),
                         .dxBu = mom_const_array(G.dxBu(), mfi),
                         .dyBu = mom_const_array(G.dyBu(), mfi),
                         .IareaBu = mom_const_array(G.IareaBu(), mfi),
                         .IareaCu = mom_const_array(G.IareaCu(), mfi),
                         .IareaCv = mom_const_array(G.IareaCv(), mfi),
                         .IdxBu = mom_const_array(G.IdxBu(), mfi),
                         .IdyBu = mom_const_array(G.IdyBu(), mfi)};

    const hv_metrics_dev M{.Kh_bg_xx = mom_const_array_or_empty(Kh_bg_xx, mfi),
                           .reduction_xx = mom_const_array_or_empty(reduction_xx, mfi),
                           .Kh_Max_xx = mom_const_array_or_empty(Kh_Max_xx, mfi),
                           .grid_sp_h2 = mom_const_array_or_empty(grid_sp_h2, mfi),
                           .Kh_bg_xy = mom_const_array_or_empty(Kh_bg_xy, mfi),
                           .reduction_xy = mom_const_array_or_empty(reduction_xy, mfi),
                           .Kh_Max_xy = mom_const_array_or_empty(Kh_Max_xy, mfi),
                           .dx2h = mom_const_array_or_empty(dx2h, mfi),
                           .dy2h = mom_const_array_or_empty(dy2h, mfi),
                           .dx_dyT = mom_const_array_or_empty(dx_dyT, mfi),
                           .dy_dxT = mom_const_array_or_empty(dy_dxT, mfi),
                           .dx2q = mom_const_array_or_empty(dx2q, mfi),
                           .dy2q = mom_const_array_or_empty(dy2q, mfi),
                           .dx_dyBu = mom_const_array_or_empty(dx_dyBu, mfi),
                           .dy_dxBu = mom_const_array_or_empty(dy_dxBu, mfi),
                           .Laplac2_const_xx = mom_const_array_or_empty(Laplac2_const_xx, mfi),
                           .Laplac2_const_xy = mom_const_array_or_empty(Laplac2_const_xy, mfi)};

    MOM::horizontal_viscosity_tr(
        bxH0, bxQ0, opts, Gd, M, X, /*OBC=*/nullptr, /*nkblock=*/GV.nk(),
        GV.H_subroundoff(), H_to_RZ, s_to_T, mom_const_array(u, mfi), mom_const_array(v, mfi),
        mom_const_array(h, mfi), mom_const_array(uh, mfi), mom_const_array(vh, mfi),
        mom_array(diffu, mfi), mom_array(diffv, mfi),
        /*hu_cont=*/none, /*hv_cont=*/none, /*u_smooth=*/none, /*v_smooth=*/none,
        /*sh_xx_bt=*/none, /*sh_xy_bt=*/none, /*GME_effic_h=*/none, /*GME_effic_q=*/none,
        /*KH_u_GME=*/none, /*KH_v_GME=*/none, /*GME_coeff_h=*/none_out,
        /*GME_coeff_q=*/none_out, /*str_xx_GME=*/none_out, /*str_xy_GME=*/none_out,
        /*NoSt=*/none_out, /*ShSt=*/none_out, /*FrictWork=*/none_out,
        /*FrictWork_bh=*/none_out, /*FrictWork_GME=*/none_out, /*Ah_h=*/none_out,
        /*Ah_q=*/none_out, /*Kh_h=*/none_out, /*Kh_q=*/none_out, /*grid_Re_Kh=*/none_out,
        /*grid_Re_Ah=*/none_out, /*div_xx_h=*/none_out, /*sh_xx_h=*/none_out,
        /*vort_xy_q=*/none_out, /*sh_xy_q=*/none_out, /*visc_limit_h=*/none_out,
        /*visc_limit_q=*/none_out, /*visc_limit_h_flag=*/none_out,
        /*visc_limit_q_flag=*/none_out, /*visc_limit_h_frac=*/none_out,
        /*visc_limit_q_frac=*/none_out, /*BS_coeff_h=*/none_out, /*BS_coeff_q=*/none_out);
  }
}

} // namespace MOM
