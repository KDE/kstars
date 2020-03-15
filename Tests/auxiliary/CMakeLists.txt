SET( KSParserTests_SRCS testcsvparser.cpp testfwparser.cpp )

#ADD_EXECUTABLE( auxiliary_tests
#    auxiliary_tests.cpp
#    testcsvparser.cpp
#    testfwparser.cpp
#    testdms.cpp
#    testcachingdms.cpp
#    testcolorscheme.cpp
#    testbinhelper.cpp
#    testfov.cpp
#    testgeolocation.cpp
#    testksuserdb.cpp)
#TARGET_LINK_LIBRARIES( auxiliary_tests ${TEST_LIBRARIES})
#ADD_TEST( NAME AuxiliaryTests COMMAND auxiliary_tests )

ADD_EXECUTABLE( testcsvparser testcsvparser.cpp )
TARGET_LINK_LIBRARIES( testcsvparser ${TEST_LIBRARIES})
ADD_TEST( NAME CSVParserTest COMMAND testcsvparser )

ADD_EXECUTABLE( testfwparser testfwparser.cpp )
TARGET_LINK_LIBRARIES( testfwparser ${TEST_LIBRARIES})
ADD_TEST( NAME FixedWidthParserTest COMMAND testfwparser )

ADD_EXECUTABLE( testdms testdms.cpp )
TARGET_LINK_LIBRARIES( testdms ${TEST_LIBRARIES})
ADD_TEST( NAME DMSTest COMMAND testdms )

ADD_EXECUTABLE( testcachingdms testcachingdms.cpp )
TARGET_LINK_LIBRARIES( testcachingdms ${TEST_LIBRARIES})
ADD_TEST( NAME TestCachingDms COMMAND testcachingdms )

ADD_EXECUTABLE( testcolorscheme testcolorscheme.cpp )
TARGET_LINK_LIBRARIES( testcolorscheme ${TEST_LIBRARIES})
ADD_TEST( NAME TestColorscheme COMMAND testcolorscheme )

ADD_EXECUTABLE( testbinhelper testbinhelper.cpp )
TARGET_LINK_LIBRARIES( testbinhelper ${TEST_LIBRARIES})
ADD_TEST( NAME TestBinHelper COMMAND testbinhelper )

ADD_EXECUTABLE( testfov testfov.cpp )
TARGET_LINK_LIBRARIES( testfov ${TEST_LIBRARIES})
ADD_TEST( NAME TestFOV COMMAND testfov )

ADD_EXECUTABLE( testgeolocation testgeolocation.cpp )
TARGET_LINK_LIBRARIES( testgeolocation ${TEST_LIBRARIES})
ADD_TEST( NAME TestGeolocation COMMAND testgeolocation )

ADD_EXECUTABLE( testksuserdb testksuserdb.cpp )
TARGET_LINK_LIBRARIES( testksuserdb ${TEST_LIBRARIES})
ADD_TEST( NAME TestKSUserDB COMMAND testksuserdb )

