IF (INDI_FOUND)
INCLUDE_DIRECTORIES(${INDI_INCLUDE_DIR})

ADD_EXECUTABLE( test_meridianflipstate test_meridianflipstate.cpp)
TARGET_LINK_LIBRARIES( test_meridianflipstate ${TEST_LIBRARIES})
ADD_TEST( NAME TestMeridianFlipState COMMAND test_meridianflipstate )
SET_TESTS_PROPERTIES( TestMeridianFlipState PROPERTIES LABELS "unstable" )

ENDIF ()
