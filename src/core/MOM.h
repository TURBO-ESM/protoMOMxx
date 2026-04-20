#pragma once
/// @file MOM.h
/// @brief Main header for the Modular Ocean Model (MOM) core.

#include <AMReX.H>
#include <AMReX_MultiFab.H>

#include "MOM_file_parser.h"
#include <memory>

namespace MOM {

/// @brief Scales input to be within necessary bounds
/// @param x Global index
/// @param x_min x min
/// @param x_max x max
/// @param xi_min grid x min
/// @param xi_max grid x max
/// @return Index within scaled space for local grid
AMREX_GPU_DEVICE AMREX_FORCE_INLINE
amrex::Real LinearMapCoordinates(const amrex::Real x,
                                 const amrex::Real x_min, const amrex::Real x_max,
                                 const amrex::Real xi_min, const amrex::Real xi_max);

/// @brief The Model class serves as the main interface for the Modular Ocean Model (MOM) core.
/// It encapsulates the runtime parameters and provides an entry point for initializing, running,
/// and finalizing the model.
class Model {
public:

  /// @brief Model constructor that initializes the model with the given ensemble number.
  /// @param ensemble_num The ensemble number for the model run; default is -1 (indicating no ensemble).
  explicit Model(const int ensemble_num = -1);
  /// @brief Initializes AMReX multifab data with static values bounded by geometry object
  /// @param geom Bounds for variable initialization
  /// @param psi Container of fields (variables).
  void InitializeVariables(const amrex::Geometry & geom,
                         amrex::MultiFab & psi);

private:
  std::shared_ptr<RuntimeParams> params;
  const int ensemble_num_;

  void initialize_MOM(int nx, int ny, int nz);
  void DefineCellCenteredMultiFab(const int nx, const int ny, const int nz,
                                const int max_chunk_size,
                                amrex::MultiFab & cell_centered_MultiFab);
  void InitializeGeometry(const int nx, const int ny, const int nz,
                        const amrex::Real dx, const amrex::Real dy, const amrex::Real dz,
                        amrex::Geometry & geom);
};

} // namespace MOM
