#include "MOM_forcing_type.hpp"

namespace MOM {

MechForcing make_mech_forcing(const Domain &domain) {
  MechForcing forces;
  forces.taux = domain.make_u_field({.nk = 1});
  forces.tauy = domain.make_v_field({.nk = 1});
  forces.taux.setVal(0.0);
  forces.tauy.setVal(0.0);
  return forces;
}

} // namespace MOM
