include_directories(
    ${kstars_SOURCE_DIR}/kstars/tools
    ${kstars_SOURCE_DIR}/kstars/skyobjects
    ${kstars_SOURCE_DIR}/kstars/skycomponents
    ${kstars_SOURCE_DIR}/kstars/auxiliary
    ${kstars_SOURCE_DIR}/kstars/time
    ${kstars_SOURCE_DIR}/kstars/catalogsdb
    ${kstars_SOURCE_DIR}/datahandlers
    )

if(STELLARSOLVER_FOUND)
include_directories(${STELLARSOLVER_INCLUDE_DIR})
endif(STELLARSOLVER_FOUND)

if(WCSLIB_FOUND)
  include_directories( ${WCSLIB_INCLUDE_DIR} )
endif(WCSLIB_FOUND)

if (BUILD_QT5)
    FIND_PACKAGE(Qt5Test REQUIRED)
    SET( TEST_LIBRARIES
        Qt5::Core
        Qt5::Test
    )
    SET( TEST_KSLITE_LIBRARIES
        Qt5::Core
        Qt5::Test
    )
else(BUILD_QT5)
    FIND_PACKAGE(Qt6Test REQUIRED)
    SET( TEST_LIBRARIES
        Qt6::Core
        Qt6::Test
    )
    SET( TEST_KSLITE_LIBRARIES
        Qt6::Core
        Qt6::Test
    )
endif(BUILD_QT5)

SET( TEST_LIBRARIES
        ${TEST_LIBRARIES}
        LibKSDataHandlers
        htmesh
        ${ZLIB_LIBRARIES}
        KStarsLib
)

SET( TEST_KSLITE_LIBRARIES
        ${TEST_KSLITE_LIBRARIES}
        LibKSDataHandlers
        htmesh
        ${ZLIB_LIBRARIES}
        KStarsLiteLib
)

if(STELLARSOLVER_FOUND)
    SET( TEST_LIBRARIES
            ${TEST_LIBRARIES}
            ${STELLARSOLVER_LIBRARIES}
    )
endif(STELLARSOLVER_FOUND)

add_subdirectory(auxiliary)
add_subdirectory(tools)
add_subdirectory(skyobjects)

IF (CFITSIO_FOUND)
    add_subdirectory(fitsviewer)
IF (INDI_FOUND)
include_directories(
    ${INDI_INCLUDE_DIR}
    ${kstars_SOURCE_DIR}/kstars/ekos/align
    ${kstars_SOURCE_DIR}/kstars/internalguide
    ${kstars_SOURCE_DIR}/kstars/focus
    ${kstars_SOURCE_DIR}/kstars/scheduler
    )
add_subdirectory(scheduler)
add_subdirectory(focus)
add_subdirectory(polaralign)
add_subdirectory(ekos)
# FIXME
# Disable this test for Windows since it fails for now
if (NOT WIN32)
add_subdirectory(internalguide)
endif(NOT WIN32)
ENDIF(INDI_FOUND)
ENDIF(CFITSIO_FOUND)

IF (UNIX AND NOT APPLE AND CFITSIO_FOUND)
    IF (BUILD_KSTARS_LITE)
        add_subdirectory(kstars_lite_ui)
    ENDIF ()
    add_subdirectory(kstars_ui)
ENDIF ()

add_subdirectory(capture)
add_subdirectory(datahandlers)
