cmake_minimum_required(VERSION 3.24)
project(protoMOMxx)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src/framework)
add_subdirectory(src/core)
add_subdirectory(config_src/drivers/solo_driver)

option(PROTOMOM_TESTS "Build unit tests" OFF)
option(PROTOMOM_FETCH_DEPS "Fetch missing dependencies automatically" ON)
if(PROTOMOM_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()