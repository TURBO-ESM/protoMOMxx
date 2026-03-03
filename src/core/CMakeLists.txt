# CMake configuration for src/core
add_library(MOM_src_core
    MOM.cpp
)

target_include_directories(MOM_src_core PRIVATE ${PROJECT_SOURCE_DIR}/src/framework)
target_include_directories(MOM_src_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MOM_src_core PUBLIC MOM_src_framework)
