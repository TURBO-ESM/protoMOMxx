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
add_executable(test_MOM_nml_parser MOM_nml_parser/test_MOM_nml_parser.cpp)
add_executable(test_MOM_io MOM_io/test_MOM_io.cpp)
add_executable(test_MOM_directories MOM_directories/test_MOM_directories.cpp)
add_executable(test_MOM_file_parser MOM_file_parser/test_MOM_file_parser.cpp)
add_executable(test_MOM_document MOM_document/test_MOM_document.cpp)
target_link_libraries(test_MOM_logger GTest::gtest_main MOM_src_framework)
target_link_libraries(test_MOM_nml_parser GTest::gtest_main MOM_src_framework)
target_link_libraries(test_MOM_io GTest::gtest_main MOM_src_framework)
target_link_libraries(test_MOM_directories GTest::gtest_main MOM_src_framework)
target_link_libraries(test_MOM_file_parser GTest::gtest_main MOM_src_framework)
target_link_libraries(test_MOM_document GTest::gtest_main MOM_src_framework)

include(GoogleTest)
gtest_discover_tests(test_MOM_logger)
gtest_discover_tests(test_MOM_nml_parser)
gtest_discover_tests(test_MOM_io)
gtest_discover_tests(test_MOM_directories)
gtest_discover_tests(test_MOM_file_parser)
gtest_discover_tests(test_MOM_document)

add_subdirectory(config_src)

# System/regression test: run the full protoMOMxx executable against the double_gyre example
# and confirm its MOM_parameters_doc.* output matches the golden copies committed under
# double_gyre/. Both tests are labeled "system" so they can be excluded from the default test
# run (`ctest -LE system`) while still being runnable on demand (`ctest -L system`).
set(DOUBLE_GYRE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/double_gyre)
set(DOUBLE_GYRE_RUN_DIR ${CMAKE_CURRENT_BINARY_DIR}/double_gyre)
file(COPY ${DOUBLE_GYRE_SOURCE_DIR}/ DESTINATION ${DOUBLE_GYRE_RUN_DIR}
     PATTERN "MOM_parameters_doc.*" EXCLUDE)

add_test(NAME double_gyre_smoke
         COMMAND $<TARGET_FILE:protoMOMxx>
         WORKING_DIRECTORY ${DOUBLE_GYRE_RUN_DIR})
set_tests_properties(double_gyre_smoke PROPERTIES LABELS system)

add_test(NAME double_gyre_regression
         COMMAND ${CMAKE_COMMAND}
                 -DEXPECTED_DIR=${DOUBLE_GYRE_SOURCE_DIR}
                 -DACTUAL_DIR=${DOUBLE_GYRE_RUN_DIR}
                 -P ${DOUBLE_GYRE_SOURCE_DIR}/check_regression.cmake)
set_tests_properties(double_gyre_regression PROPERTIES
  LABELS system
  DEPENDS double_gyre_smoke)