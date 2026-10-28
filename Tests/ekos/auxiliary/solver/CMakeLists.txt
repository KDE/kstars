ADD_EXECUTABLE( test_ekos_solverprofiles testsolverprofiles.cpp )
TARGET_LINK_LIBRARIES( test_ekos_solverprofiles ${TEST_LIBRARIES})
ADD_TEST( NAME SolverProfilesTest COMMAND test_ekos_solverprofiles WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
SET_TESTS_PROPERTIES( SolverProfilesTest PROPERTIES LABELS "stable")
