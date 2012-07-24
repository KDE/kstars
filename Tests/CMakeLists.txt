include_directories( ${kstars_SRCS} )
FIND_PACKAGE(Qt4 REQUIRED)
SET( TEST_LIBRARIES ${QT_QTTEST_LIBRARY} ${QT_LIBRARIES} )

SET( KSParserTests_SRCS testksparser.cpp )
QT4_AUTOMOC( ${KSParserTests_SRCS} )
ADD_EXECUTABLE( testksparser ${KSParserTests_SRCS} )
TARGET_LINK_LIBRARIES( testksparser ${TEST_LIBRARIES} ${QT_QTTEST_LIBRARY} )
ADD_TEST( NAME KSParserTest COMMAND testksparser )