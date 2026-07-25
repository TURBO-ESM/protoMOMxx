#include "MOM_domains.h"

namespace MOM {

Domain make_domain(RuntimeParams &params) {

  params.doc_module("MOM_domains", "");

  bool reentrant_x = true;
  bool reentrant_y = false;
  bool tripolar_n = false;
  int ni_global = 0;
  int nj_global = 0;
  int ni_halo = 4;
  int nj_halo = 4;

  params.get("REENTRANT_X", reentrant_x,
             {.default_value = true,
              .desc = "If true, the domain is zonally reentrant."});

  params.get("REENTRANT_Y", reentrant_y,
             {.default_value = false,
              .desc = "If true, the domain is meridionally reentrant."});

  params.get("TRIPOLAR_N", tripolar_n,
             {.default_value = false,
              .desc = "Use tripolar connectivity at the northern edge of the domain. "
                      "With TRIPOLAR_N, NIGLOBAL must be even."});

  params.get("NIGLOBAL", ni_global,
             {.desc = "The total number of thickness grid points in "
                      "the x-direction in the physical domain.",
              .fail_if_missing = true});

  params.get("NJGLOBAL", nj_global,
             {.desc = "The total number of thickness grid points in "
                      "the y-direction in the physical domain.",
              .fail_if_missing = true});

  params.get("NIHALO", ni_halo,
             {.default_value = 4,
              .desc = "The number of halo points on each side in the x-direction."});

  params.get("NJHALO", nj_halo,
             {.default_value = 4,
              .desc = "The number of halo points on each side in the y-direction."});

  // defer: MASKTABLE, AUTO_MASKTABLE, LAYOUT, IO_LAYOUT (MOM6's processor
  //        layout machinery; the domain's one-box-per-rank decomposition and
  //        AMReX's DistributionMapping take over that role).

  return Domain(ni_global, nj_global, ni_halo, nj_halo,
                reentrant_x, reentrant_y, tripolar_n);
}

} // namespace MOM
