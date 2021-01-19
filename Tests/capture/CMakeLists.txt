ADD_EXECUTABLE( test_placeholderpath test_placeholderpath.cpp )
TARGET_LINK_LIBRARIES( test_placeholderpath ${TEST_LIBRARIES})
ADD_TEST( NAME TestPlaceholderPath COMMAND test_placeholderpath )
