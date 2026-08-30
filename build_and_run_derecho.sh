#!/bin/bash -e

for arg in "$@"; do
  case "${arg}" in
    -h|--help)
      cat <<'EOF'
Usage: ./build_and_run_derecho.sh [options]

Derecho wrapper around build_and_run.sh. Purges the module environment, loads
the current default Derecho modules (ncarenv, cmake, gcc, cray-mpich and
ncarcompilers), then forwards all options to build_and_run.sh.

The help for build_and_run.sh, whose options this script accepts, follows.
--------------------------------------------------------------------------
EOF
      ./build_and_run.sh --help
      # 'return' when sourced, 'exit' when executed directly.
      return 0 2>/dev/null || exit 0 ;;
  esac
done

# Current default modules on Derecho
module purge
module load ncarenv/25.10
module load cmake/3.31.8 gcc/14.3.0 cray-mpich/8.1.32 ncarcompilers/1.2.0

. ./build_and_run.sh "$@"
