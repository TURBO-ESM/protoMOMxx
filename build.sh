#!/bin/bash -e

usage() {
  cat <<'EOF'
Usage: ./build.sh
       . ./build.sh

Configures and builds protoMOMxx with CMake. Assumes AMReX is already built and
installed; use create_env.sh for that, or build_and_run.sh to do both at once.

This script takes no options -- it is configured entirely via environment
variables:

  ROOTDIR                       Project root directory        (default: $(pwd))
  BUILD_DIR                     CMake build directory         (default: ${ROOTDIR}/build)
  JOBS                          Parallel build jobs           (default: 2)
  CMAKE_BUILD_TYPE              Release or Debug              (default: Release)
  CMAKE_PREFIX_PATH             Where CMake looks for AMReX   (default: unset)
  FRESH_BUILD                   True removes BUILD_DIR first  (default: False)
  PROTOMOM_CUDA                 ON builds the CUDA backend    (default: OFF)
  PROTOMOM_FETCH_DEPS           ON auto-downloads missing deps such as GoogleTest
                                                              (default: ON)
  PROTOMOM_ADD_TEST_TARGETS     ON builds the test targets    (default: OFF)

Example:
  . ./create_env.sh
  CMAKE_PREFIX_PATH="${AMREX_ROOT}/install" PROTOMOM_ADD_TEST_TARGETS=ON JOBS=8 . ./build.sh
EOF
}

for arg in "$@"; do
  case "${arg}" in
    -h|--help)
      usage
      # 'return' when sourced, 'exit' when executed directly.
      return 0 2>/dev/null || exit 0 ;;
  esac
done

: ${ROOTDIR:="$(pwd)"}
: ${BUILD_DIR:="${ROOTDIR}/build"}
: ${JOBS:="2"}
: ${PROTOMOM_ADD_TEST_TARGETS:="OFF"}
: ${PROTOMOM_FETCH_DEPS:="ON"}
: ${CMAKE_BUILD_TYPE:="Release"}
: ${FRESH_BUILD:="False"}
: ${PROTOMOM_CUDA:="OFF"}

if [[ "${FRESH_BUILD}" == "True" ]]; then
  rm -rf "${BUILD_DIR}"
fi
cmake                                                 \
  -S .                                                \
  -B "${BUILD_DIR}"                                   \
  -DPROTOMOM_CUDA="${PROTOMOM_CUDA}"                  \
  -DPROTOMOM_FETCH_DEPS:BOOL="${PROTOMOM_FETCH_DEPS}" \
  -DPROTOMOM_ADD_TEST_TARGETS:BOOL="${PROTOMOM_ADD_TEST_TARGETS}"           \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}"          \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
cmake --build  "${BUILD_DIR}" -j "${JOBS}"
