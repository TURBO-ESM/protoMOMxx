# protoMOMxx

This repository contains the source code for protoMOMxx, an experimental prototype of the Modular Ocean Model (MOM) in C++ and AMReX
for code modernization and GPU acceleration. 

The code is currently in *very* early stages of development, and is ***not yet functional.***. The goal of this project is to explore the design and implementation of a modernized MOM codebase, and to provide a testbed for new features and optimizations.

# Cloning

protoMOMxx includes TIM as a git submodule at `extern/TIM`, so clone recursively:

```bash
git clone --recurse-submodules https://github.com/TURBO-ESM/protoMOMxx.git
```

If you already cloned without `--recurse-submodules`, initialize the submodule
in place:

```bash
git submodule update --init --recursive
```

# Quick Start

The easiest way to build and run protoMOMxx is via the `build_and_run.sh` script, which
fetches and builds AMReX (if not already present), configures and builds protoMOMxx, and
then runs the executable (or the unit tests):

```bash
./build_and_run.sh
```

On Derecho, use the `build_and_run_derecho.sh` wrapper instead, which loads the required
modules before calling `build_and_run.sh`:

```bash
./build_and_run_derecho.sh
```

`build_and_run.sh` accepts the following options:

| Option          | Description                                                    |
|-----------------|------------------------------------------------------------------|
| `--gpu`         | Build with the CUDA backend (loads `cuda/12.9.0`, builds AMReX into `dependencies/amrex-cuda`, and protoMOMxx into `build-gpu`) |
| `--tests`       | Build and run the unit tests instead of the `protoMOMxx` executable |
| `--system-tests`| Build and run the (slower) system tests instead, e.g. full-executable smoke/regression tests |
| `--mpi-tests`   | Build and run the multi-rank tests instead, i.e. those launched under `mpiexec` |
| `--all-tests`   | Build and run all test categories (unit, mpi, and system) |
| `--debug`       | Build with `CMAKE_BUILD_TYPE=Debug` instead of `Release`       |
| `--fresh`       | Force a fresh CMake configuration of protoMOMxx even if the build directory already exists |
| `--jobs N`      | Build with `N` parallel jobs (default: 2)                      |

For example, to build and run the unit tests with 8 parallel jobs:

```bash
./build_and_run.sh --tests --jobs 8
```

## Manual build

`build_and_run.sh` is a thin wrapper around two scripts that can also be run independently:

- `create_env.sh` fetches (if necessary) and builds AMReX into `dependencies/amrex` (or
  `dependencies/amrex-cuda` when `AMReX_GPU_BACKEND=CUDA`), then installs it under
  `dependencies/<amrex-dir>/install`.
- `build.sh` configures and builds protoMOMxx itself, using `CMAKE_PREFIX_PATH` to locate
  the AMReX installation.

Both scripts are configured via environment variables (see the top of each script for the
full list, e.g. `JOBS`, `BUILD_DIR`, `CMAKE_BUILD_TYPE`, `PROTOMOM_ADD_TEST_TARGETS`, `PROTOMOM_CUDA`,
`FRESH_BUILD`). For example:

```bash
. ./create_env.sh
CMAKE_PREFIX_PATH="${AMREX_ROOT}/install" PROTOMOM_ADD_TEST_TARGETS=ON . ./build.sh
ctest --test-dir build -L unit    # fast unit tests (single rank, no launcher)
ctest --test-dir build -L mpi     # multi-rank tests, launched under mpiexec
ctest --test-dir build -L system  # slower full-executable system tests
ctest --test-dir build            # everything
```

The `double_gyre` system test also includes a regression check that diffs the freshly generated
`MOM_parameters_doc.*` output against golden copies committed under `tests/double_gyre/`. If a
change to default parameters is intentional, regenerate those golden files from the build tree
(`build/tests/double_gyre/MOM_parameters_doc.*`) and commit the update.

If you already have an AMReX installation available, you can skip `create_env.sh` and
configure protoMOMxx directly with CMake:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/amrex/install
cmake --build build -j
./build/protoMOMxx
```

## TIM

protoMOMxx links `TIM::tim`, TIM's C++ layer. If a TIM install can be found (add its prefix
to `CMAKE_PREFIX_PATH` or set `TIM_DIR`), that TIM is used. Otherwise `tim/cpp` is built from
`TIM_SOURCE_DIR`, which defaults to the `extern/TIM` submodule. An installed TIM needs MPI,
NetCDF and AMReX with their Fortran components, so a Fortran compiler must be available. Pass
`-DCMAKE_DISABLE_FIND_PACKAGE_TIM=ON` to always build the bundled copy.

## MOM C++ kernels and the MOM6 bridge

`src/mom_kernels/` holds AMReX ports of MOM6 kernels, and the `extern "C"` bridges that MOM6
calls from its `#ifdef _TIM` branches. Both used to live in TIM. They build as two targets:

| Target | Contents | Links |
|---|---|---|
| `protoMOMxx::mom_kernels` | the AMReX kernels (namespace `MOM`) | AMReX |
| `protoMOMxx::mom_bridge` | the `turbotmp_*_bridge` entry points and `turbotmp/turbotmp_helper` | `mom_kernels`, `TIM::tim`, AMReX |

Related CMake options:

| Option | Default | Effect |
|---|---|---|
| `PROTOMOM_KERNELS_ONLY` | `OFF` | Build only `mom_kernels` and `mom_bridge` (and, with tests on, `test_mom`), not the solo driver |
| `PROTOMOM_MOM_BRIDGE` | `ON` | Build `mom_bridge` |
| `PROTOMOM_INSTALL` | `OFF` | Install both targets and a `protoMOMxxConfig.cmake`. Needs an installed TIM |

With `PROTOMOM_INSTALL=ON`, MOM6 consumes the installed bridge with
`find_package(protoMOMxx REQUIRED COMPONENTS MOM_BRIDGE)` and links `protoMOMxx::mom_bridge`:

```bash
cmake -S . -B build-kernels -DPROTOMOM_KERNELS_ONLY=ON -DPROTOMOM_INSTALL=ON \
      -DCMAKE_PREFIX_PATH="/path/to/amrex/install;/path/to/tim/install" \
      -DCMAKE_INSTALL_PREFIX=/path/to/protomomxx/install
cmake --build build-kernels -j
cmake --install build-kernels
```

`tests/test_mom/` is the GoogleTest suite for the kernels. It replays captured Fortran I/O
passed through `-DTEST_MOM_DATA_DIR=<dir>`. With `PROTOMOM_ADD_TEST_TARGETS=ON` it is always
built, but it is registered with ctest (label `test_mom`) only when `TEST_MOM_DATA_DIR` is set.
It can also be configured on its own (`cmake -S tests/test_mom ...`), either against an
installed AMReX or with AMReX built from `AMREX_SOURCE_DIR`. See
`tests/test_mom/CMakeLists.txt`.

# Contributing

To run the doxygen documentation check, install doxygen and graphviz (via conda or your package manager), then run:

```bash
doxygen docs/Doxyfile
```