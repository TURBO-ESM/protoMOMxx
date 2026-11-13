#include "MOM_dynamics_unsplit_RK2.hpp"

#include <AMReX_Gpu.H>
#include <AMReX_MFIter.H>

#include "MOM_logger.hpp"

namespace MOM {

namespace {

// The routines below stand in for the modules that the time step calls but
// that are not implemented yet. Each one sets its outputs to zero, or leaves
// its arguments unchanged when it only updates them, so for now the step adds
// no Coriolis, pressure, viscous or wind terms. Their arguments are the ones
// that exist so far; the modules will add theirs.

// todo: MOM6's CoriolisAdv_stencil (MOM_CoriolisAdv). It returns 2 for every
//       Coriolis scheme but the WENO ones.
int CoriolisAdv_stencil() { return 2; }

// todo: MOM6's CorAdCalc (MOM_CoriolisAdv), which sets CAu and CAv.
void CorAdCalc(const amrex::MultiFab & /*u*/, const amrex::MultiFab & /*v*/,
               const amrex::MultiFab & /*h*/, const amrex::MultiFab & /*uh*/,
               const amrex::MultiFab & /*vh*/, amrex::MultiFab &CAu, amrex::MultiFab &CAv,
               const PorousBarrier & /*pbv*/) {
  CAu.setVal(0.0);
  CAv.setVal(0.0);
}

// todo: MOM6's PressureForce (MOM_PressureForce), which sets PFu and PFv.
void PressureForce(const amrex::MultiFab & /*h*/, amrex::MultiFab &PFu, amrex::MultiFab &PFv) {
  PFu.setVal(0.0);
  PFv.setVal(0.0);
}

// todo: MOM6's horizontal_viscosity (MOM_hor_visc), which sets diffu and diffv.
void horizontal_viscosity(const amrex::MultiFab & /*u*/, const amrex::MultiFab & /*v*/,
                          const amrex::MultiFab & /*h*/, const amrex::MultiFab & /*uh*/,
                          const amrex::MultiFab & /*vh*/, amrex::MultiFab &diffu,
                          amrex::MultiFab &diffv, const amrex::Real /*dt*/) {
  diffu.setVal(0.0);
  diffv.setVal(0.0);
}

// todo: MOM6's set_viscous_ML (MOM_set_viscosity), which sets the viscosities
//       of the surface mixed layer.
void set_viscous_ML(const amrex::MultiFab & /*u*/, const amrex::MultiFab & /*v*/,
                    const amrex::MultiFab & /*h*/, const MechForcing & /*forces*/,
                    const amrex::Real /*dt*/) {}

// todo: MOM6's thickness_to_dz (MOM_interface_heights), which sets the
//       vertical extent of each layer.
void thickness_to_dz(const amrex::MultiFab & /*h*/, amrex::MultiFab &dz) { dz.setVal(0.0); }

// todo: MOM6's vertvisc_coef (MOM_vert_friction), which sets the coupling
//       coefficients between the layers.
void vertvisc_coef(const amrex::MultiFab & /*u*/, const amrex::MultiFab & /*v*/,
                   const amrex::MultiFab & /*h*/, const amrex::MultiFab & /*dz*/,
                   const MechForcing & /*forces*/, const amrex::Real /*dt*/) {}

// todo: MOM6's vertvisc (MOM_vert_friction), which applies the vertical
//       viscosity and the wind stress to u and v.
void vertvisc(amrex::MultiFab & /*u*/, amrex::MultiFab & /*v*/, const amrex::MultiFab & /*h*/,
              const MechForcing & /*forces*/, const amrex::Real /*dt*/) {}

/// @brief h_av = (h + hp)/2, on the valid points and halo points around them.
void average_thickness(const amrex::MultiFab &h, const amrex::MultiFab &hp,
                       amrex::MultiFab &h_av, const int halo) {
  for (amrex::MFIter mfi(h_av); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mom_box(amrex::grow(mfi.validbox(), amrex::IntVect(halo, halo, 0)));
    const amrex::Array4<const amrex::Real> h_in = mom_const_array(h, mfi);
    const amrex::Array4<const amrex::Real> h_p = mom_const_array(hp, mfi);
    const amrex::Array4<amrex::Real> av = mom_array(h_av, mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      av(i, j, k) = (h_in(i, j, k) + h_p(i, j, k)) * 0.5;
    });
  }
}

/// @brief out = mask * (u + dt * ((PF + CA) + diff)), on the valid points of a
/// u or v field. out may be u itself.
void add_accelerations(const amrex::MultiFab &mask, const amrex::MultiFab &u,
                       const amrex::MultiFab &PF, const amrex::MultiFab &CA,
                       const amrex::MultiFab &diff, const amrex::Real dt, amrex::MultiFab &out) {
  for (amrex::MFIter mfi(out); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mom_box(mfi.validbox());
    const amrex::Array4<const amrex::Real> m = mom_const_array(mask, mfi);
    const amrex::Array4<const amrex::Real> u_in = mom_const_array(u, mfi);
    const amrex::Array4<const amrex::Real> pf = mom_const_array(PF, mfi);
    const amrex::Array4<const amrex::Real> ca = mom_const_array(CA, mfi);
    const amrex::Array4<const amrex::Real> df = mom_const_array(diff, mfi);
    const amrex::Array4<amrex::Real> o = mom_array(out, mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      o(i, j, k) = m(i, j, 0) * (u_in(i, j, k) + dt * ((pf(i, j, k) + ca(i, j, k)) + df(i, j, k)));
    });
  }
}

/// @brief uhtr = uhtr + dt*uh, on the valid points of a u or v field and 2
/// points around them.
void accumulate_flux(const amrex::MultiFab &uh, const amrex::Real dt, amrex::MultiFab &uhtr) {
  for (amrex::MFIter mfi(uhtr); mfi.isValid(); ++mfi) {
    const amrex::Box bx = mom_box(amrex::grow(mfi.validbox(), amrex::IntVect(2, 2, 0)));
    const amrex::Array4<const amrex::Real> flux = mom_const_array(uh, mfi);
    const amrex::Array4<amrex::Real> sum = mom_array(uhtr, mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
      sum(i, j, k) = sum(i, j, k) + dt * flux(i, j, k);
    });
  }
}

} // namespace

DynUnsplitRK2 make_dyn_unsplit_RK2(const Domain &domain, const Grid &G, const VerticalGrid &GV,
                                   const PorousBarrier &pbv, RuntimeParams &params) {

  params.doc_module("MOM_dynamics_unsplit_RK2", "");

  amrex::Real be = 0.0;
  params.get("BE", be,
             {.default_value = 0.6,
              .desc = "If SPLIT is true, BE determines the relative weighting "
                      "of a 2nd-order Runga-Kutta baroclinic time stepping "
                      "scheme (0.5) and a backward Euler scheme (1) that is "
                      "used for the Coriolis and inertial terms.  BE may be "
                      "from 0.5 to 1, but instability may occur near 0.5. "
                      "BE is also applicable if SPLIT is false and USE_RK2 "
                      "is true.",
              .units = "nondim"});
  amrex::Real begw = 0.0;
  params.get("BEGW", begw,
             {.default_value = 0.0,
              .desc = "If SPLIT is true, BEGW is a number from 0 to 1 that "
                      "controls the extent to which the treatment of gravity "
                      "waves is forward-backward (0) or simulated backward "
                      "Euler (1).  0 is almost always used. "
                      "If SPLIT is false and USE_RK2 is true, BEGW can be "
                      "between 0 and 0.5 to damp gravity waves.",
              .units = "nondim"});

  bool dt_visc_bug = false;
  params.get("UNSPLIT_DT_VISC_BUG", dt_visc_bug,
             {.default_value = false,
              .desc = "If false, use the correct timestep in the viscous terms applied in the first "
                      "predictor step with the unsplit time stepping scheme, and in the calculation "
                      "of the turbulent mixed layer properties for viscosity with unsplit or "
                      "unsplit_RK2.  If true, an older incorrect value is used."});

  bool debug = false;
  params.get("DEBUG", debug,
             {.default_value = false,
              .desc = "If true, write out verbose debugging data.",
              .debugging_param = true});
  if (debug) {
    // defer: the DEBUG checksums of the time step.
    logger::fatal("make_dyn_unsplit_RK2: The debugging checksums (DEBUG = True) "
                  "are not implemented yet.");
  }
  bool use_tides = false;
  params.get("TIDES", use_tides,
             {.default_value = false,
              .desc = "If true, apply tidal momentum forcing."});
  bool calculate_SAL = false;
  params.get("CALCULATE_SAL", calculate_SAL,
             {.default_value = use_tides,
              .desc = "If true, calculate self-attraction and loading."});

  DynUnsplitRK2 dyn{.domain = domain,
                    .G = G,
                    .GV = GV,
                    .pbv = pbv,
                    .be = be,
                    .begw = begw,
                    .dt_visc_bug = dt_visc_bug,
                    .continuity = make_continuity(G, GV, pbv, params)};
  // todo: CoriolisAdv_init
  if (calculate_SAL) {
    // defer: SAL_init
    logger::fatal("make_dyn_unsplit_RK2: Self-attraction and loading "
                  "(CALCULATE_SAL = True) is not implemented yet.");
  }
  if (use_tides) {
    // defer: tidal_forcing_init
    logger::fatal("make_dyn_unsplit_RK2: Tidal forcing (TIDES = True) "
                  "is not implemented yet.");
  }
  // todo: PressureForce_init, hor_visc_init and vertvisc_init

  return dyn;
}

void DynUnsplitRK2::step(State &state, const amrex::Real dt, const MechForcing &forces,
                         const bool x_first) const {

  // defer: the DEBUG checksums between the stages (MOM_state_chksum,
  //        MOM_accel_chksum). make_dyn_unsplit_RK2 rejects DEBUG = True.
  // defer: the open boundary updates, the surface pressure (p_surf) and the
  //        diagnostics, which are not implemented in protoMOMxx.

  // The names MOM6 uses for the state.
  amrex::MultiFab &u = state.u;
  amrex::MultiFab &v = state.v;
  amrex::MultiFab &h = state.h;
  amrex::MultiFab &uh = state.uh;
  amrex::MultiFab &vh = state.vh;

  const amrex::Real dt_pred = dt * be;  // The time step of the predictor [T ~> s].
  const int cor_stencil = CoriolisAdv_stencil();
  const int nk = GV.nk();

  // The accelerations [L T-2 ~> m s-2]. MOM6 sets them to zero once and keeps
  // them between steps in its control structure, for its diagnostics. Here
  // they are set to zero at the start of every step instead. The answers are
  // the same as long as each step writes every point of them that it reads.
  // todo: check this as each stub is replaced by its module.
  amrex::MultiFab CAu = domain.make_u_field({.nk = nk});    // Coriolis and advection.
  amrex::MultiFab CAv = domain.make_v_field({.nk = nk});
  amrex::MultiFab PFu = domain.make_u_field({.nk = nk});    // Pressure force.
  amrex::MultiFab PFv = domain.make_v_field({.nk = nk});
  amrex::MultiFab diffu = domain.make_u_field({.nk = nk});  // Horizontal viscosity.
  amrex::MultiFab diffv = domain.make_v_field({.nk = nk});
  CAu.setVal(0.0);
  CAv.setVal(0.0);
  PFu.setVal(0.0);
  PFv.setVal(0.0);
  diffu.setVal(0.0);
  diffv.setVal(0.0);

  amrex::MultiFab h_av = domain.make_h_field({.nk = nk});  // Averaged thicknesses [H ~> m or kg m-2].
  amrex::MultiFab hp = domain.make_h_field({.nk = nk});    // Predicted thicknesses [H ~> m or kg m-2].
  amrex::MultiFab dz = domain.make_h_field({.nk = nk});    // Vertical extent of the layers [Z ~> m].
  amrex::MultiFab up = domain.make_u_field({.nk = nk});    // Predicted zonal velocity [L T-1 ~> m s-1].
  amrex::MultiFab vp = domain.make_v_field({.nk = nk});    // Predicted meridional velocity [L T-1 ~> m s-1].
  h_av.setVal(0.0);
  hp.setVal(0.0);
  up.setVal(0.0);
  vp.setVal(0.0);

  // diffu = horizontal viscosity terms (u, h)
  horizontal_viscosity(u, v, h, uh, vh, diffu, diffv, dt);
  domain.pass_vector(diffu, diffv);

  // This continuity step is only for the Coriolis terms: the denominator of
  // the PV and the mass transport.
  // uh = u[n-1]*h[n-1/2]
  // hp = h[n-1/2] + dt/2 div . uh
  continuity.continuity(u, v, h, hp, uh, vh, dt_pred, x_first);
  // MOM6 exchanges only cor_stencil halo points of hp, uh and vh, and of u
  // and v below. These exchanges fill the whole halo.
  domain.pass_var(hp);
  domain.pass_vector(uh, vh);
  if (cor_stencil > 2) domain.pass_vector(u, v);

  // h_av = (h + hp)/2  (used in the PV denominator)
  average_thickness(h, hp, h_av, cor_stencil);

  // CAu = -(f+zeta)/h_av vh + d/dx KE  (function of u[n-1] and uh[n-1])
  CorAdCalc(u, v, h_av, uh, vh, CAu, CAv, pbv);

  // PFu = d/dx M(h_av, T, S)  (function of h[n-1/2])
  PressureForce(h, PFu, PFv);
  domain.pass_vector(PFu, PFv);
  domain.pass_vector(CAu, CAv);

  // up+[n-1/2] = u[n-1] + dt_pred * (PFu + CAu)
  add_accelerations(G.mask2dCu(), u, PFu, CAu, diffu, dt_pred, up);
  add_accelerations(G.mask2dCv(), v, PFv, CAv, diffv, dt_pred, vp);

  // up[n-1/2] <- up*[n-1/2] + dt/2 d/dz visc d/dz up[n-1/2]
  const amrex::Real dt_visc = dt_visc_bug ? dt_pred : dt;
  set_viscous_ML(u, v, h_av, forces, dt_visc);
  thickness_to_dz(h_av, dz);
  vertvisc_coef(up, vp, h_av, dz, forces, dt_pred);
  vertvisc(up, vp, h_av, forces, dt_pred);
  domain.pass_vector(up, vp);

  // uh = up[n-1/2] * h[n-1/2]
  // h_av = h + dt div . uh
  continuity.continuity(up, vp, h, hp, uh, vh, dt, x_first);
  domain.pass_var(hp);
  domain.pass_vector(uh, vh);

  // h_av <- (h + hp)/2  (centered at n-1/2)
  average_thickness(h, hp, h_av, cor_stencil);

  // CAu = -(f+zeta(up))/h_av vh + d/dx KE(up)  (function of up[n-1/2], h[n-1/2])
  CorAdCalc(up, vp, h_av, uh, vh, CAu, CAv, pbv);

  // up* = u[n] + (1+gamma) * dt * (PFu + CAu)  Extrapolated for damping
  // u*[n+1] = u[n] + dt * (PFu + CAu)
  // up must be set first, from u[n].
  const amrex::Real dt_begw = dt * (1.0 + begw);
  add_accelerations(G.mask2dCu(), u, PFu, CAu, diffu, dt_begw, up);
  add_accelerations(G.mask2dCu(), u, PFu, CAu, diffu, dt, u);
  add_accelerations(G.mask2dCv(), v, PFv, CAv, diffv, dt_begw, vp);
  add_accelerations(G.mask2dCv(), v, PFv, CAv, diffv, dt, v);

  // up[n] <- up* + dt d/dz visc d/dz up
  // u[n] <- u*[n] + dt d/dz visc d/dz u[n]
  thickness_to_dz(h_av, dz);
  vertvisc_coef(up, vp, h_av, dz, forces, dt);
  vertvisc(up, vp, h_av, forces, dt);
  vertvisc_coef(u, v, h_av, dz, forces, dt);
  vertvisc(u, v, h_av, forces, dt);
  domain.pass_vector(up, vp);
  domain.pass_vector(u, v);

  // uh = up[n] * h[n]  (up[n] might be extrapolated to damp gravity waves)
  // h[n+1] = h[n] + dt div . uh
  continuity.continuity(up, vp, h, h, uh, vh, dt, x_first);
  domain.pass_var(h);
  domain.pass_vector(uh, vh);

  // Sum the volume fluxes for the tracer advection.
  // defer: the reset of uhtr and vhtr after the tracer advection.
  accumulate_flux(uh, dt, state.uhtr);
  accumulate_flux(vh, dt, state.vhtr);

  // defer: eta_av, the time-mean sea surface height, which only the
  //        diagnostics and the surface state read.
}

} // namespace MOM
