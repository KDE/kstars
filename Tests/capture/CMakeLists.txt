IF (INDI_FOUND)
INCLUDE_DIRECTORIES(${INDI_INCLUDE_DIR})

if (NOT WIN32)
ADD_EXECUTABLE( test_placeholderpath test_placeholderpath.cpp test_placeholderpath.qrc)
TARGET_LINK_LIBRARIES( test_placeholderpath ${TEST_LIBRARIES})
ADD_TEST( NAME TestPlaceholderPath COMMAND test_placeholderpath )
SET_TESTS_PROPERTIES( TestPlaceholderPath PROPERTIES LABELS "stable" )
endif()

ENDIF ()
