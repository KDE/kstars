include_directories(
    ${kstars_SOURCE_DIR}/kstars/tools
    ${kstars_SOURCE_DIR}/kstars/skyobjects
    ${kstars_SOURCE_DIR}/kstars/skycomponents
    ${kstars_SOURCE_DIR}/kstars/auxiliary
    ${kstars_SOURCE_DIR}/kstars/time
    )

if(STELLARSOLVER_FOUND)
include_directories(${STELLARSOLVER_INCLUDE_DIR})
endif(STELLARSOLVER_FOUND)

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

IF (CFITSIO_FOUND)
    add_subdirectory(fitsviewer)
IF (INDI_FOUND)
include_directories(
    ${kstars_SOURCE_DIR}/kstars/ekos/align
    ${kstars_SOURCE_DIR}/kstars/internalguide
    ${kstars_SOURCE_DIR}/kstars/focus
    ${kstars_SOURCE_DIR}/kstars/scheduler
    )
add_subdirectory(scheduler)
add_subdirectory(focus)
add_subdirectory(polaralign)
# FIXME
# Disable this test for Windows since it fails for now
if (NOT WIN32)
add_subdirectory(internalguide)
endif(NOT WIN32)
ENDIF(INDI_FOUND)
ENDIF(CFITSIO_FOUND)

if(STELLARSOLVER_FOUND)
    include_directories(${STELLARSOLVER_INCLUDE_DIR})
endif(STELLARSOLVER_FOUND)

IF (UNIX AND NOT APPLE AND CFITSIO_FOUND)
    IF (BUILD_KSTARS_LITE)
        add_subdirectory(kstars_lite_ui)
    ENDIF ()
    add_subdirectory(kstars_ui)
ENDIF ()

add_subdirectory(capture)
