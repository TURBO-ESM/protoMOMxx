add_executable(test_domain test_domain.cpp)
target_link_libraries(test_domain GTest::gtest_main MOM_config_src_infra)
add_executable(test_infra test_infra.cpp)
target_link_libraries(test_infra GTest::gtest MOM_config_src_infra)

include(GoogleTest)
gtest_discover_tests(test_domain PROPERTIES LABELS "unit")
gtest_discover_tests(test_infra PROPERTIES LABELS "unit")
