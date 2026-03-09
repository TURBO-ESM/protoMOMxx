project(protoMOMxx_tests)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(googletest)
endif()
enable_testing()

file(GLOB MOM_LOGGER_TESTS "MOM_logger/*.cpp")

add_executable(test_MOM_logger ${MOM_LOGGER_TESTS})
target_link_libraries(test_MOM_logger GTest::gtest_main MOM_src_framework)

include(GoogleTest)
gtest_discover_tests(test_MOM_logger)
