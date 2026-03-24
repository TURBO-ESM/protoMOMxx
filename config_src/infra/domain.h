# pragma once

#include <cstddef>

namespace MOM {

class Domain {
public:
  Domain() = default;
  Domain(const std::size_t ni_global, const std::size_t nj_global,
         const std::size_t ni_halo, const std::size_t nj_halo,
         const bool reentrant_i, const bool reentrant_j,
         const bool tripolar_N);
  
  const std::size_t ni_global, nj_global;
  const std::size_t ni_halo,   nj_halo;
  const bool reentrant_i, reentrant_j;
  const bool tripolar_N;
    
};
    
}