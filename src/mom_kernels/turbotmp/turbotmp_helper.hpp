/**
 * @file turbotmp_helper.hpp
 * @brief Host<->device transfer and Fortran<->C layout helpers used by the
 *        turbotmp_*_bridge entry points.
 */

#pragma once
#include <AMReX_Array4.H>
#include <AMReX_MultiFab.H>

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
    * lbx/lby/lbz are the array's Fortran index-space lower bounds (i.e., the
    * values returned by lbound() on the Fortran side). For a faked-out 2D
    * dimension, pass 1 (matching how nz=1 is passed for 2D arrays).
    * The returned A4Box's box -- and therefore a4.arr's valid index range --
    * is positioned at the array's true absolute location (lb-1 .. lb-1+n-1),
    * not just [0, n-1]. Staggered-grid arrays (u/v-point fields) legitimately
    * have lb != 1, and shared iteration boxes require each array4 to be
    * positioned at its own real offset so indexing hits the correct element.
    *
    * @param nx    Extent in x.
    * @param ny    Extent in y.
    * @param nz    Extent in z.
    * @param ncomp Number of components.
    * @param lbx   Fortran lower bound in x.
    * @param lby   Fortran lower bound in y.
    * @param lbz   Fortran lower bound in z.
    * @return The allocated A4Box; release it with free_array4().
    */
   A4Box make_array4(int nx, int ny, int nz, int ncomp, int lbx, int lby, int lbz);
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

/// IntA4Box mirrors A4Box for integer-typed data. Serves both LogicalArray_C
/// (integer-encoded 0/1) and IntArray_C -- both are plain `int*` on the C side.
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
    * @brief Allocate an IntA4Box. See make_array4() for the lbx/lby/lbz
    *        convention, which is identical.
    * @param nx    Extent in x.
    * @param ny    Extent in y.
    * @param nz    Extent in z.
    * @param ncomp Number of components.
    * @param lbx   Fortran lower bound in x.
    * @param lby   Fortran lower bound in y.
    * @param lbz   Fortran lower bound in z.
    * @return The allocated IntA4Box; release it with free_int_array4().
    */
   IntA4Box make_int_array4(int nx, int ny, int nz, int ncomp, int lbx, int lby, int lbz);
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
}
