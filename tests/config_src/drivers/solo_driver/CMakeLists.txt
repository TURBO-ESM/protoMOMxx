add_executable(test_run_control test_run_control.cpp)
target_link_libraries(test_run_control GTest::gtest_main MOM_config_src_solo_driver)

include(GoogleTest)
gtest_discover_tests(test_run_control PROPERTIES LABELS "unit")
