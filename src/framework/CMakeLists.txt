# CMake configuration for src/framework
add_library(protoMOMxx_framework
    MOM_file_parser.cpp
    MOM_string_functions.h
)

target_include_directories(protoMOMxx_framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
