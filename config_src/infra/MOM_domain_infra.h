#pragma once
/// @file MOM_domain_infra.h
/// @brief The MOM::Domain object is a thin wrapper over TIM::Domain (via composition).
///        The analogue of MOM6's MOM_domain_infra, which wraps FMS mpp domains.

#include <optional>

#include <AMReX_BoxArray.H>
#include <AMReX_DistributionMapping.H>
#include <AMReX_IntVect.H>
#include <AMReX_MultiFab.H>
#include <AMReX_Periodicity.H>

#include "core/tim_domain.hpp"
#include "core/tim_stagger.hpp"

namespace MOM {

/// @brief The staggering (grid residency) of a field. An alias of TIM::Stagger
using Stagger = TIM::Stagger;

/// @brief The construction specification of a Domain.
struct DomainSpec {
  int ni_global = 0;  ///< Number of global grid points in the i-direction (x).
  int nj_global = 0;  ///< Number of global grid points in the j-direction (y).
  int ni_halo = 0;    ///< Number of halo points in the i-direction.
  int nj_halo = 0;    ///< Number of halo points in the j-direction.
  bool reentrant_x = false;  ///< Whether the i-direction is periodic (reentrant).
  bool reentrant_y = false;  ///< Whether the j-direction is periodic (reentrant).
  bool tripolar_n = false;   ///< Whether the domain uses a tripolar grid at the
                             ///< northern boundary. Not implemented yet.
  /// @brief Number of boxes to decompose the domain into; nullopt (the
  /// default) means one box per rank.
  std::optional<int> n_boxes = std::nullopt;
};

/// @class Domain
/// @brief The computational domain of a model instance: global horizontal
/// extents, connectivity, halo metadata, and the horizontal decomposition.
///
/// Domain is a thin wrapper over TIM::Domain (the analogue of MOM6's
/// MOM_domain_infra module wrapping an FMS mpp domain2D): the decomposition
/// mechanics (the cell-centered amrex::BoxArray, the amrex::DistributionMapping
/// that assigns boxes to ranks, and the periodicity used for halo exchanges)
/// live in TIM. This class maps MOM's vocabulary (reentrant_{x|y}, tripolar_n)
/// onto TIM's plain connectivity flags and is the home for future MOM-specific
/// domain policy.
///
/// The Domain stays agnostic of the vertical grid: the decomposition and the
/// number of levels are combined at field-creation sites via box_array(),
/// like MOM6 combining G and GV at allocation. Halo widths are metadata to be
/// consumed at field creation (AMReX halos are per-field ghost cells), not
/// baked into the domain's index space.
class Domain {
public:
  /// @brief Construct the domain and its horizontal decomposition.
  /// @param spec The domain specification; see DomainSpec.
  /// @pre The infrastructure layer (MOM::Infra) is initialized.
  /// @pre The values are consistent (TIM aborts otherwise); the
  ///      configuration-driven path validates them in make_domain first.
  explicit Domain(const DomainSpec &spec);

  /// @brief Number of global grid points in the i-direction (x).
  /// @return The global i-extent.
  int ni_global() const { return domain_.ni_global(); }

  /// @brief Number of global grid points in the j-direction (y).
  /// @return The global j-extent.
  int nj_global() const { return domain_.nj_global(); }

  /// @brief Number of halo points in the i-direction.
  /// @return The i-direction halo width.
  int ni_halo() const { return domain_.ni_halo(); }

  /// @brief Number of halo points in the j-direction.
  /// @return The j-direction halo width.
  int nj_halo() const { return domain_.nj_halo(); }

  /// @brief The halo widths as a MultiFab ghost-cell vector. Horizontal-only:
  /// the k-component is always 0.
  /// @return {ni_halo, nj_halo, 0}.
  amrex::IntVect nghost() const { return domain_.nghost(); }

  /// @brief True if the i-direction is periodic (reentrant).
  /// @return The zonal reentrancy flag.
  bool reentrant_x() const { return domain_.periodic_x(); }

  /// @brief True if the j-direction is periodic (reentrant).
  /// @return The meridional reentrancy flag.
  bool reentrant_y() const { return domain_.periodic_y(); }

  /// @brief True if the domain uses a tripolar grid at the northern boundary.
  /// @return The tripolar connectivity flag.
  bool tripolar_n() const { return domain_.tripolar_n(); }

  /// @brief Cell-centered BoxArray covering the global domain.
  /// @param n_levels Number of vertical levels of the field to be created:
  ///        1 for 2-D fields, NK for 3-D layer fields, NK+1 for interface
  ///        fields, etc.
  /// @return The cell-centered BoxArray with the requested k-extent.
  amrex::BoxArray box_array(const int n_levels) const {
    return domain_.boxArray(n_levels);
  }

  /// @brief Number of boxes in the horizontal decomposition.
  /// @return The box count.
  int n_boxes() const { return domain_.n_boxes(); }

  /// @brief The assignment of the horizontal boxes to processors.
  /// @return The distribution mapping of the horizontal decomposition.
  const amrex::DistributionMapping &distribution_mapping() const {
    return domain_.distribution_mapping();
  }

  /// @brief Create a distributed field on this domain's decomposition: a
  /// MultiFab with the requested staggering, vertical extent, and component
  /// count, carrying the domain's halo widths as ghost cells unless overridden.
  /// @param stagger Where the field's values sit within a grid cell.
  /// @param n_levels Number of vertical levels of the field. Must be positive.
  /// @param ncomp Number of field components. Must be positive.
  /// @param nghost Ghost-cell widths of the field. The default is the domain's
  ///        halo widths. Halos are horizontal-only.
  /// @return The newly created field.
  amrex::MultiFab make_field(const Stagger stagger, const int n_levels,
                             const int ncomp,
                             const std::optional<amrex::IntVect> nghost = std::nullopt) const {
    return domain_.make_field(stagger, n_levels, ncomp, nghost);
  }

  /// @brief The domain's periodicity, for halo exchanges.
  /// @return The global extent in each periodic direction, 0 otherwise.
  amrex::Periodicity periodicity() const { return domain_.periodicity(); }

private:
  /// @brief The underlying TIM domain: the decomposition mechanics.
  TIM::Domain domain_;
};

} // namespace MOM
