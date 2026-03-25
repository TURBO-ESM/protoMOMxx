add_library(MOM_config_src_infra
  domain.cpp
)

target_include_directories(MOM_config_src_infra
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)