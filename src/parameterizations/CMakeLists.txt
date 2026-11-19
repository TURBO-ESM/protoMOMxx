# CMake configuration for src/parameterizations
add_library(MOM_src_parameterizations
  lateral/MOM_hor_visc.cpp
  vertical/MOM_set_viscosity.cpp
)

target_include_directories(MOM_src_parameterizations
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lateral
    ${CMAKE_CURRENT_SOURCE_DIR}/vertical
)

target_link_libraries(MOM_src_parameterizations
  PUBLIC
    MOM_src_initialization
    mom_kernels
)
add_amrex_to_protomom_target(MOM_src_parameterizations)
