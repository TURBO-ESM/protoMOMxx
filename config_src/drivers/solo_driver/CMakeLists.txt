# CMake configuration for the main MOM6 driver
add_library(MOM_driver
    MOM_driver.cpp
)

target_include_directories(MOM_driver PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_link_libraries(MOM_driver PUBLIC MOM_src_core)