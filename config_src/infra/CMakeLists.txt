add_library(MOM_config_src_infra
  MOM_domain_infra.cpp
  MOM_infra.cpp
)

target_include_directories(MOM_config_src_infra
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MOM_config_src_infra
  PUBLIC
    TIM::tim
)
add_amrex_to_protomom_target(MOM_config_src_infra)
