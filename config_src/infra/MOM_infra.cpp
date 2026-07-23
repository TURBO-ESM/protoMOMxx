#include "MOM_infra.h"

namespace MOM {

// A thin wrapper over TIM::Runtime owner mode: the member's construction
// does all the work (MPI_Init, then amrex::Initialize, etc.), and
// its destruction does the ordered teardown.
Infra::Infra(int &argc, char **&argv) : runtime_(argc, argv) {}

} // namespace MOM
