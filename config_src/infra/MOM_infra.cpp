#include "MOM_infra.h"

#include <AMReX.H>

namespace MOM {

Infra::Infra(int &argc, char **&argv) {
    amrex::Initialize(argc, argv);
}

Infra::~Infra() {
    amrex::Finalize();
}

} // namespace MOM
