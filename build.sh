#!/bin/bash -e

: ${ROOTDIR:="$(pwd)"}
: ${BUILD_DIR:="${ROOTDIR}/build"}
: ${JOBS:="2"}
: ${PROTOMOM_TESTS:="OFF"}
: ${PROTOMOM_FETCH_DEPS:="ON"}
: ${CMAKE_BUILD_TYPE:="Release"}

cmake                                                 \
  -S .                                                \
  -B "${BUILD_DIR}"                                   \
  -DPROTOMOM_FETCH_DEPS:BOOL="${PROTOMOM_FETCH_DEPS}" \
  -DPROTOMOM_TESTS:BOOL="${PROTOMOM_TESTS}"           \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}"          \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
cmake --build  "${BUILD_DIR}" -j "${JOBS}"
