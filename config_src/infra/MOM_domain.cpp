#include <cstddef>
#include <stdexcept>

#include "MOM_domain.h"

namespace MOM {


Domain::Domain(const std::size_t ni_global, const std::size_t nj_global,
               const std::size_t ni_halo, const std::size_t nj_halo,
               const bool reentrant_i, const bool reentrant_j,
               const bool tripolar_N)
 : ni_global(ni_global), nj_global(nj_global),
   ni_halo(ni_halo), nj_halo(nj_halo),
   reentrant_i(reentrant_i), reentrant_j(reentrant_j),
   tripolar_N(tripolar_N)
 {

   if (reentrant_j && tripolar_N) {
     throw std::invalid_argument("Invalid domain configuration: reentrant_j and tripolar_N cannot both be true.");
   }

  }

} // namespace MOM
