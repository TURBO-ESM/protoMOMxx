cmake_minimum_required(VERSION 3.10)
project(protoMOMxx)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(src/framework)
add_subdirectory(src/core)
add_subdirectory(config_src/drivers/solo_driver)