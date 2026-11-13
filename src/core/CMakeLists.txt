# CMake configuration for src/core
add_library(MOM_src_core
  MOM.cpp
  MOM_continuity.cpp
  MOM_dynamics_unsplit_RK2.cpp
)

target_include_directories(MOM_src_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MOM_src_core
  PUBLIC
    MOM_src_initialization
    mom_kernels
)
add_amrex_to_protomom_target(MOM_src_core)
