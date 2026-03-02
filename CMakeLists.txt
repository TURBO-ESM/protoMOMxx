cmake_minimum_required(VERSION 3.10)
project(protoMOMxx)

add_subdirectory(src/framework)
add_subdirectory(src/core)
add_subdirectory(config_src/drivers/solo_driver)
add_subdirectory(tests)

add_executable(protoMOMxx config_src/drivers/solo_driver/MOM_driver.cpp)

target_link_libraries(protoMOMxx
    MOM_src_framework
    MOM_src_core
    MOM_driver
)