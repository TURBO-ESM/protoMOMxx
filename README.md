# protoMOMxx

This repository contains the source code for protoMOMxx, an experimental prototype of the Modular Ocean Model (MOM) in C++ and AMReX
for code modernization and GPU acceleration. 

The code is currently in *very* early stages of development, and is ***not yet functional.***. The goal of this project is to explore the design and implementation of a modernized MOM codebase, and to provide a testbed for new features and optimizations.

# Quick Start

To build the protoMOMxx executable:

```bash
cmake -S . -B build
cmake --build build -j
```

This will create the `protoMOMxx` executable in the `build` directory. To run the executable:

```bash
./build/protoMOMxx
```

To run the unit tests:

```bash
cmake -S . -B buildtest -DPROTOMOM_TESTS=ON
cmake --build buildtest -j
ctest --test-dir buildtest
```

# Contributing

To run the doxygen documentation check, install doxygen and graphviz (via conda or your package manager), then run:

```bash
doxygen docs/Doxyfile
```