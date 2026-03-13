# CMake configuration for the solo driver
add_executable(protoMOMxx MOM_driver.cpp)
set_target_properties(protoMOMxx PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(protoMOMxx
    MOM_src_framework
    MOM_src_core
)
