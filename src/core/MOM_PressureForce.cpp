#include "MOM_PressureForce.hpp"

#include "MOM_logger.hpp"

namespace MOM {

PressureForce make_pressure_force(const Grid &G, const VerticalGrid &GV, RuntimeParams &params) {

  params.doc_module("MOM_PressureForce", "");

  bool analytic_FV_PGF = true;
  params.get("ANALYTIC_FV_PGF", analytic_FV_PGF,
             {.default_value = true,
              .desc = "If true the pressure gradient forces are calculated "
                      "with a finite volume form that analytically integrates "
                      "the equations of state in pressure to avoid any "
                      "possibility of numerical thermobaric instability, as "
                      "described in Adcroft et al., O. Mod. (2008)."});
  if (!analytic_FV_PGF) {
    // defer: PressureForce_Mont_init and the Montgomery potential form.
    logger::fatal("make_pressure_force: The Montgomery potential form of the pressure "
                  "gradient (ANALYTIC_FV_PGF = False) is not implemented yet.");
  }

  return PressureForce{.GV = GV, .PressureForce_FV = make_pressure_force_FV(G, GV, params)};
}

void PressureForce::pressure_force(const amrex::MultiFab &h, amrex::MultiFab &PFu,
                                   amrex::MultiFab &PFv) const {
  if (!GV.Boussinesq()) {
    // defer: PressureForce_FV_nonBouss
    logger::fatal("pressure_force: The non-Boussinesq pressure gradient is not "
                  "implemented yet.");
  }
  PressureForce_FV.PressureForce_FV_Bouss(h, PFu, PFv);
}

} // namespace MOM
