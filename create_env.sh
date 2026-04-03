#!/bin/bash -e

# Set default root directory unless already set
: ${ROOTDIR:=$(pwd)}
: ${DEPENDENCIES_DIR:="${ROOTDIR}/dependencies"}
: ${JOBS:="2"}
: ${AMReX_GPU_BACKEND:="NONE"}
: ${AMREX_ROOT:="${DEPENDENCIES_DIR}/amrex"}

AMREX_SRC="${AMREX_ROOT}/src"
AMREX_BUILD_DIR="${AMREX_ROOT}/build"
AMREX_INSTALL_DIR="${AMREX_ROOT}/install"

if [ ! -d "${AMREX_SRC}" ]; then
  mkdir -p "${AMREX_SRC}" && git clone --depth 1 --branch "26.04" https://github.com/AMReX-Codes/amrex.git "${AMREX_SRC}"
fi

if [ ! -d "${AMREX_BUILD_DIR}" ]; then
  cmake                                           \
    -S "${AMREX_SRC}"                             \
    -B "${AMREX_BUILD_DIR}"                       \
    -DAMReX_GPU_BACKEND="${AMReX_GPU_BACKEND}"    \
    -DCMAKE_INSTALL_PREFIX="${AMREX_INSTALL_DIR}"
  cmake --build "${AMREX_BUILD_DIR}" -j "${JOBS}"
fi

if [ ! -d "${AMREX_INSTALL_DIR}" ]; then
  cmake --install "${AMREX_BUILD_DIR}"
fi
