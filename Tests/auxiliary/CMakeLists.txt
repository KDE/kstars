SET( KSParserTests_SRCS testcsvparser.cpp testfwparser.cpp )

ADD_EXECUTABLE( testcsvparser testcsvparser.cpp )
TARGET_LINK_LIBRARIES( testcsvparser ${TEST_LIBRARIES})
ADD_TEST( NAME CSVParserTest COMMAND testcsvparser )

ADD_EXECUTABLE( testfwparser testfwparser.cpp )
TARGET_LINK_LIBRARIES( testfwparser ${TEST_LIBRARIES})
ADD_TEST( NAME FixedWidthParserTest COMMAND testfwparser )

ADD_EXECUTABLE( testdms testdms.cpp )
TARGET_LINK_LIBRARIES( testdms ${TEST_LIBRARIES})
ADD_TEST( NAME DMSTest COMMAND testdms )
