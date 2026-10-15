SET( FocusTests_SRCS testfocus.cpp testfocusstars.cpp )

ADD_EXECUTABLE( testfocus testfocus.cpp )
TARGET_LINK_LIBRARIES( testfocus ${TEST_LIBRARIES})
ADD_TEST( NAME FocusTest COMMAND testfocus )
SET_TESTS_PROPERTIES( FocusTest PROPERTIES LABELS "stable")

ADD_EXECUTABLE( testfocusstars testfocusstars.cpp )
TARGET_LINK_LIBRARIES( testfocusstars ${TEST_LIBRARIES})
ADD_TEST( NAME FocusStarsTest COMMAND testfocusstars )
SET_TESTS_PROPERTIES( FocusStarsTest PROPERTIES LABELS "stable")


ADD_EXECUTABLE( testfocusdevices testfocusdevices.cpp )
TARGET_LINK_LIBRARIES( testfocusdevices ${TEST_LIBRARIES})
ADD_TEST( NAME FocusDevicesTest COMMAND testfocusdevices )
SET_TESTS_PROPERTIES( FocusDevicesTest PROPERTIES LABELS "stable")
