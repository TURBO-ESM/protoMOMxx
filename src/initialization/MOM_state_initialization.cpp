#include <string>

#include "MOM_state_initialization.hpp"

#include "MOM_logger.hpp"

namespace MOM {

namespace {

// Set the layer thicknesses (in height units) to layers of uniform thickness
// evenly distributed between the surface and the maximum depth, squeezed
// where the bottom is shallower, with every layer at least an Angstrom thick.
// Writes the computational domain only.
void initialize_thickness_uniform(amrex::MultiFab &dz,
                                  const amrex::MultiFab &depth_tot,
                                  const VerticalGrid &vgrid) {

  const int nk = vgrid.nk();
  const amrex::Real max_depth = vgrid.max_depth();
  const amrex::Real Angstrom_Z = vgrid.Angstrom_Z();

  if (max_depth <= 0.0) {
    logger::fatal("initialize_thickness_uniform: MAXIMUM_DEPTH has a nonsensical value! Was it set?");
  }

  for (amrex::MFIter mfi(dz); mfi.isValid(); ++mfi) {
    // Slab (2D) box to ensure sequential iteration over k.
    const amrex::Box bx = amrex::makeSlab(mfi.validbox(), 2, 0);
    const amrex::Array4<amrex::Real> h = dz.array(mfi);
    const amrex::Array4<const amrex::Real> D = depth_tot.const_array(mfi);
    amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE(int i, int j, int) {
      // Walk the interfaces from the bottom up. Interface k is the top of
      // layer k, so eta_below holds MOM6's eta1D(K+1) and eta its eta1D(K).
      amrex::Real eta_below = -D(i, j, 0);
      for (int k = nk - 1; k >= 0; --k) {
        amrex::Real eta = -(max_depth * amrex::Real(k) / amrex::Real(nk));
        if (eta < (eta_below + Angstrom_Z)) {
          eta = eta_below + Angstrom_Z;
          h(i, j, k) = Angstrom_Z;
        } else {
          h(i, j, k) = eta - eta_below;
        }
        eta_below = eta;
      }
    });
  }
}

// Convert the layer thicknesses from height units to thickness units over the
// computational domain, leaving the halo values of h as they are.
void dz_to_thickness(const amrex::MultiFab &dz, amrex::MultiFab &h,
                     const VerticalGrid &vgrid) {

  if (vgrid.Boussinesq()) {
    // MOM6 sets h = GV%Z_to_H * dz. The factor is 1 in the Boussinesq mode at
    // the default H_TO_M and H_RESCALE_POWER, which are not read yet.
    amrex::MultiFab::Copy(h, dz, 0, 0, h.nComp(), 0);
  } else {
    // defer: the non-Boussinesq conversion, through the equation of state or
    //        the layer densities, with the thickness/units layer.
    logger::fatal("dz_to_thickness: the non-Boussinesq mode is not implemented.");
  }
}

// Set the velocities to zero, halos included.
void initialize_velocity_zero(amrex::MultiFab &u, amrex::MultiFab &v) {
  u.setVal(0.0);
  v.setVal(0.0);
}

} // namespace

State make_state(const Domain &domain, const Grid &grid,
                 const VerticalGrid &vgrid, RuntimeParams &params) {

  const int nk = vgrid.nk();
  State state;
  state.u = domain.make_u_field({.nk = nk});
  state.v = domain.make_v_field({.nk = nk});
  state.h = domain.make_h_field({.nk = nk});
  state.u.setVal(0.0);
  state.v.setVal(0.0);
  state.h.setVal(vgrid.Angstrom_H());

  params.doc_module("MOM_state_initialization", "");

  // MOM6 passes the setups depth_tot = G%bathyT + G%Z_ref. Z_ref is not
  // implemented yet, and at its default of zero depth_tot is the bottom depth.
  // defer: G%Z_ref (REFERENCE_HEIGHT, read by MOM6's MOM_grid_init).
  const amrex::MultiFab &depth_tot = grid.bathyT();

  // defer: the restart path, where MOM6 reads the parameters below without
  //        logging them and restores u, v and h from the restart file
  //        (FATAL_INCONSISTENT_RESTART_TIME, restore_state, ROTATE_INDEX).
  //        Until then, this sets up a new run.

  bool from_Z_file = false;
  params.get("INIT_LAYERS_FROM_Z_FILE", from_Z_file,
             {.default_value = false,
              .desc = "If true, initialize the layer thicknesses, temperatures, and "
                      "salinities from a Z-space file on a latitude-longitude grid."});
  if (from_Z_file) {
    // defer: MOM_temp_salt_initialize_from_Z.
    logger::fatal("make_state: INIT_LAYERS_FROM_Z_FILE is not implemented yet.");
  }

  // The thickness setups work in height units [Z ~> m]; dz_to_thickness
  // converts the result to thickness units below, as in MOM6.
  amrex::MultiFab dz = domain.make_h_field({.nk = nk});
  dz.setVal(0.0);

  std::string h_config = "uniform";
  params.get("THICKNESS_CONFIG", h_config,
             {.default_value = std::string("uniform"),
              .desc = "A string that determines how the initial layer thicknesses are "
                      "specified for a new run:\n"
                      "\t file - read interface heights from the file specified\n"
                      "\t\t by (THICKNESS_FILE).\n"
                      "\t thickness_file - read thicknesses from the file specified\n"
                      "\t\t by (THICKNESS_FILE).\n"
                      "\t mass_file - read thicknesses in units of mass per unit area from the file\n"
                      "\t\t specified by (THICKNESS_FILE).\n"
                      "\t coord - determined by ALE coordinate.\n"
                      "\t uniform - uniform thickness layers evenly distributed\n"
                      "\t\t between the surface and MAXIMUM_DEPTH.\n"
                      "\t list - read a list of positive interface depths.\n"
                      "\t param - use thicknesses from parameter THICKNESS_INIT_VALUES.\n"
                      "\t DOME - use a slope and channel configuration for the\n"
                      "\t\t DOME sill-overflow test case.\n"
                      "\t ISOMIP - use a configuration for the\n"
                      "\t\t ISOMIP test case.\n"
                      "\t benchmark - use the benchmark test case thicknesses.\n"
                      "\t Neverworld - use the Neverworld test case thicknesses.\n"
                      "\t search - search a density profile for the interface\n"
                      "\t\t densities. This is not yet implemented.\n"
                      "\t circle_obcs - the circle_obcs test case is used.\n"
                      "\t DOME2D - 2D version of DOME initialization.\n"
                      "\t adjustment2d - 2D lock exchange thickness ICs.\n"
                      "\t sloshing - sloshing gravity thickness ICs.\n"
                      "\t seamount - no motion test with seamount ICs.\n"
                      "\t dumbbell - sloshing channel ICs.\n"
                      "\t soliton - Equatorial Rossby soliton.\n"
                      "\t rossby_front - a mixed layer front in thermal wind balance.\n"
                      "\t USER - call a user modified routine."});

  if (h_config == "uniform") {
    initialize_thickness_uniform(dz, depth_tot, vgrid);
  } else if (h_config == "file" || h_config == "thickness_file" ||
      h_config == "mass_file" || h_config == "coord" || h_config == "list" ||
      h_config == "param" || h_config == "DOME" || h_config == "ISOMIP" ||
      h_config == "benchmark" || h_config == "Neverworld" ||
      h_config == "Neverland" || h_config == "search" ||
      h_config == "circle_obcs" || h_config == "lock_exchange" ||
      h_config == "external_gwave" || h_config == "DOME2D" ||
      h_config == "adjustment2d" || h_config == "sloshing" ||
      h_config == "seamount" || h_config == "dumbbell" ||
      h_config == "soliton" || h_config == "phillips" ||
      h_config == "rossby_front" || h_config == "USER") {
    // defer: all other THICKNESS_CONFIG options
    logger::fatal("make_state: THICKNESS_CONFIG \"", h_config,
                  "\" is not implemented yet.");
  } else {
    logger::fatal("make_state: Unrecognized layer thickness configuration \"",
                  h_config, "\".");
  }

  // defer: the temperature and salinity initialization (TS_CONFIG, into the
  //        tv structure), which MOM6 runs when ENABLE_THERMODYNAMICS is true.

  dz_to_thickness(dz, state.h, vgrid);

  bool depress_sfc = false;
  params.get("DEPRESS_INITIAL_SURFACE", depress_sfc,
             {.default_value = false,
              .desc = "If true,  depress the initial surface to avoid huge "
                      "tsunamis when a large surface pressure is applied."});
  bool trim_ic_for_p_surf = false;
  params.get("TRIM_IC_FOR_P_SURF", trim_ic_for_p_surf,
             {.default_value = false,
              .desc = "If true, cuts way the top of the column for initial conditions "
                      "at the depth where the hydrostatic pressure matches the imposed "
                      "surface pressure which is read from file."});
  if (depress_sfc || trim_ic_for_p_surf) {
    // defer: depress_surface and trim_for_ice.
    logger::fatal("make_state: DEPRESS_INITIAL_SURFACE and TRIM_IC_FOR_P_SURF "
                  "are not implemented yet.");
  }

  // defer: the initial ALE regridding (REGRID_ACCELERATE_INIT), which MOM6
  //        runs when USE_REGRIDDING is true.

  // The thicknesses in halo points might be needed to initialize the velocities.
  domain.pass_var(state.h);

  std::string u_config = "zero";
  params.get("VELOCITY_CONFIG", u_config,
             {.default_value = std::string("zero"),
              .desc = "A string that determines how the initial velocities "
                      "are specified for a new run:\n"
                      "\t file - read velocities from the file specified\n"
                      "\t\t by (VELOCITY_FILE).\n"
                      "\t zero - the fluid is initially at rest.\n"
                      "\t uniform - the flow is uniform (determined by\n"
                      "\t\t parameters INITIAL_U_CONST and INITIAL_V_CONST).\n"
                      "\t rossby_front - a mixed layer front in thermal wind balance.\n"
                      "\t soliton - Equatorial Rossby soliton.\n"
                      "\t USER - call a user modified routine."});

  if (u_config == "zero") {
    initialize_velocity_zero(state.u, state.v);
  } else if (u_config == "file" || u_config == "uniform" ||
      u_config == "circular" || u_config == "phillips" ||
      u_config == "rossby_front" || u_config == "soliton" ||
      u_config == "USER") {
    // defer: all other VELOCITY_CONFIG options
    logger::fatal("make_state: VELOCITY_CONFIG \"", u_config,
                  "\" is not implemented yet.");
  } else {
    logger::fatal("make_state: Unrecognized velocity configuration \"",
                  u_config, "\".");
  }

  // MOM6's pass_vector, which differs from two scalar exchanges only at a
  // tripolar fold.
  domain.pass_vars(state.u, state.v);

  // defer: the DEBUG checksums of u, v and h (MOM6's uvchksum and hchksum),
  //        with the checksum oracle.

  bool use_oda_incupd = false;
  params.get("ODA_INCUPD", use_oda_incupd,
             {.default_value = false,
              .desc = "If true, oda incremental updates will be applied "
                      "everywhere in the domain."});
  if (use_oda_incupd) {
    // defer: the oda_incupd setup (initialize_oda_incupd_fixed/_file).
    logger::fatal("make_state: ODA_INCUPD is not implemented yet.");
  }

  bool use_sponge = false;
  params.get("SPONGE", use_sponge,
             {.default_value = false,
              .desc = "If true, sponges may be applied anywhere in the domain. "
                      "The exact location and properties of those sponges are "
                      "specified via SPONGE_CONFIG."});
  if (use_sponge) {
    // defer: the sponge setup (SPONGE_CONFIG).
    logger::fatal("make_state: SPONGE is not implemented yet.");
  }

  return state;
}

} // namespace MOM
