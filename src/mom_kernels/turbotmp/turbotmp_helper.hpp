/**
 * @file turbotmp_helper.hpp
 * @brief Host<->device transfer and Fortran<->C layout helpers used by the
 *        turbotmp_*_bridge entry points.
 */

#pragma once
#include <AMReX_Array4.H>
#include <AMReX_MultiFab.H>

#include "turbotmp_bridge_c_types.h"

/// Helpers shared by the extern "C" bridges; not part of the installed surface.
namespace turbotmp {

/// A device-resident copy of one Fortran real array, viewed as an AMReX Array4.
struct A4Box
{
    amrex::Box bx;                   ///< Index box the array occupies (0-based, positioned at lb-1).
    amrex::Real* data = nullptr;     ///< Raw device pointer, C order.
    amrex::Array4<amrex::Real> arr;  ///< AMReX view of `data`, C order.

    amrex::Real* data_f = nullptr;  ///< Raw device pointer, Fortran order.

    int nx;     ///< Extent in x.
    int ny;     ///< Extent in y.
    int nz;     ///< Extent in z.
    int ncomp;  ///< Number of components.
};

   /**
    * @brief Allocate an A4Box for a Fortran real array.
    *
    * Sizes and positions the array from the descriptor's shape[] and lb[]
    * (the values returned by shape() and lbound() on the Fortran side). A 2D
    * descriptor (dim == 2) gets nz = 1 and lbz = 1. ncomp is always 1.
    * Aborts unless dim is 2 or 3.
    * The returned A4Box's box -- and therefore a4.arr's valid index range --
    * is positioned at the array's true absolute location (lb-1 .. lb-1+n-1),
    * not just [0, n-1]. Staggered-grid arrays (u/v-point fields) legitimately
    * have lb != 1, and shared iteration boxes require each array4 to be
    * positioned at its own real offset so indexing hits the correct element.
    *
    * @param a Descriptor of the Fortran array.
    * @return The allocated A4Box; release it with free_array4().
    */
   A4Box make_array4(const RealArray_C* a);
   /**
    * @brief Free the device buffers of an A4Box.
    * @param a4 Box to free; its pointers are reset to nullptr.
    */
   void free_array4(A4Box& a4);
   /**
    * @brief Copy a Fortran-order host array into an A4Box, transposing to C order.
    * @param f  Host array in Fortran order.
    * @param a4 Destination box.
    */
   void copy_FortranHost_to_array4(const double* f, A4Box& a4);
   /**
    * @brief Copy an A4Box back to a Fortran-order host array.
    * @param a4 Source box.
    * @param f  Host array in Fortran order.
    */
   void copy_array4_to_FortranHost(const A4Box& a4, double* f);

/// IntA4Box mirrors A4Box for integer-typed data. make_int_array4 builds one
/// from a LogicalArray_C (an integer-encoded 0/1 shadow of a Fortran logical
/// array). There is no IntArray_C overload yet; add one when a bridge first
/// takes an IntArray_C.
struct IntA4Box
{
    amrex::Box bx;           ///< Index box the array occupies (0-based, positioned at lb-1).
    int* data = nullptr;     ///< Raw device pointer, C order.
    amrex::Array4<int> arr;  ///< AMReX view of `data`, C order.

    int* data_f = nullptr;  ///< Raw device pointer, Fortran order.

    int nx;     ///< Extent in x.
    int ny;     ///< Extent in y.
    int nz;     ///< Extent in z.
    int ncomp;  ///< Number of components.
};

   /**
    * @brief Allocate an IntA4Box for a Fortran logical array. See
    *        make_array4() for the sizing and positioning convention, which
    *        is identical.
    * @param a Descriptor of the integer-encoded logical array.
    * @return The allocated IntA4Box; release it with free_int_array4().
    */
   IntA4Box make_int_array4(const LogicalArray_C* a);
   /**
    * @brief Free the device buffers of an IntA4Box.
    * @param a4 Box to free; its pointers are reset to nullptr.
    */
   void free_int_array4(IntA4Box& a4);
   /**
    * @brief Copy a Fortran-order host int array into an IntA4Box, transposing to C order.
    * @param f  Host array in Fortran order.
    * @param a4 Destination box.
    */
   void copy_FortranHost_to_int_array4(const int* f, IntA4Box& a4);
   /**
    * @brief Copy an IntA4Box back to a Fortran-order host int array.
    * @param a4 Source box.
    * @param f  Host array in Fortran order.
    */
   void copy_int_array4_to_FortranHost(const IntA4Box& a4, int* f);

   /**
    * @brief Convert a Fortran (1-based) Box_C to an AMReX (0-based) Box.
    * @param b Box with Fortran start and end indices.
    * @return The same index range, shifted by -1 in every direction.
    */
   amrex::Box to_box(const Box_C* b);
}
