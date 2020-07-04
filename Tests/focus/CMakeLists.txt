SET( FocusTests_SRCS testfocus.cpp  )

ADD_EXECUTABLE( testfocus testfocus.cpp )
TARGET_LINK_LIBRARIES( testfocus ${TEST_LIBRARIES})
ADD_TEST( NAME FocusTest COMMAND testfocus )

