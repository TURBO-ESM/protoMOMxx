#include "MOM.h"
#include "MOM_logger.h"

#include <AMReX_ParallelDescriptor.H>
#include <AMReX_Array.H>

namespace MOM {

Model::Model(const int ensemble_num)
  : ensemble_num_(ensemble_num) {

  // todo: read VERBOSITY param instead of hardcoding it below
  logger::set_verbosity(logger::LogLevel::DEBUG);

  logger::info("Initializing protoMOMxx...");
  if (ensemble_num_ >= 0) {
    logger::info("Ensemble number: ", ensemble_num_);
  }

  // todo: Read namelist file
  //
  // todo: get_MOM_input():
  //          	Gets the names of the I/O directories and initialization files
  //          	Also calls the subroutine that opens run-time parameter files
  //          	Needs namelist file capability
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
  // Number of cells on each direction
  int nx = 64;
  int ny = 64;
  int nz = 64;

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
  DefineCellCenteredMultiFab(nx, ny, nz, max_chunk_size, psi);

    // AMReX object to hold domain meta data... Like the physical size of the domain and if it is periodic in each direction
  amrex::Geometry geom;
  InitializeGeometry(nx, ny, nz, dx, dy, dz, geom);

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

void Model::DefineCellCenteredMultiFab(const int nx, const int ny, const int nz,
                                       const int max_chunk_size,
                                       amrex::MultiFab & cell_centered_MultiFab)
{
    // lower and upper indices of domain
    const amrex::IntVect domain_low_index(AMREX_D_DECL(0,0,0));
    const amrex::IntVect domain_high_index(AMREX_D_DECL(nx-1, ny-1, nz-1)); // Need to determine number of z levels.
    
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

void Model::InitializeGeometry(const int nx, const int ny, const int nz,
                        const amrex::Real dx, const amrex::Real dy, const amrex::Real dz,
                        amrex::Geometry & geom)
{
  // lower and upper indices of domain
  const amrex::IntVect domain_low_index(0,0,0);
  const amrex::IntVect domain_high_index(nx-1, ny-1, nz-1);

  // create box of indicies for cells
  const amrex::Box cell_centered_box(domain_low_index, domain_high_index);

  // physical min and max boundaries of cells
  const amrex::RealBox real_box({0, 0, 0},
                                {nx*dx, ny*dy, nz*dz});

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
