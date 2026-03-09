# CMake configuration for the solo driver
add_executable(protoMOMxx MOM_driver.cpp)

target_link_libraries(protoMOMxx
    MOM_src_framework
    MOM_src_core
)
