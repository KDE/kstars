include_directories( ${kstars_SOURCE_DIR} )
# QT4_AUTOMOC( ${kstars_SRCS} )
# add_library( KStarsAll SHARED ${kstars_extra_SRCS})


FIND_PACKAGE(Qt4 REQUIRED)
SET( TEST_LIBRARIES ${QT_QTTEST_LIBRARY} ${QT_LIBRARIES}
      LibKSDataHandlers
    ${KDE4_KDECORE_LIBS}
        ${KDE4_KNEWSTUFF3_LIBS}
        ${KDE4_KIO_LIBS}
        htmesh
        ${ZLIB_LIBRARIES}
        ${QT_QTSQL_LIBRARY}
          KStarsLib
)

SET( KSParserTests_SRCS testksparser.cpp )
ADD_EXECUTABLE( testksparser ${KSParserTests_SRCS} )
QT4_AUTOMOC( ${KSParserTests_SRCS} )
TARGET_LINK_LIBRARIES( testksparser ${TEST_LIBRARIES} ${QT_QTTEST_LIBRARY})
ADD_TEST( NAME KSParserTest COMMAND testksparser )