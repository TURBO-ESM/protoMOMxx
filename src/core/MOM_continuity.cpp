#include "MOM_continuity.hpp"

#include <string_view>

#include <AMReX_Gpu.H>
#include <AMReX_MFIter.H>

#include "MOM_domain_infra.hpp"
#include "MOM_logger.hpp"

namespace MOM {

namespace {

/// @brief Stop if a field has the wrong stagger or too few ghost cells.
/// Otherwise it would be read one point off, with no error.
/// @param mf The field.
/// @param stagger The stagger the kernel expects.
/// @param halo The number of ghost cells needed in i and j.
/// @param name The field's name, for the message.
void check_field(const amrex::MultiFab &mf, const Stagger stagger, const int halo,
                 const std::string_view name) {
  if (!has_stagger(mf, stagger)) {
    logger::fatal("continuity: ", name, " does not have the stagger the kernel expects.");
  }
  if (ni_halo(mf) < halo || nj_halo(mf) < halo) {
    logger::fatal("continuity: ", name, " has ", ni_halo(mf), " x ", nj_halo(mf),
                  " ghost cells, and the stencil needs ", halo, ".");
  }
}

} // namespace

Continuity make_continuity(const Grid &G, const VerticalGrid &GV, const PorousBarrier &pbv,
                           RuntimeParams &params) {

  params.doc_module("MOM_continuity_PPM", "");

  Continuity solver{.G = G, .GV = GV, .pbv = pbv};
  reconstruction_CS_C &rec = solver.reconstruction;
  transport_adjust_CS_C &adj = solver.transport_adjust;

  params.get("MONOTONIC_CONTINUITY", rec.monotonic,
             {.default_value = false,
              .desc = "If true, CONTINUITY_PPM uses the Colella and Woodward "
                      "monotonic limiter.  The default (false) is to use a "
                      "simple positive definite limiter."});
  params.get("SIMPLE_2ND_PPM_CONTINUITY", rec.simple_2nd,
             {.default_value = false,
              .desc = "If true, CONTINUITY_PPM uses a simple 2nd order "
                      "(arithmetic mean) interpolation of the edge values. "
                      "This may give better PV conservation properties. While "
                      "it formally reduces the accuracy of the continuity "
                      "solver itself in the strongly advective limit, it does "
                      "not reduce the overall order of accuracy of the dynamic "
                      "core."});
  params.get("UPWIND_1ST_CONTINUITY", rec.upwind_1st,
             {.default_value = false,
              .desc = "If true, CONTINUITY_PPM becomes a 1st-order upwind "
                      "continuity solver.  This scheme is highly diffusive "
                      "but may be useful for debugging or in single-column "
                      "mode where its minimal stencil is useful."});

  // Now that the scheme is known, check that the halo is wide enough for it.
  const int stencil = solver.stencil();
  if (ni_halo(G.mask2dT()) < stencil) {
    logger::fatal("make_continuity: the x-halo needs to be increased by ",
                  stencil - ni_halo(G.mask2dT()), ".");
  }
  if (nj_halo(G.mask2dT()) < stencil) {
    logger::fatal("make_continuity: the y-halo needs to be increased by ",
                  stencil - nj_halo(G.mask2dT()), ".");
  }

  // MOM6's default is 0.5*NK*Angstrom_m. Angstrom_m equals Angstrom_Z here,
  // because there is no unit scaling yet.
  params.get("ETA_TOLERANCE", adj.tol_eta,
             {.default_value = 0.5 * GV.nk() * GV.Angstrom_Z(),
              .desc = "The tolerance for the differences between the "
                      "barotropic and baroclinic estimates of the sea surface "
                      "height due to the fluxes through each face.  The total "
                      "tolerance for SSH is 4 times this value.  The default "
                      "is 0.5*NK*ANGSTROM, and this should not be set less "
                      "than about 10^-15*MAXIMUM_DEPTH.",
              .units = "m"});
  // The default is the speed of light.
  params.get("VELOCITY_TOLERANCE", adj.tol_vel,
             {.default_value = 3.0e8,
              .desc = "The tolerance for barotropic velocity discrepancies "
                      "between the barotropic solution and  the sum of the "
                      "layer thicknesses.",
              .units = "m s-1"});
  params.get("CONT_PPM_AGGRESS_ADJUST", adj.aggress_adjust,
             {.default_value = false,
              .desc = "If true, allow the adjusted velocities to have a "
                      "relative CFL change up to 0.5."});
  params.get("CONT_PPM_VOLUME_BASED_CFL", adj.vol_CFL,
             {.default_value = adj.aggress_adjust,
              .desc = "If true, use the ratio of the open face lengths to the "
                      "tracer cell areas when estimating CFL numbers.  The "
                      "default is set by CONT_PPM_AGGRESS_ADJUST.",
              .do_not_read = adj.aggress_adjust});
  params.get("CONTINUITY_CFL_LIMIT", adj.CFL_limit_adjust,
             {.default_value = 0.5,
              .desc = "The maximum CFL of the adjusted velocities.",
              .units = "nondim"});
  params.get("CONT_PPM_BETTER_ITER", adj.better_iter,
             {.default_value = true,
              .desc = "If true, stop corrective iterations using a velocity "
                      "based criterion and only stop if the iteration is "
                      "better than all predecessors."});
  params.get("CONT_PPM_USE_VISC_REM_MAX", adj.use_visc_rem_max,
             {.default_value = true,
              .desc = "If true, use more appropriate limiting bounds for "
                      "corrections in strongly viscous columns."});
  params.get("CONT_PPM_MARGINAL_FACE_AREAS", adj.marginal_faces,
             {.default_value = true,
              .desc = "If true, use the marginal face areas from the continuity "
                      "solver for use as the weights in the barotropic solver. "
                      "Otherwise use the transport averaged areas."});

  // MOM6 sets a floor on the marginal thickness from this flag, but nothing
  // reads that floor, so the flag has no effect there either. It is read
  // anyway, so that the parameter docs match MOM6's.
  bool use_h_marg_min = false;
  params.get("CONT_USE_H_MARG_MIN", use_h_marg_min,
             {.default_value = false,
              .desc = "If true, the marginal thickness used and returned from continuity "
                      "is bounded from below by a sub-roundoff value. Otherwise the "
                      "minimum is 0."});

  return solver;
}

int Continuity::stencil() const {
  int stencil = 3;
  if (reconstruction.simple_2nd) stencil = 2;
  if (reconstruction.upwind_1st) stencil = 1;
  return stencil;
}

void Continuity::continuity(const amrex::MultiFab &u, const amrex::MultiFab &v,
                            const amrex::MultiFab &hin, amrex::MultiFab &h,
                            amrex::MultiFab &uh, amrex::MultiFab &vh, const amrex::Real dt,
                            const bool x_first) const {
  // Check every field the kernel reads. The grid's fields all have the same
  // halo and always have the right stagger, so mask2dT stands for them.
  check_field(u, Stagger::XFace, stencil(), "u");
  check_field(v, Stagger::YFace, stencil(), "v");
  check_field(hin, Stagger::Cell, stencil(), "hin");
  check_field(h, Stagger::Cell, stencil(), "h");
  check_field(uh, Stagger::XFace, stencil(), "uh");
  check_field(vh, Stagger::YFace, stencil(), "vh");
  check_field(G.mask2dT(), Stagger::Cell, stencil(), "the grid");
  check_field(pbv.por_face_areaU, Stagger::XFace, stencil(), "por_face_areaU");
  check_field(pbv.por_face_areaV, Stagger::YFace, stencil(), "por_face_areaV");

  // The loop doesn't use tiling, because the kernel extends its loops into the
  // halo itself.
  for (amrex::MFIter mfi(hin); mfi.isValid(); ++mfi) {
    // hin is an h field, so its boxes already use MOM6's numbering.
    // The box's cells, as in MOM6's computational domain. The kernel builds
    // the face loops from it, e.g. the u faces as growLo(compute_box, 0, 1).
    const amrex::Box compute_box = mfi.validbox();
    // The kernel's isd and ied are the first and last i of hin's storage,
    // ghost cells included, as in MOM6's data domain.
    const amrex::Box data_box = mfi.fabbox();

    MOM::continuity_PPM(mom_const_array(u, mfi), mom_const_array(v, mfi),
                        mom_const_array(hin, mfi), mom_array(h, mfi),
                        mom_array(uh, mfi), mom_array(vh, mfi), dt, compute_box, stencil(),
                        x_first, mom_const_array(G.mask2dT(), mfi),
                        mom_const_array(G.dy_Cu(), mfi), mom_const_array(G.IareaT(), mfi),
                        mom_const_array(G.IdxT(), mfi), mom_const_array(G.areaT(), mfi),
                        mom_const_array(G.dxT(), mfi), mom_const_array(G.mask2dCu(), mfi),
                        mom_const_array(G.dxCu(), mfi), mom_const_array(G.dx_Cv(), mfi),
                        mom_const_array(G.IdyT(), mfi), mom_const_array(G.dyT(), mfi),
                        mom_const_array(G.mask2dCv(), mfi), mom_const_array(G.dyCv(), mfi),
                        data_box.smallEnd(0), data_box.bigEnd(0), GV.Angstrom_H(),
                        GV.H_subroundoff(), reconstruction, transport_adjust,
                        /*obc=*/nullptr, mom_const_array(pbv.por_face_areaU, mfi),
                        mom_const_array(pbv.por_face_areaV, mfi),
                        // uhbt, vhbt, visc_rem_u, visc_rem_v: absent
                        {}, {}, {}, {},
                        // u_cor, v_cor: absent
                        {}, {},
                        // the BT_cont face areas and velocity corrections: absent
                        {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                        // h_u, h_v, du_cor, dv_cor: absent
                        {}, {}, {}, {});

    // The kernel returns its scratch to The_Arena() while its launches may
    // still be queued on this box's GPU stream. Wait for them, so that the
    // next box, on another stream, cannot reuse that memory too early. This
    // does nothing on CPUs. Backlog item TB-007 moves the scratch to
    // The_Async_Arena(), which makes the wait unnecessary.
    amrex::Gpu::streamSynchronize();
  }
}

} // namespace MOM
