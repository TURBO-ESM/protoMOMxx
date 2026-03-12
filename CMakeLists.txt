cmake_minimum_required(VERSION 3.10)
project(protoMOMxx)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(src/framework)
add_subdirectory(src/core)
add_subdirectory(config_src/drivers/solo_driver)

option(PROTOMOM_TESTS "Build unit tests" OFF)
if(PROTOMOM_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()