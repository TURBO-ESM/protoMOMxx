#include <stdexcept>

#include "MOM_domain_infra.h"

namespace MOM {

Domain::Domain(const int ni_global, const int nj_global,
               const int ni_halo, const int nj_halo,
               const bool reentrant_x, const bool reentrant_y,
               const bool tripolar_n, const int n_boxes)
 : domain_(make_tim_domain(ni_global, nj_global, ni_halo, nj_halo,
                           reentrant_x, reentrant_y, tripolar_n, n_boxes)) {}

TIM::Domain Domain::make_tim_domain(const int ni_global, const int nj_global,
                                    const int ni_halo, const int nj_halo,
                                    const bool reentrant_x, const bool reentrant_y,
                                    const bool tripolar_n, const int n_boxes) {

  if (ni_global <= 0 || nj_global <= 0) {
    throw std::invalid_argument("Invalid domain configuration: ni_global and nj_global must be positive.");
  }

  if (ni_halo < 0 || nj_halo < 0) {
    throw std::invalid_argument("Invalid domain configuration: ni_halo and nj_halo must be non-negative.");
  }

  if (reentrant_y && tripolar_n) {
    throw std::invalid_argument("Invalid domain configuration: reentrant_y and tripolar_n cannot both be true.");
  }

  // defer: tripolar connectivity.
  if (tripolar_n) {
    throw std::invalid_argument("Invalid domain configuration: tripolar_n is not implemented yet.");
  }

  if (n_boxes < 0) {
    throw std::invalid_argument("Invalid domain configuration: n_boxes must be positive, or 0 for one box per rank.");
  }

  return TIM::Domain(ni_global, nj_global, ni_halo, nj_halo,
                     reentrant_x, reentrant_y, tripolar_n, n_boxes);
}

} // namespace MOM
