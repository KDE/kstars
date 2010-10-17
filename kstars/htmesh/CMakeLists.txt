INCLUDE(CheckFunctionExists)
check_function_exists(drand48 HAVE_DRAND48)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-htmesh.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-htmesh.h )

### HTMesh library
set(HTMesh_LIB_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/HtmRange.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/HtmRangeIterator.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/RangeConvex.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/SkipList.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/SkipListElement.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/SpatialConstraint.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/SpatialEdge.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/SpatialException.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/SpatialIndex.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/SpatialVector.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/HTMesh.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/MeshBuffer.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh/MeshIterator.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}")

kde4_add_library(htmesh STATIC ${HTMesh_LIB_SRCS})

add_definitions(-D_EXPORTING)

set_target_properties(htmesh PROPERTIES
    VERSION 1.0.0
    SOVERSION 1)
install(TARGETS htmesh ${INSTALL_TARGETS_DEFAULT_ARGS} )
