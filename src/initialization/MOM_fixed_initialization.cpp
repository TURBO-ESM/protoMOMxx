#include <utility>

#include "MOM_fixed_initialization.h"
#include "MOM_grid_initialize.h"
#include "MOM_shared_initialization.h"

namespace MOM {

Grid make_grid(const Domain &domain, RuntimeParams &params) {

  params.doc_module("MOM_grid_init", "");

  GridFields fields = set_grid_metrics(domain, params);

  // todo: topography (TOPO_CONFIG, MINIMUM_DEPTH, MAXIMUM_DEPTH) and the
  //       land/sea masks are read and set here, between the metrics and the
  //       rotation, matching MOM6's MOM_initialize_fixed order. The analytic
  //       topographies are shaped from the geographic extents, which the
  //       topography setup reads for itself through read_grid_extents.

  fields.CoriolisBu = MOM_initialize_rotation(domain, fields.geoLatBu, params);

  // defer: the reciprocals (IdxT, IdyCu, IareaT, ...) of MOM6's
  //        set_derived_dyn_horgrid, until their first user (the dynamics
  //        kernels), and the u/v-cell areas (areaCu/areaCv), which are
  //        mask-dependent (areaCu = dxCu * dy_Cu with dy_Cu = mask2dCu * dyCu
  //        in MOM6's initialize_masks), until the masks are introduced.

  return Grid(std::move(fields));
}

} // namespace MOM
