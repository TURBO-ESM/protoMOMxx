find_package(GTest QUIET)
if(NOT GTest_FOUND)
  if(PROTOMOM_FETCH_DEPS)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
  else()
    message(FATAL_ERROR
      "GTest not found. Install GoogleTest or set CMAKE_PREFIX_PATH "
      "to include its install location, or configure with "
      "-DPROTOMOM_FETCH_DEPS=ON to download it automatically.")
  endif()
endif()
enable_testing()

add_executable(test_MOM_logger MOM_logger/test_MOM_logger.cpp)
target_link_libraries(test_MOM_logger GTest::gtest_main MOM_src_framework)

include(GoogleTest)
gtest_discover_tests(test_MOM_logger)

add_subdirectory(config_src)