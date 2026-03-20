# CMake configuration file for src/framework
add_library(MOM_src_framework
  MOM_logger.cpp
  MOM_parser_utils.cpp
  MOM_directories.cpp
  MOM_nml_parser.cpp
  MOM_file_parser.cpp
  MOM_document.cpp
)

target_include_directories(MOM_src_framework
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)