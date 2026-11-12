#include "MOM_porous_barrier.hpp"

namespace MOM {

PorousBarrier make_porous_barrier(const Domain &domain, const int nk) {
  PorousBarrier pbv;
  pbv.por_face_areaU = domain.make_u_field({.nk = nk});
  pbv.por_face_areaV = domain.make_v_field({.nk = nk});
  pbv.por_face_areaU.setVal(1.0);
  pbv.por_face_areaV.setVal(1.0);
  return pbv;
}

} // namespace MOM
