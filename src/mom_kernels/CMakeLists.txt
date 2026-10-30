# MOM C++ kernels, and the Fortran<->C++ bridge MOM6 calls under -D_TIM.
# Moved here from TIM's mom/cpp/ and turbotmp/.
#
#   mom_kernels  the AMReX kernels alone (protoMOMxx::mom_kernels). Needs only
#                AMReX.
#   mom_bridge   the extern "C" bridges and their helper (protoMOMxx::mom_bridge).
#                Links mom_kernels and TIM::tim (for turbotmp_bridge_c_types.h).
#
# Both are exported (find_package(protoMOMxx COMPONENTS MOM_BRIDGE)), so the
# bridge links mom_kernels rather than recompiling the kernel sources: the
# kernel objects come from one archive only.
#
# Also configured by tests/test_mom when that is built standalone. It sets
# PROTOMOM_MOM_BRIDGE=OFF so only mom_kernels is defined and TIM is not needed.

# AMReX defines AMReX::amrex_3d as an ALIAS of amrex_3d in the same breath
# (Src/CMakeLists.txt), so the namespaced name covers both the add_subdirectory
# and find_package paths.
if(NOT TARGET AMReX::amrex_3d)
    find_package(AMReX REQUIRED)
endif()

set(mom_turbotmp_dir ${CMAKE_CURRENT_SOURCE_DIR}/turbotmp)

# Settings shared by every target built from these sources.
#
# Include directories are deliberately not set here -- both targets are
# exported and so need BUILD_INTERFACE/INSTALL_INTERFACE forms, which
# install(EXPORT) rejects a bare source path for.
#
# The FP-precision options are not set here either but per source file, at the
# bottom of this file (see the comment there). A source that misses them still
# compiles and links, it just stops being bit-identical to Fortran, which is the
# one property this directory exists to guarantee.
function(mom_cpp_configure tgt)
    target_link_libraries(${tgt} PUBLIC AMReX::amrex_3d)

    target_compile_features(${tgt} PUBLIC cxx_std_17)
endfunction()

# The AMReX kernels in namespace MOM. Their headers include only AMReX, so this
# target needs nothing from TIM.
add_library(mom_kernels STATIC
    mom_continuity_ppm.cpp
    mom_interface_heights.cpp
    mom_coriolisadv.cpp
    mom_vert_friction.cpp
    mom_hor_visc.cpp
)
add_library(protoMOMxx::mom_kernels ALIAS mom_kernels)
mom_cpp_configure(mom_kernels)
# Build-tree only: the kernel headers are not part of the installed surface.
target_include_directories(mom_kernels PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

# The extern "C" bridges MOM6 calls from its `#ifdef _TIM` branches (e.g.
# src/core/MOM_continuity_PPM.F90). turbotmp_helper is used only by the bridges.
if(PROTOMOM_MOM_BRIDGE)
    add_library(mom_bridge STATIC
        turbotmp_mom_continuity_ppm_bridge.cpp
        turbotmp_mom_interface_heights_bridge.cpp
        turbotmp_mom_coriolisadv_bridge.cpp
        turbotmp_mom_vert_friction_bridge.cpp
        turbotmp_mom_hor_visc_bridge.cpp
        ${mom_turbotmp_dir}/turbotmp_helper.cpp
    )
    add_library(protoMOMxx::mom_bridge ALIAS mom_bridge)
    mom_cpp_configure(mom_bridge)
    # TIM::tim is PUBLIC because the installed bridge headers include
    # turbotmp_bridge_c_types.h, which TIM installs next to tim_coms_infra_C_API.h.
    target_link_libraries(mom_bridge
        PUBLIC  TIM::tim
        PRIVATE mom_kernels)
    target_include_directories(mom_bridge PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${mom_turbotmp_dir}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/protoMOMxx>)

    if(PROTOMOM_INSTALL)
        # mom_bridge is static, so its exported link interface names mom_kernels
        # ($<LINK_ONLY:mom_kernels>) even though the link is PRIVATE; both must be
        # in the same export set or install(EXPORT) fails.
        install(TARGETS mom_kernels mom_bridge
            EXPORT protoMOMxxExports
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        # The bridge's public surface, and only that. MOM6 declares the entry
        # points with Fortran `interface` blocks and includes nothing, but a C or
        # C++ consumer needs these. turbotmp_bridge_c_types.h comes from TIM. The
        # kernel headers and turbotmp_helper.hpp are implementation headers
        # reached only from the .cpp files, so they are not installed.
        install(FILES
            turbotmp_mom_continuity_ppm_bridge.h
            turbotmp_mom_interface_heights_bridge.h
            turbotmp_mom_grid_c_types.h
            turbotmp_mom_coriolisadv_bridge.h
            turbotmp_mom_vert_friction_bridge.h
            turbotmp_mom_hor_visc_bridge.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/protoMOMxx)
    endif()
endif()

set(mom_cpp_sources
    mom_continuity_ppm.cpp
    mom_interface_heights.cpp
    turbotmp_mom_continuity_ppm_bridge.cpp
    turbotmp_mom_interface_heights_bridge.cpp
    mom_coriolisadv.cpp
    turbotmp_mom_coriolisadv_bridge.cpp
    mom_vert_friction.cpp
    turbotmp_mom_vert_friction_bridge.cpp
    mom_hor_visc.cpp
    turbotmp_mom_hor_visc_bridge.cpp
    ${mom_turbotmp_dir}/turbotmp_helper.cpp
)

# Every source here includes AMReX headers. With the CUDA backend those headers
# use __host__/__device__ qualifiers and CUDA kernel-launch syntax, so nvcc must
# compile them; CMake will not infer that from a .cpp extension. Source-file
# properties are directory-scoped, so this covers both targets above.
if(AMReX_GPU_BACKEND STREQUAL "CUDA")
    set_source_files_properties(${mom_cpp_sources} PROPERTIES LANGUAGE CUDA)
endif()

# To ensure bit-identical results with the Fortran reference, we need to
# disable FMA and reassociation, on the host and, with CUDA, on the device.
#
# These are source-file COMPILE_OPTIONS rather than target options because
# CMake places source options after CMAKE_<LANG>_FLAGS and after every target
# option, including the usage requirements inherited from AMReX::amrex_3d.
# AMReX puts fast-math there (--use_fast_math under AMReX_CUDA_FASTMATH, which
# defaults ON; -ffast-math under AMReX_FASTMATH), and both compilers resolve
# conflicting options last-one-wins, so ours have to come last.
#
# - GNU/Clang: -ffp-contract=off alone does not undo -ffast-math's
#   reassociation, so -fno-fast-math comes first.
# - nvcc: --use_fast_math implies --fmad=true --ftz=true --prec-div=false
#   --prec-sqrt=false; each is reset explicitly. Host flags are not nvcc
#   options and must go through -Xcompiler. nvcc's host compiler is taken to
#   be the C++ compiler (CMake's default for CMAKE_CUDA_HOST_COMPILER).
set(mom_fp_options
    # Host C++ compile
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fno-fast-math>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-ffp-contract=off>
    $<$<COMPILE_LANG_AND_ID:CXX,IntelLLVM,Intel>:-fp-model=precise>
    # nvcc: device code
    $<$<COMPILE_LANG_AND_ID:CUDA,NVIDIA>:--fmad=false>
    $<$<COMPILE_LANG_AND_ID:CUDA,NVIDIA>:--ftz=false>
    $<$<COMPILE_LANG_AND_ID:CUDA,NVIDIA>:--prec-div=true>
    $<$<COMPILE_LANG_AND_ID:CUDA,NVIDIA>:--prec-sqrt=true>
    # nvcc: host code, forwarded to the host compiler
    $<$<AND:$<COMPILE_LANG_AND_ID:CUDA,NVIDIA>,$<CXX_COMPILER_ID:GNU,Clang>>:-Xcompiler=-fno-fast-math,-ffp-contract=off>
    $<$<AND:$<COMPILE_LANG_AND_ID:CUDA,NVIDIA>,$<CXX_COMPILER_ID:IntelLLVM,Intel>>:-Xcompiler=-fp-model=precise>
)
set_source_files_properties(${mom_cpp_sources} PROPERTIES COMPILE_OPTIONS "${mom_fp_options}")
