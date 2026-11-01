IF (INDI_FOUND)
INCLUDE_DIRECTORIES(${INDI_INCLUDE_DIR})

ADD_EXECUTABLE( test_ekos_fieldderotation testfieldderotation.cpp )
TARGET_LINK_LIBRARIES( test_ekos_fieldderotation ${TEST_LIBRARIES})
ADD_TEST( NAME FieldDerotationTest COMMAND test_ekos_fieldderotation )
SET_TESTS_PROPERTIES( FieldDerotationTest PROPERTIES LABELS "stable")
ENDIF ()
