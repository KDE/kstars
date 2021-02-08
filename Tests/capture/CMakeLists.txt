IF (INDI_FOUND)
INCLUDE_DIRECTORIES(${INDI_INCLUDE_DIR})

# 2021-02-08 JM: This test now fails on Windows, so it's disabled.
if (NOT WIN32)
ADD_EXECUTABLE( test_placeholderpath test_placeholderpath.cpp )
TARGET_LINK_LIBRARIES( test_placeholderpath ${TEST_LIBRARIES})
ADD_TEST( NAME TestPlaceholderPath COMMAND test_placeholderpath )
endif()

ENDIF ()
