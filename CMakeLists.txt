cmake_minimum_required(VERSION 3.24)
project(protoMOMxx)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PROTOMOM_CUDA "Build with Cuda backend" OFF)
option(PROTOMOM_TESTS "Build unit tests" OFF)
option(PROTOMOM_FETCH_DEPS "Fetch missing dependencies automatically" ON)

if(PROTOMOM_CUDA)
  enable_language(CUDA)
  find_package(AMReX REQUIRED CUDA)
  include(AMReXTargetHelpers)
else()
  find_package(AMReX REQUIRED)
endif()

include(cmake/protomomtarget.cmake)

add_subdirectory(src/framework)
add_subdirectory(src/core)
add_subdirectory(config_src/drivers/solo_driver)

if(PROTOMOM_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()