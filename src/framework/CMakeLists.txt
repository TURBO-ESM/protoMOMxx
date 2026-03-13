# CMake configuration file for src/framework
add_library(MOM_src_framework
  MOM_logger.cpp
  MOM_logger.h
)

target_include_directories(MOM_src_framework
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)