#pragma once

#include <cstddef>

namespace MOM {


/**
 * @class Domain
 * @brief Represents the computational domain for MOM.
 *
 */
class Domain {
public:

  /**
   * @brief Constructor for domain.
   * @param ni_global Number of global grid points in the i-direction (x).
   * @param nj_global Number of global grid points in the j-direction (y).
   * @param ni_halo   Number of halo points in the i-direction.
   * @param nj_halo   Number of halo points in the j-direction.
   * @param reentrant_i Whether the i-direction is periodic (reentrant).
   * @param reentrant_j Whether the j-direction is periodic (reentrant).
   * @param tripolar_N  Whether the domain uses a tripolar grid at the northern boundary.
   */
  Domain(const std::size_t ni_global, const std::size_t nj_global,
         const std::size_t ni_halo, const std::size_t nj_halo,
         const bool reentrant_i, const bool reentrant_j,
         const bool tripolar_N);

  /**
   * @brief Number of global grid points in the i-direction (x).
   */
  const std::size_t ni_global;

  /**
   * @brief Number of global grid points in the j-direction (y).
   */
  const std::size_t nj_global;

  /**
   * @brief Number of halo points in the i-direction.
   */
  const std::size_t ni_halo;

  /**
   * @brief Number of halo points in the j-direction.
   */
  const std::size_t nj_halo;

  /**
   * @brief True if the i-direction is periodic (reentrant).
   */
  const bool reentrant_i;

  /**
   * @brief True if the j-direction is periodic (reentrant).
   */
  const bool reentrant_j;

  /**
   * @brief True if the domain uses a tripolar grid at the northern boundary.
   */
  const bool tripolar_N;
};

} // namespace MOM