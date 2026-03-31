# Shared test infrastructure (header-only: kstars_test_macros.h, etc.)
# This must come first so all subdirectories can find the headers.
add_subdirectory(shared)

include_directories(
    ${kstars_SOURCE_DIR}/Tests/shared
    ${kstars_SOURCE_DIR}/kstars/tools
    ${kstars_SOURCE_DIR}/kstars/skyobjects
    ${kstars_SOURCE_DIR}/kstars/skycomponents
    ${kstars_SOURCE_DIR}/kstars/auxiliary
    ${kstars_SOURCE_DIR}/kstars/time
    ${kstars_SOURCE_DIR}/kstars/catalogsdb
    ${kstars_SOURCE_DIR}/datahandlers
    )

# Provide the source tree to the test helpers so they can locate and install runtime data files.
add_compile_definitions(KSTARS_TEST_SOURCEDIR="${kstars_SOURCE_DIR}")

if(StellarSolver_FOUND)
include_directories(${STELLARSOLVER_INCLUDE_DIR})
endif(StellarSolver_FOUND)

if(WCSLIB_FOUND)
  include_directories( ${WCSLIB_INCLUDE_DIR} )
endif(WCSLIB_FOUND)

FIND_PACKAGE(Qt6Test REQUIRED)
SET( TEST_LIBRARIES
    Qt6::Core
    Qt6::Test
)
SET( TEST_KSLITE_LIBRARIES
    Qt6::Core
    Qt6::Test
)

SET( TEST_LIBRARIES
        ${TEST_LIBRARIES}
        htmesh
        ${ZLIB_LIBRARIES}
        KStarsLib
)

SET( TEST_KSLITE_LIBRARIES
        ${TEST_KSLITE_LIBRARIES}
        htmesh
        ${ZLIB_LIBRARIES}
        KStarsLiteLib
)

if(StellarSolver_FOUND)
    SET( TEST_LIBRARIES
            ${TEST_LIBRARIES}
            StellarSolver::stellarsolver
    )
endif(StellarSolver_FOUND)

# testksuserdb (in the auxiliary subdir) transitively includes indicommon.h via
# profileinfo.h -> ksuserdb.h.  That header needs indiversion.h from libindi.
# The ${INDI_INCLUDE_DIR} guard must come *before* add_subdirectory(auxiliary)
# because CMake's include_directories() only applies to targets defined after the call.
IF (INDI_FOUND)
    include_directories(${INDI_INCLUDE_DIR})
ENDIF(INDI_FOUND)

add_subdirectory(auxiliary)
add_subdirectory(tools)
add_subdirectory(skyobjects)
add_subdirectory(skycomponents)
add_subdirectory(time)
add_subdirectory(projections)

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
# ekos/ is the parent for all Ekos module tests (scheduler, capture, focus,
# guide, align, mount, observatory, analyze, auxiliary).
add_subdirectory(ekos)
ENDIF(INDI_FOUND)
ENDIF(CFITSIO_FOUND)

IF (UNIX AND NOT APPLE AND CFITSIO_FOUND)
    IF (BUILD_KSTARS_LITE)
        add_subdirectory(kstars_lite_ui)
    ENDIF ()
    add_subdirectory(kstars_ui)
ENDIF ()

add_subdirectory(datahandlers)
