include_directories(
    ${kstars_SOURCE_DIR}/kstars/tools
    ${kstars_SOURCE_DIR}/kstars/skyobjects
    ${kstars_SOURCE_DIR}/kstars/skycomponents
    ${kstars_SOURCE_DIR}/kstars/auxiliary
    ${kstars_SOURCE_DIR}/kstars/time
    ${kstars_SOURCE_DIR}/kstars/ekos/align
    )

if(WCSLIB_FOUND)
  include_directories( ${WCSLIB_INCLUDE_DIR} )
endif(WCSLIB_FOUND)

FIND_PACKAGE(Qt5Test REQUIRED)

SET( TEST_LIBRARIES
        Qt5::Core
        Qt5::Test
        LibKSDataHandlers
        htmesh
        ${ZLIB_LIBRARIES}
        KStarsLib
)

SET( TEST_KSLITE_LIBRARIES
        Qt5::Core
        Qt5::Test
        LibKSDataHandlers
        htmesh
        ${ZLIB_LIBRARIES}
        KStarsLiteLib
)

add_subdirectory(auxiliary)
add_subdirectory(skyobjects)
add_subdirectory(polaralign)

IF (CFITSIO_FOUND)
    add_subdirectory(fitsviewer)
ENDIF ()

IF (UNIX AND NOT APPLE AND CFITSIO_FOUND)
    IF (BUILD_KSTARS_LITE)
        add_subdirectory(kstars_lite_ui)
    ENDIF ()
    add_subdirectory(kstars_ui)
ENDIF ()
