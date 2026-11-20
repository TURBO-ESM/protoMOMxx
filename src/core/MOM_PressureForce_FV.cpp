#include "MOM_PressureForce_FV.hpp"

#include "MOM_domains.hpp"
#include "MOM_logger.hpp"

namespace MOM {

PressureForceFV make_pressure_force_FV(const Grid &G, const VerticalGrid &GV,
                                       RuntimeParams &params) {

  params.doc_module("MOM_PressureForce_FV", "");

  PressureForceFV PF{.G = G, .GV = GV};

  // MOM6 also reads DEBUG here, which it uses only with an equation of state.
  params.get("RHO_PGF_REF", PF.rho_ref,
             {.default_value = GV.Rho0(),
              .desc = "The reference density that is subtracted off when calculating pressure "
                      "gradient forces.  Its inverse is subtracted off of specific volumes when "
                      "in non-Boussinesq mode.  The default is RHO_0.",
              .units = "kg m-3"});
  bool enable_bugs = true;
  params.get("ENABLE_BUGS_BY_DEFAULT", enable_bugs,
             {.default_value = true, .do_not_log = true});  // MOM6 logs it from MOM.F90.
  params.get("RHO_PGF_REF_BUG", PF.rho_ref_bug,
             {.default_value = enable_bugs,
              .desc = "If true, recover a bug that RHO_0 (the mean seawater density in Boussinesq mode) "
                      "and RHO_PGF_REF (the subtracted reference density in finite volume pressure "
                      "gradient forces) are incorrectly interchanged in several instances in Boussinesq mode."});

  bool tides = false;
  params.get("TIDES", tides,
             {.default_value = false,
              .desc = "If true, apply tidal momentum forcing."});
  if (tides) {
    // defer: the tidal forcing, with TIDES_ANSWER_DATE and BOUSSINESQ_SAL_TIDES.
    logger::fatal("make_pressure_force_FV: Tidal forcing (TIDES = True) "
                  "is not implemented yet.");
  }
  bool calculate_SAL = false;
  params.get("CALCULATE_SAL", calculate_SAL,
             {.default_value = tides,
              .desc = "If true, calculate self-attraction and loading."});
  if (calculate_SAL) {
    // defer: the self-attraction and loading, with SAL_USE_BPA and
    //        BOUSSINESQ_SAL_TIDES.
    logger::fatal("make_pressure_force_FV: Self-attraction and loading "
                  "(CALCULATE_SAL = True) is not implemented yet.");
  }

  bool use_temperature = true;
  params.get("ENABLE_THERMODYNAMICS", use_temperature,
             {.default_value = true,
              .desc = "If true, Temperature and salinity are used as state variables.",
              .do_not_log = true});
  bool use_EOS = use_temperature;
  params.get("USE_EOS", use_EOS,
             {.default_value = use_temperature,
              .desc = "If true,  density is calculated from temperature and "
                      "salinity with an equation of state.  If USE_EOS is "
                      "true, ENABLE_THERMODYNAMICS must be true as well.",
              .do_not_log = true});
  if (use_EOS) {
    // defer: the pressure integrals with an equation of state.
    logger::fatal("make_pressure_force_FV: The pressure gradient with an equation of state "
                  "(USE_EOS = True) is not implemented yet.");
  }

  // Without an equation of state, the options from here up to USE_STANLEY_PGF
  // have no effect. They are read so that the parameter doc matches MOM6's.
  bool use_SSH_in_Z0p = false;
  params.get("SSH_IN_EOS_PRESSURE_FOR_PGF", use_SSH_in_Z0p,
             {.default_value = false,
              .desc = "If true, include contributions from the sea surface height in the height-based "
                      "pressure used in the equation of state calculations for the Boussinesq pressure "
                      "gradient forces, including adjustments for atmospheric or sea-ice pressure.",
              .do_not_log = !GV.Boussinesq()});

  bool use_ALE = false;
  params.get("USE_REGRIDDING", use_ALE,
             {.default_value = false,
              .desc = "If True, use the ALE algorithm (regridding/remapping). "
                      "If False, use the layered isopycnal algorithm.",
              .do_not_log = true});  // MOM6 logs it from MOM.F90.
  bool useMassWghtInterp = false;
  params.get("MASS_WEIGHT_IN_PRESSURE_GRADIENT", useMassWghtInterp,
             {.default_value = false,
              .desc = "If true, use mass weighting when interpolating T/S for integrals "
                      "near the bathymetry in FV pressure gradient calculations."});
  bool MassWghtInterpTop = useMassWghtInterp;
  params.get("MASS_WEIGHT_IN_PRESSURE_GRADIENT_TOP", MassWghtInterpTop,
             {.default_value = useMassWghtInterp,
              .desc = "If true and MASS_WEIGHT_IN_PRESSURE_GRADIENT is true, use mass weighting when "
                      "interpolating T/S for integrals near the top of the water column in FV "
                      "pressure gradient calculations. "});
  bool MassWghtInterp_NonBous_bug = false;
  params.get("MASS_WEIGHT_IN_PGF_NONBOUS_BUG", MassWghtInterp_NonBous_bug,
             {.default_value = false,
              .desc = "If true, use a masking bug in non-Boussinesq calculations with mass weighting "
                      "when interpolating T/S for integrals near the bathymetry in FV pressure "
                      "gradient calculations.",
              .do_not_log = GV.Boussinesq() || !useMassWghtInterp});
  bool MassWghtInterpVanOnly = false;
  params.get("MASS_WEIGHT_IN_PGF_VANISHED_ONLY", MassWghtInterpVanOnly,
             {.default_value = false,
              .desc = "If true, use mass weighting when interpolating T/S for integrals "
                      "only if one side is vanished according to RESET_INTXPA_H_NONVANISHED. "});

  bool correction_intxpa = false;
  params.get("CORRECTION_INTXPA", correction_intxpa,
             {.default_value = false,
              .desc = "If true, use a correction for surface pressure curvature in intx_pa.",
              .do_not_log = !use_EOS});
  bool reset_intxpa_integral = false;
  params.get("RESET_INTXPA_INTEGRAL", reset_intxpa_integral,
             {.default_value = false,
              .desc = "If true, reset INTXPA to match pressures at first nonvanished cell. "
                      "Includes pressure correction.",
              .do_not_log = !use_EOS});
  bool reset_intxpa_flattest = false;
  params.get("RESET_INTXPA_INTEGRAL_FLATTEST", reset_intxpa_flattest,
             {.default_value = false,
              .desc = "If true, use flattest interface as reference interface where there is no "
                      "better choice for RESET_INTXPA_INTEGRAL. Otherwise, use surface interface.",
              .do_not_log = !use_EOS});
  // MOM6 turns these three off without an equation of state.
  correction_intxpa = false;
  reset_intxpa_integral = false;
  reset_intxpa_flattest = false;
  amrex::Real h_nonvanished = 0.0;
  params.get("RESET_INTXPA_H_NONVANISHED", h_nonvanished,
             {.default_value = 1.0e-6,
              .desc = "A minimal layer thickness that indicates that a layer is thick enough to usefully "
                      "reestimate the pressure integral across the interface below.",
              .units = "m",
              .do_not_log = !reset_intxpa_integral});
  bool use_inaccurate_pgf_rho_anom = false;
  params.get("USE_INACCURATE_PGF_RHO_ANOM", use_inaccurate_pgf_rho_anom,
             {.default_value = false,
              .desc = "If true, use a form of the PGF that uses the reference density "
                      "in an inaccurate way. This is not recommended."});
  bool reconstruct = use_ALE;
  params.get("RECONSTRUCT_FOR_PRESSURE", reconstruct,
             {.default_value = use_ALE,
              .desc = "If True, use vertical reconstruction of T & S within "
                      "the integrals of the FV pressure gradient calculation. "
                      "If False, use the constant-by-layer algorithm. "
                      "The default is set by USE_REGRIDDING."});
  int recon_scheme = 1;
  params.get("PRESSURE_RECONSTRUCTION_SCHEME", recon_scheme,
             {.default_value = 1,
              .desc = "Order of vertical reconstruction of T/S to use in the "
                      "integrals within the FV pressure gradient calculation.\n"
                      " 0: PCM or no reconstruction.\n"
                      " 1: PLM reconstruction.\n"
                      " 2: PPM reconstruction.\n"
                      " 3: PLM with least squares slope."});
  bool boundary_extrap = true;
  params.get("BOUNDARY_EXTRAPOLATION_PRESSURE", boundary_extrap,
             {.default_value = true,
              .desc = "If true, the reconstruction of T & S for pressure in "
                      "boundary cells is extrapolated, rather than using PCM "
                      "in these cells. If true, the same order polynomial is "
                      "used as is used for the interior cells."});
  bool use_stanley_pgf = false;
  params.get("USE_STANLEY_PGF", use_stanley_pgf,
             {.default_value = false,
              .desc = "If true, turn on Stanley SGS T variance parameterization "
                      "in PGF code."});
  if (use_stanley_pgf) {
    // defer: the Stanley parameterization, with STANLEY_COEFF.
    logger::fatal("make_pressure_force_FV: The Stanley parameterization "
                  "(USE_STANLEY_PGF = True) is not implemented yet.");
  }

  PF.GFS_scale = 1.0;
  if (GV.g_prime()[0] != GV.g_Earth()) PF.GFS_scale = GV.g_prime()[0] / GV.g_Earth();
  // defer: MOM6 writes GFS / G_EARTH to the log here (log_param).

  return PF;
}

void PressureForceFV::PressureForce_FV_Bouss(const amrex::MultiFab &h, amrex::MultiFab &PFu,
                                             amrex::MultiFab &PFv) const {
  // MOM6 finds the interface heights one point into the halo, so h needs 1
  // ghost cell.
  check_field(h, Stagger::Cell, 1, "PressureForce_FV_Bouss", "h");
  check_field(PFu, Stagger::XFace, 0, "PressureForce_FV_Bouss", "PFu");
  check_field(PFv, Stagger::YFace, 0, "PressureForce_FV_Bouss", "PFv");

  // todo: call the kernel of MOM6's PressureForce_FV_Bouss once it is ported,
  //       and drop the zeroing below. The call would look like this:
  //
  // for (amrex::MFIter mfi(h); mfi.isValid(); ++mfi) {
  //   MOM::pressureforce_fv_bouss_tr(mfi.validbox(), mom_const_array(h, mfi),
  //                                  /* the grid fields, GV and the options */,
  //                                  mom_array(PFu, mfi), mom_array(PFv, mfi));
  // }
  PFu.setVal(0.0);
  PFv.setVal(0.0);
}

} // namespace MOM
