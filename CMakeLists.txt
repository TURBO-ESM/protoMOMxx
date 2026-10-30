cmake_minimum_required(VERSION 3.24)
project(protoMOMxx)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PROTOMOM_CUDA "Build with Cuda backend" OFF)
option(PROTOMOM_ADD_TEST_TARGETS "Build test targets" OFF)
option(PROTOMOM_FETCH_DEPS "Fetch missing dependencies automatically" ON)
option(PROTOMOM_MOM_BRIDGE "Build the MOM Fortran<->C++ bridge (protoMOMxx::mom_bridge)" ON)
option(PROTOMOM_KERNELS_ONLY
  "Build only the MOM kernel and bridge targets (mom_kernels, mom_bridge), not the solo driver" OFF)
option(PROTOMOM_INSTALL
  "Install mom_kernels/mom_bridge and the protoMOMxx package config (needs an installed TIM)" OFF)

# An AMReX or TIM built with Fortran requires the importing project to enable
# Fortran before find_package (AMReXConfig.cmake hard-errors otherwise, and
# TIMConfig.cmake finds MPI and NetCDF with their Fortran components). The same
# pattern as TIM's tim/cpp/CMakeLists.txt; a no-op without a Fortran compiler.
include(CheckLanguage)
check_language(Fortran)
if(CMAKE_Fortran_COMPILER)
  enable_language(Fortran)
endif()

if(PROTOMOM_CUDA)
  enable_language(CUDA)
  find_package(AMReX REQUIRED CUDA)
  include(AMReXTargetHelpers)
else()
  find_package(AMReX REQUIRED)
endif()

include(cmake/protomomtarget.cmake)

# TIM (tim/cpp) yields the TIM::tim target. Use an existing one when there is
# one -- a TIM::tim target already defined by an enclosing project, or an
# installed TIM found through find_package (point CMAKE_PREFIX_PATH or TIM_DIR
# at it) -- so a consumer that already links TIM does not get a second copy of
# the tim/cpp symbols. Pass -DCMAKE_DISABLE_FIND_PACKAGE_TIM=ON to skip the
# search.
if(NOT TARGET TIM::tim)
  find_package(TIM CONFIG QUIET)
  # TIM_DIR is set when a TIMConfig.cmake was located. If it was located but
  # failed to load (e.g. a missing Fortran dependency), stop rather than
  # silently building a second TIM next to the one the caller pointed at.
  # A disabled search leaves TIM_FOUND unset while a TIM_DIR cached by an
  # earlier configure remains, so that case is not a load failure.
  if(NOT CMAKE_DISABLE_FIND_PACKAGE_TIM AND NOT TIM_FOUND AND TIM_DIR)
    message(FATAL_ERROR
      "Found TIM's package config in '${TIM_DIR}' but it failed to load; see the "
      "messages above. An installed TIM needs MPI, NetCDF and AMReX with their "
      "Fortran components, so a Fortran compiler must be available.")
  endif()
  if(TIM_FOUND)
    message(STATUS "Using installed TIM from ${TIM_DIR}")
  endif()
endif()

# Otherwise build tim/cpp as a self-contained CMake subproject. Added after
# AMReX is found, so tim/cpp's guarded find_package(AMReX) is skipped and the
# AMReX already found by protoMOMxx is used. TIM_SOURCE_DIR defaults to the
# bundled extern/TIM submodule, but can be overridden to point at an existing
# TIM checkout.
if(NOT TARGET TIM::tim)
  set(TIM_SOURCE_DIR "${CMAKE_SOURCE_DIR}/extern/TIM" CACHE PATH
    "Path to the TIM checkout (defaults to the extern/TIM submodule).")
  if(NOT EXISTS "${TIM_SOURCE_DIR}/tim/cpp/CMakeLists.txt")
    message(FATAL_ERROR
      "TIM sources not found under '${TIM_SOURCE_DIR}/tim/cpp'.\n"
      "If you cloned protoMOMxx without submodules, initialize them:\n"
      "    git submodule update --init --recursive\n"
      "or point -DTIM_SOURCE_DIR=<path> at an existing TIM checkout.")
  endif()
  add_subdirectory("${TIM_SOURCE_DIR}/tim/cpp" "${CMAKE_BINARY_DIR}/extern/TIM/tim_cpp")
endif()

if(PROTOMOM_INSTALL)
  if(NOT PROTOMOM_MOM_BRIDGE)
    message(FATAL_ERROR "PROTOMOM_INSTALL=ON requires PROTOMOM_MOM_BRIDGE=ON.")
  endif()
  # The exported mom_bridge names TIM::tim in its link interface, so consumers
  # must be able to find the same TIM. A tim/cpp built in this tree is not
  # installed or exported.
  get_target_property(_protomom_tim_imported TIM::tim IMPORTED)
  if(NOT _protomom_tim_imported)
    message(FATAL_ERROR
      "PROTOMOM_INSTALL=ON requires an installed TIM (found with find_package), "
      "not the tim/cpp built from TIM_SOURCE_DIR. Point CMAKE_PREFIX_PATH or "
      "TIM_DIR at a TIM install.")
  endif()
endif()

add_subdirectory(src/mom_kernels)

if(NOT PROTOMOM_KERNELS_ONLY)
  add_subdirectory(src/framework)
  add_subdirectory(src/types)
  add_subdirectory(src/initialization)
  add_subdirectory(src/core)
  add_subdirectory(config_src/drivers/solo_driver)
  add_subdirectory(config_src)
endif()

if(PROTOMOM_ADD_TEST_TARGETS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Package config: find_package(protoMOMxx REQUIRED COMPONENTS MOM_BRIDGE)
# provides protoMOMxx::mom_bridge and, through it, protoMOMxx::mom_kernels.
if(PROTOMOM_INSTALL)
  include(CMakePackageConfigHelpers)
  set(PROTOMOM_CONFIG_INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/protoMOMxx)

  install(EXPORT protoMOMxxExports
    NAMESPACE protoMOMxx::
    FILE protoMOMxx-targets.cmake
    DESTINATION ${PROTOMOM_CONFIG_INSTALL_DESTINATION})

  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/protoMOMxxConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/protoMOMxxConfig.cmake
    INSTALL_DESTINATION ${PROTOMOM_CONFIG_INSTALL_DESTINATION})
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/protoMOMxxConfig.cmake
    DESTINATION ${PROTOMOM_CONFIG_INSTALL_DESTINATION})
endif()
