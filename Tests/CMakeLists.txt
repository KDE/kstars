include_directories(
    ${kstars_SOURCE_DIR}/kstars/tools
    ${kstars_SOURCE_DIR}/kstars/skyobjects
    ${kstars_SOURCE_DIR}/kstars/skycomponents
    ${kstars_SOURCE_DIR}/kstars/auxiliary
    ${kstars_SOURCE_DIR}/kstars/time
    )

#include_directories( ${kstars_SOURCE_DIR} )

FIND_PACKAGE(Qt5Test REQUIRED)

SET( TEST_LIBRARIES 
        Qt5::Core
        Qt5::Test
        LibKSDataHandlers
        htmesh
        ${ZLIB_LIBRARIES}
        KStarsLib
)

add_subdirectory(auxiliary)

