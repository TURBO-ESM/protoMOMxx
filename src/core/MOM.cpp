#include "MOM.h"
#include "MOM_logger.h"

namespace MOM {

Model::Model(RuntimeParams &params) {

  logger::info("Initializing the MOM core...");

  params.doc_module("MOM", "Main MOM ocean model module"); // set current param module for documentation purposes

  int verbosity = 2;
  params.get("VERBOSITY", verbosity,
             {.default_value = 2,
              .desc = "Integer controlling level of messaging\n"
                      "\t0 = Only FATAL messages\n"
                      "\t2 = Only FATAL, WARNING, NOTE [default]\n"
                      "\t9 = All",
              .units = "",
              .fail_if_missing = false});

  logger::set_verbosity(verbosity);
  logger::info("Log verbosity: ", logger::get_verbosity());

  config_ = read_config_switches(params);

  // Initialization phases, in the order of MOM6's initialize_MOM:
  initialize_fixed(params);
  initialize_vertical(params);
  initialize_state(params);
  initialize_dynamics(params);

  logger::note("MOM core initialization complete.");
}

Model::Config Model::read_config_switches(RuntimeParams &params) {

  Config config;

  params.get("SPLIT", config.split, {.default_value = true, .desc = "Use the split time stepping if true."});

  params.get("SPLIT_RK4", config.split_rk4,
             {.default_value = false,
              .desc = "If true, use a version of the split explicit time stepping scheme that "
                      "exchanges velocities with step_MOM that have the average barotropic phase over "
                      "a baroclinic timestep rather than the instantaneous barotropic phase.",
              .do_not_log = !config.split});

  if (!config.split) {
    params.get("USE_RK2", config.use_RK2,
               {.default_value = false,
                .desc = "If true, use RK2 instead of RK3 in the unsplit time stepping."});
  }

  params.get("FPMIX", config.fpmix,
             {.default_value = false,
              .desc = "If true, use the FPMIX algorithm for tracer advection.",
              .do_not_log = true});

  if (config.fpmix && !config.split) {
    logger::fatal("FPMIX is only implemented for the split time stepping.");
  }

  params.get("DEBUG", config.debug,
             {.default_value = false,
              .desc = "If true, write out verbose debugging data.",
              .units = "nondim",
              .debugging_param = true});

  // todo: GLOBAL_INDEXING belongs to the domain setup.
  params.get("GLOBAL_INDEXING", config.global_indexing,
             {.default_value = false,
              .desc = "If true, use global indexing for all I/O and internal operations. "
                      "If false, use local indexing with halo regions.",
              .layout_param = true});

  return config;
}

void Model::initialize_fixed(RuntimeParams &params) {

  logger::note("initialize_fixed: (stub)");

  params.get("NIGLOBAL", ni_global_,
             {.desc = "The total number of thickness grid points in the x-direction in the physical domain.",
              .fail_if_missing = true});

  params.get("NJGLOBAL", nj_global_,
             {.desc = "The total number of thickness grid points in the y-direction in the physical domain.",
              .fail_if_missing = true});

  // todo: Domain class (config_src/infra): build the decomposition
  //       (BoxArray/DistributionMapping/Geometry) from NIGLOBAL/NJGLOBAL,
  //       REENTRANT_X/Y, halos, and layout params.
  //       Analogue of MOM6's MOM_domains_init.
  // todo: HorGrid: grid metrics (GRID_CONFIG), topography (TOPO_CONFIG),
  //       masks, and rotation. Analogue of MOM6's MOM_grid_init +
  //       MOM_initialize_fixed.
}

void Model::initialize_vertical(RuntimeParams &params) {

  logger::note("initialize_vertical: (stub)");

  params.get("NK", nk_,
             {.desc = "The total number of thickness grid points in the z-direction in the physical domain.",
              .fail_if_missing = true});

  // todo: VerticalGrid class: nk, reduced gravities (g_prime), target
  //       densities (Rlay) from COORD_CONFIG. Analogue of MOM6's
  //       verticalGridInit + MOM_initialize_coord.
}

void Model::initialize_state(RuntimeParams &params) {

  logger::note("initialize_state: (stub)");

  // todo: State container (u, v, h) with proper staggering, filled from
  //       THICKNESS_CONFIG / VELOCITY_CONFIG. Analogue of MOM6's
  //       MOM_initialize_state.
  (void)params;

  // tmp: the original psi (stream function) demo, kept operational until the
  // real state initialization. Until then, this demo exercises the AMReX machinery.

  // Cell size in each direction
  amrex::Real dx = 100000;
  amrex::Real dy = 100000;
  amrex::Real dz = 100000;

  // Mesh will be broken into chunks of up to max_chunk_size
  int max_chunk_size = 32;

  amrex::MultiFab psi;
  DefineCellCenteredMultiFab(ni_global_, nj_global_, nk_, max_chunk_size, psi);

  // AMReX object to hold domain meta data... Like the physical size of the domain and if it is periodic in each direction
  amrex::Geometry geom;
  InitializeGeometry(ni_global_, nj_global_, nk_, dx, dy, dz, geom);

  InitializeVariables(geom, psi);

  psi.FillBoundary(geom.periodicity());
}

void Model::initialize_dynamics(RuntimeParams &params) {

  logger::note("initialize_dynamics: (stub)");

  // todo: dynamics subsystem construction, dispatching on config_.split /
  //       config_.use_RK2 as in MOM6's four-way initialize_dyn_* branch.
  // defer: restart registration (register_restarts_dyn_*), diagnostics.
  (void)params;
}

void Model::InitializeVariables(const amrex::Geometry & geom,
                         amrex::MultiFab & psi)
{

    const amrex::Real x_min = geom.ProbLo(0);
    const amrex::Real x_max = geom.ProbHi(0);
    const amrex::Real y_min = geom.ProbLo(1);
    const amrex::Real y_max = geom.ProbHi(1);

    const amrex::Real dx = geom.CellSize(0);
    const amrex::Real dy = geom.CellSize(1);

    //////////////////////////////////////////////////////////////////////////
    // Initialization of stream function (psi)
    //////////////////////////////////////////////////////////////////////////

    // coefficient for initialization psi
    const amrex::Real a = 1000000;
    const double pi = 4. * std::atan(1.);

    for (amrex::MFIter mfi(psi); mfi.isValid(); ++mfi)
    {
        const amrex::Box& bx = mfi.validbox();

        const amrex::Array4<amrex::Real>& phi_array = psi.array(mfi);

        // [this] capture needed due to calling LinearMapCoordinates
        amrex::ParallelFor(bx, [=, this] AMREX_GPU_DEVICE(int i, int j, int k)
        {
            const amrex::Real x_cell_center = (i+0.5) * dx;
            const amrex::Real y_cell_center = (j+0.5) * dy;

            const amrex::Real x_transformed = LinearMapCoordinates(x_cell_center, x_min, x_max, 0.0, 2*pi);
            const amrex::Real y_transformed = LinearMapCoordinates(y_cell_center, y_min, y_max, 0.0, 2*pi);

            phi_array(i,j,k) = a*std::sin(x_transformed)*std::sin(y_transformed);
        });
    }
}

void Model::DefineCellCenteredMultiFab(const int ni_global, const int nj_global, const int nk,
                                       const int max_chunk_size,
                                       amrex::MultiFab & cell_centered_MultiFab)
{
    // lower and upper indices of domain
    const amrex::IntVect domain_low_index(AMREX_D_DECL(0,0,0));
    const amrex::IntVect domain_high_index(AMREX_D_DECL(ni_global-1, nj_global-1, nk-1)); // Need to determine number of z levels.

    // create box of indicies for cells
    const amrex::Box cell_centered_box(domain_low_index, domain_high_index);

    // initialize the boxarray "cell_box_array" from the single box "cell_centered_box"
    amrex::BoxArray cell_box_array(cell_centered_box);

    // break up boxarray "cell_box_array" into chunks no larger than "max_chunk_size" along a direction
    cell_box_array.maxSize(max_chunk_size);

    // assigns processor to each box in the box array
    amrex::DistributionMapping distribution_mapping(cell_box_array, 1);

    // number of components for each array
    int Ncomp = 1;

    // number of ghost cells for each array
    int Nghost = 1;

    cell_centered_MultiFab.define(cell_box_array, distribution_mapping, Ncomp, Nghost);
}

void Model::InitializeGeometry(const int ni_global, const int nj_global, const int nk,
                        const amrex::Real dx, const amrex::Real dy, const amrex::Real dz,
                        amrex::Geometry & geom)
{
  // lower and upper indices of domain
  const amrex::IntVect domain_low_index(0,0,0);
  const amrex::IntVect domain_high_index(ni_global-1, nj_global-1, nk-1);

  // create box of indicies for cells
  const amrex::Box cell_centered_box(domain_low_index, domain_high_index);

  // physical min and max boundaries of cells
  const amrex::RealBox real_box({0, 0, 0},
                                {ni_global*dx, nj_global*dy, nk*dz});

  // This, a value of 0, says we are using Cartesian coordinates
  int coordinate_system = 0;

  // This sets the boundary conditions in each direction to periodic
  amrex::Array<int,AMREX_SPACEDIM> is_periodic {1,1};

  // This defines a Geometry object
  geom.define(cell_centered_box, real_box, coordinate_system, is_periodic);
 // geom.define(cell_centered_box, real_box, amrex::CoordSys::cartesian, is_periodic); // Could use an amrex defined enum instead of an int to specify the coordinate system
}

AMREX_GPU_DEVICE AMREX_FORCE_INLINE
amrex::Real LinearMapCoordinates(const amrex::Real x,
                                 const amrex::Real x_min, const amrex::Real x_max,
                                 const amrex::Real xi_min, const amrex::Real xi_max)
{
    return x_min + ((xi_max-xi_min)/(x_max-x_min))*x;
}

} // namespace MOM
