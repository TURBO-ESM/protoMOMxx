#!/bin/bash -e

: ${ROOTDIR="$(pwd)"}
: ${JOBS:="2"}
: ${PROTOMOM_ADD_TEST_TARGETS:="OFF"}
: ${PROTOMOM_ALL_TESTS:="OFF"}
: ${PROTOMOM_SYSTEM_TESTS:="OFF"}
: ${PROTOMOM_UNIT_TESTS:="OFF"}
: ${PROTOMOM_MPI_TESTS:="OFF"}
: ${CMAKE_BUILD_TYPE:="Release"}
: ${AMReX_GPU_BACKEND:="NONE"}
: ${BUILD_DIR:="${ROOTDIR}/build"}
: ${AMREX_ROOT:="${ROOTDIR}/dependencies/amrex"}
: ${FRESH_BUILD:="False"}

usage() {
  cat <<'EOF'
Usage: ./build_and_run.sh [options]

Builds AMReX (if not already present), configures and builds protoMOMxx, then
runs either the double_gyre example or one of the test suites.

Options:
  --gpu            Build with the CUDA backend. Loads cuda, builds AMReX
                   into dependencies/amrex-cuda and protoMOMxx into build-gpu.
  --tests          Build and run the unit tests instead of the example.
  --system-tests   Build and run the (slower) system tests instead, e.g.
                   full-executable smoke/regression tests.
  --mpi-tests      Build and run the multi-rank tests instead, i.e. those
                   launched under mpiexec.
  --all-tests      Build and run all test categories (unit, mpi and system).
  --debug          Build with CMAKE_BUILD_TYPE=Debug instead of Release.
  --fresh          Remove the build directory first, forcing a fresh CMake
                   configuration of protoMOMxx.
  --jobs N         Build with N parallel jobs (default: 2).
  -h, --help       Show this help and exit.

With no test option, the double_gyre example is run in a throwaway directory to
verify the build.

Examples:
  ./build_and_run.sh                      # build and run the double_gyre example
  ./build_and_run.sh --tests --jobs 8     # build and run the unit tests
  ./build_and_run.sh --debug --fresh      # clean Debug build

On Derecho use ./build_and_run_derecho.sh, which loads the required modules and
accepts these same options.

See also build.sh and create_env.sh, which this script wraps and which are
configured via environment variables.
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --gpu)
            module load cuda/12.9.0
            AMReX_GPU_BACKEND="CUDA"
            PROTOMOM_CUDA="ON"
            BUILD_DIR="${ROOTDIR}/build-gpu"
            AMREX_ROOT="${ROOTDIR}/dependencies/amrex-cuda" ;;
        --tests)
            PROTOMOM_UNIT_TESTS="ON" ;;
        --system-tests)
            PROTOMOM_SYSTEM_TESTS="ON" ;;
        --mpi-tests)
            PROTOMOM_MPI_TESTS="ON" ;;
        --all-tests)
            PROTOMOM_ALL_TESTS="ON" ;;
        --debug)
            CMAKE_BUILD_TYPE="Debug" ;;
        --fresh)
            FRESH_BUILD="True" ;;
        --jobs)
            JOBS="$2"
            # Verify that the number of jobs parsed is a valid integer > 0
            if [[ ! "${JOBS}" =~ ^[0-9]+$ ]]; then
                echo "--jobs option ${JOBS} not a valid positive integer."
                exit 1
            fi
            if [[ ! "${JOBS}" -gt 0 ]]; then
                echo "--jobs option ${JOBS} not greater than 0."
                exit 1
            fi
            shift ;;
        -h|--help)
            usage
            # 'return' when sourced (e.g. by build_and_run_derecho.sh),
            # 'exit' when executed directly.
            return 0 2>/dev/null || exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Run './build_and_run.sh --help' for usage." >&2
            return 1 2>/dev/null || exit 1 ;;
    esac
    shift
done

CMAKE_PREFIX_PATH="${AMREX_ROOT}/install"

# Tell CMake to build the test targets whenever any test category was requested.
if [[ "${PROTOMOM_UNIT_TESTS}" == "ON" \
      || "${PROTOMOM_SYSTEM_TESTS}" == "ON" \
      || "${PROTOMOM_MPI_TESTS}" == "ON" \
      || "${PROTOMOM_ALL_TESTS}" == "ON" ]]; then
  PROTOMOM_ADD_TEST_TARGETS="ON"
fi

. ./create_env.sh
. ./build.sh

if [[ "${PROTOMOM_ALL_TESTS}" == "ON" ]]; then
  ctest --test-dir "${BUILD_DIR}"
elif [[ "${PROTOMOM_SYSTEM_TESTS}" == "ON" ]]; then
  # System tests (e.g. full-executable smoke tests) are excluded by default
  ctest --test-dir "${BUILD_DIR}" -L system
elif [[ "${PROTOMOM_MPI_TESTS}" == "ON" ]]; then
  ctest --test-dir "${BUILD_DIR}" -L mpi
elif [[ "${PROTOMOM_UNIT_TESTS}" == "ON" ]]; then
  ctest --test-dir "${BUILD_DIR}" -L unit
else
  # No tests. Just run the double_gyre example to verify that the build was successful.
  # Do so in a throwaway dir so we don't overwrite the committed MOM_parameters_doc.* files.
  echo -e "\nRunning the double_gyre example..."
  RUN_DIR="${BUILD_DIR}/double_gyre_throwaway"
  mkdir -p "${RUN_DIR}"
  cp "${ROOTDIR}"/tests/double_gyre/{input.nml,MOM_input,MOM_override} "${RUN_DIR}/"
  (cd "${RUN_DIR}" && "${BUILD_DIR}/protoMOMxx")
fi
