#!/bin/bash -e

# Current default modules on Derecho
module purge
module load ncarenv/25.10
module load cmake/3.31.8 gcc/14.3.0 cray-mpich/8.1.32 ncarcompilers/1.2.0

. ./buiil_and_run.sh "$@"