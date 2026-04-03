#!/bin/bash -e

: ${ROOTDIR="$(pwd)"}
: ${JOBS:="2"}
: ${PROTOMOM_TESTS:="OFF"}
: ${AMReX_GPU_BACKEND:="NONE"}
: ${BUILD_DIR:="${ROOTDIR}/build"}
: ${AMREX_ROOT:="${ROOTDIR}/dependencies/amrex"}

module purge
module load ncarenv/25.10
module load cmake/3.31.8 gcc/14.3.0 cray-mpich/8.1.32 ncarcompilers/1.2.0

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --gpu)
            module load cuda/12.9.0
            AMReX_GPU_BACKEND="CUDA"
            BUILD_DIR="${ROOTDIR}/build-gpu"
            AMREX_ROOT="${ROOTDIR}/dependencies/amrex-cuda" ;;
        --tests)
            PROTOMOM_TESTS="ON" ;;
        --jobs)
            JOBS="$2"
            if [[ ! "${JOBS}" =~ ^[0-9]+$ ]]; then
                echo "--jobs option ${JOBS} not a valid positive integer."
                exit 1
            fi
            if [[ ! "${JOBS}" -gt 0 ]]; then
                echo "--jobs option ${JOBS} not greater than 0."
                exit 1
            fi
            shift ;;
    esac
    shift
done

CMAKE_PREFIX_PATH="${AMREX_ROOT}/install"

. ./create_env.sh
. ./build.sh

if [ "${PROTOMOM_TESTS}" == "ON" ]; then
  ctest --test-dir "${BUILD_DIR}"
else
  ${BUILD_DIR}/protoMOMxx
fi