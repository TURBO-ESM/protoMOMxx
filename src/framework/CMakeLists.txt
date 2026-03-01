# CMake configuration for src/framework
add_library(MOM_src_framework
    MOM_file_parser.cpp
    MOM_nml_parser.cpp
    MOM_parser_utilities.cpp
    MOM_get_input.cpp
    MOM_string_functions.h
)

target_include_directories(MOM_src_framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
