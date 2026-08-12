#include "MOM_domain_infra.h"

namespace MOM {

Domain::Domain(const DomainSpec &spec)
 : domain_(TIM::DomainSpec{.ni_global = spec.ni_global,
                           .nj_global = spec.nj_global,
                           .ni_halo = spec.ni_halo,
                           .nj_halo = spec.nj_halo,
                           .periodic_x = spec.reentrant_x,
                           .periodic_y = spec.reentrant_y,
                           .tripolar_n = spec.tripolar_n,
                           .n_boxes = spec.n_boxes}) {}

} // namespace MOM
