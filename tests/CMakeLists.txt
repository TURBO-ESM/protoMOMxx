# CMake configuration for protoMOMxx unit tests
cmake_minimum_required(VERSION 3.10)
project(protoMOMxx_tests)

# Enable testing
include(CTest)
enable_testing()

# Optionally, download and add GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


# Add test source files in subdirectories
file(GLOB MOM_FILE_PARSER_TESTS "MOM_file_parser/*.cpp")

add_executable(test_MOM_file_parser ${MOM_FILE_PARSER_TESTS})
target_link_libraries(test_MOM_file_parser gtest_main protoMOMxx_framework)

# Add include directories if needed
# target_include_directories(test_MOM_file_parser PRIVATE ../src/framework)

include(GoogleTest)
gtest_discover_tests(test_MOM_file_parser)
