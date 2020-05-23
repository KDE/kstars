ADD_EXECUTABLE( test_polaralign test_polaralign.cpp )
TARGET_LINK_LIBRARIES( test_polaralign ${TEST_LIBRARIES})
ADD_TEST( NAME TestPolarAlign COMMAND test_polaralign )
