#pragma once
/// @file MOM.h
/// @brief Main header for the Modular Ocean Model (MOM) core.

#include <AMReX.H>
#include <AMReX_MultiFab.H>

namespace MOM {

/// @brief The Model class serves as the main interface for the Modular Ocean Model (MOM) core.
/// It encapsulates the runtime parameters and provides an entry point for initializing, running,
/// and finalizing the model.
class Model {
public:

  /// @brief Model constructor that initializes the model with the given ensemble number.
  /// @param ensemble_num The ensemble number for the model run; default is -1 (indicating no ensemble).
  explicit Model(const int ensemble_num = -1);


private:
  const int ensemble_num_;

  void initialize_MOM();

  void InitializeVariables(const amrex::Geometry & geom,
                         amrex::MultiFab & psi);
  void DefineCellCenteredMultiFab(const int nx, const int ny,
                                const int max_chunk_size,
                                amrex::MultiFab & cell_centered_MultiFab);
  void InitializeGeometry(const int nx, const int ny,
                        const amrex::Real dx, const amrex::Real dy,
                        amrex::Geometry & geom);
  amrex::Real LinearMapCoordinates(const amrex::Real x, 
                                 const amrex::Real x_min, const amrex::Real x_max,
                                 const amrex::Real xi_min, const amrex::Real xi_max);
};

} // namespace MOM
