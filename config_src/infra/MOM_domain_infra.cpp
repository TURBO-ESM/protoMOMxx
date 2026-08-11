#include "MOM_domain_infra.h"

namespace MOM {

Domain::Domain(const int ni_global, const int nj_global,
               const int ni_halo, const int nj_halo,
               const bool reentrant_x, const bool reentrant_y,
               const bool tripolar_n, const int n_boxes)
 : domain_(ni_global, nj_global, ni_halo, nj_halo,
           reentrant_x, reentrant_y, tripolar_n, n_boxes) {}

} // namespace MOM
