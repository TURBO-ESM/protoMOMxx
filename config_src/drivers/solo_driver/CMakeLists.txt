# CMake configuration for the solo driver

# The driver's time setup, in a library so that it can be unit tested.
add_library(MOM_config_src_solo_driver
  MOM_run_control.cpp
)

target_include_directories(MOM_config_src_solo_driver
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MOM_config_src_solo_driver
  PUBLIC
    MOM_src_framework
    MOM_config_src_infra
)

add_executable(protoMOMxx MOM_driver.cpp)
set_target_properties(protoMOMxx PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(protoMOMxx PRIVATE
    MOM_config_src_solo_driver
    MOM_src_framework
    MOM_src_core
    MOM_config_src_infra
)
add_amrex_to_protomom_target(protoMOMxx)
