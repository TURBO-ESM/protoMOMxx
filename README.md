# protoMOMxx

This repository contains the source code for protoMOMxx, an experimental prototype of the Modular Ocean Model (MOM) in C++ and AMReX
for code modernization and GPU acceleration. 

The code is currently in *very* early stages of development, and is ***not yet functional.***. The goal of this project is to explore the design and implementation of a modernized MOM codebase, and to provide a testbed for new features and optimizations.

## Quickstart

To build the protoMOMxx executable:

```
  cd protoMOMxx
  cmake -S . -B build
  cmake --build build -j
```

This will create the `protoMOMxx` executable in the `build` directory. You can run it with:

```
  ./protoMOMxx
```

To run the unit tests:

```
  cd protoMOMxx
  cmake -S . -B buildtest
  cmake --build buildtest -j
  ctest --test-dir buildtest --output-on-failure
```
