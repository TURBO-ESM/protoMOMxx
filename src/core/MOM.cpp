#include "MOM.h"
#include "MOM_logger.h"
#include "MOM_directories.h"

namespace MOM {

Model::Model(const int ensemble_num)
  : ensemble_num_(ensemble_num) {

  logger::info("Initializing protoMOMxx...");
  if (ensemble_num_ >= 0) logger::info("Ensemble number: ", ensemble_num_);

  // Read input.nml to determine the directories and files to read from and write to
  Directories directories(ensemble_num_);

  // RuntimeParams reads the parameter files specified in input.nml and makes them available for querying.
  params = std::make_shared<RuntimeParams>(
    directories.parameter_filenames(),
    "MOM_parameters_doc"
  );
  params->doc_module("MOM", "Main MOM ocean model module"); // set current param module for documentation purposes

  int verbosity = 2;
  params->get("VERBOSITY", verbosity, 
              {.default_value = 2,
               .desc = "Integer controlling level of messaging\n"
                       "\t0 = Only FATAL messages\n"
                       "\t2 = Only FATAL, WARNING, NOTE [default]\n"
                       "\t9 = All",
               .units = "",
               .fail_if_missing = false});

  logger::set_verbosity(verbosity);
  logger::info("Log verbosity: ", logger::get_verbosity());

  // todo: Below parameter queries are currently just examples to demonstrate the get() interface and should 
  // be moved to more appropriate locations as the corresponding functionality is implemented.

  bool split = false;
  params->get("SPLIT", split, {.default_value = true, .desc = "Use the split time stepping if true."});

  bool split_rk4 = false;
  params->get("SPLIT_RK4", split_rk4,
              {.default_value = false,
               .desc = "If true, use a version of the split explicit time stepping scheme that "
                       "exchanges velocities with step_MOM that have the average barotropic phase over "
                       "a baroclinic timestep rather than the instantaneous barotropic phase.",
               .do_not_log = !split});

  bool use_RK2 = false;
  if (!split) {
    params->get("USE_RK2", use_RK2,
                {.default_value = false, 
                 .desc = "If true, use RK2 instead of RK3 in the unsplit time stepping."});
  }

  bool fpmix = false;
  params->get("FPMIX", fpmix, 
              {.default_value = false,
               .desc = "If true, use the FPMIX algorithm for tracer advection.",
               .do_not_log = true});
  
  if (fpmix && !split) {
    logger::fatal("FPMIX is only implemented for the split time stepping.");
  }

  int N_SMOOTH = 0;
  params->open_block("KPP", "KPP module parameters");
  params->get("N_SMOOTH", N_SMOOTH,
              {.default_value = 0,
               .desc = "Number of times to apply the smoothing operator to the initial condition",
               .units = "nondim"});
  params->close_block();

  bool debug = false;
  params->get("DEBUG", debug,
              {.default_value = false,
               .desc = "If true, write out verbose debugging data.",
               .units = "nondim",
               .debugging_param = true});

  bool global_indexing = false;
  params->get("GLOBAL_INDEXING", global_indexing,
              {.default_value = false,
               .desc = "If true, use global indexing for all I/O and internal operations. "
                       "If false, use local indexing with halo regions.",
               .layout_param = true});
  
  double rad_earth = 0.;
  params->get("RAD_EARTH", rad_earth,
              {.default_value = 6.378e6,
               .desc = "The radius of the Earth.",
               .units = "m"});

  // todo: set_calendar_type()
  //		Functionality from TIM/time_manager/time_manager.F90
  // defer: time_interp_external_init()
  // 		Initializes the interpolation capability in MOM
  // todo: initialize_MOM()
  //		Performs the core initialization of MOM
  // todo: get_MOM_state_eleemnts()
  // 		Functionality from src/core/MOM.F90
  // defer: initialize_ice_shelf_fluxes()
  // defer: initialize_ice_shelf_forces()	
  // defer: ice_shelf_query()
  // defer: data_override_init()
  // todo: extract_surface_state()
  // todo: surface_forcing_init()
  // 		config_src/drivers/solo_dirver/MOM_surface_forcing.F90
  // defer: MOM_wave_interface_init()
  // 		src/user/MOM_wave_interface.F90
  // todo: real_to_time()
  // defer: diag_mediator_close_registration()
  // defer: write_time_stamp_file()
  // todo: set_forcing()
  // 		MOM_surface_forcing.F90
  // todo: finish_MOM_initilization()
  // todo: step_MOM()
  // 		src/core/MOM.F90
  // defer: mech_forcing_diags()
  // defer: forcing_diagnostics()
  // defer: save_MOM_restart()
  // defer: write_ocean_solo_res()
  // defer: diag_mediator_end()
  // todo: MOM_end()

  initialize_MOM();
}

void Model::initialize_MOM() {

  int ni_global = 0;
  params->get("NIGLOBAL", ni_global,
                {.desc = "The total number of thickness grid points in the x-direction in the physical domain.",
                 .fail_if_missing=true});

  int nj_global = 0;
  params->get("NJGLOBAL", nj_global,
                {.desc = "The total number of thickness grid points in the y-direction in the physical domain.",
                 .fail_if_missing=true});

  int nk = 0;
  params->get("NK", nk,
                {.desc = "The total number of thickness grid points in the z-direction in the physical domain.",
                 .fail_if_missing=true});
  // Cell size in each direction
  amrex::Real dx = 100000;
  amrex::Real dy = 100000;
  amrex::Real dz = 100000;

  // Mesh will be broken into chunks of up to max_chunk_size
  int max_chunk_size = 32;

  // Number of time steps to take
  int n_time_steps = 4000;

  // Size of time step
  amrex::Real dt = 90;

  amrex::MultiFab psi;
  DefineCellCenteredMultiFab(ni_global, nj_global, nk, max_chunk_size, psi);

    // AMReX object to hold domain meta data... Like the physical size of the domain and if it is periodic in each direction
  amrex::Geometry geom;
  InitializeGeometry(ni_global, nj_global, nk, dx, dy, dz, geom);

  InitializeVariables(geom, psi);

  psi.FillBoundary(geom.periodicity());

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
