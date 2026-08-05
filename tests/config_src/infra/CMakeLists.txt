add_executable(test_domain test_domain.cpp)
target_link_libraries(test_domain GTest::gtest MOM_config_src_infra)
add_executable(test_infra test_infra.cpp)
target_link_libraries(test_infra GTest::gtest MOM_config_src_infra)

include(GoogleTest)
gtest_discover_tests(test_domain PROPERTIES LABELS "unit")
gtest_discover_tests(test_infra PROPERTIES LABELS "unit")

find_package(MPI REQUIRED COMPONENTS CXX)  # for MPIEXEC_*
add_test(NAME test_infra_mpi
         COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 2
                 ${MPIEXEC_PREFLAGS} $<TARGET_FILE:test_infra> ${MPIEXEC_POSTFLAGS})
set_tests_properties(test_infra_mpi PROPERTIES LABELS "mpi" PROCESSORS 2)
