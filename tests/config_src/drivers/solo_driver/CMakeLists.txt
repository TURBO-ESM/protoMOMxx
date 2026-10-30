add_executable(test_run_control test_run_control.cpp)
target_link_libraries(test_run_control GTest::gtest_main MOM_config_src_solo_driver)

add_executable(test_surface_forcing test_surface_forcing.cpp)
target_link_libraries(test_surface_forcing GTest::gtest MOM_config_src_solo_driver MOM_config_src_infra)

include(GoogleTest)
gtest_discover_tests(test_run_control PROPERTIES LABELS "unit")
gtest_discover_tests(test_surface_forcing PROPERTIES LABELS "unit")

add_test(NAME test_surface_forcing_mpi
         COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 2
                 ${MPIEXEC_PREFLAGS} $<TARGET_FILE:test_surface_forcing> ${MPIEXEC_POSTFLAGS})
set_tests_properties(test_surface_forcing_mpi PROPERTIES LABELS "mpi" PROCESSORS 2)
