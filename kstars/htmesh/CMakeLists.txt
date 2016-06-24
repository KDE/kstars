INCLUDE(CheckFunctionExists)
check_function_exists(drand48 HAVE_DRAND48)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-htmesh.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-htmesh.h )

### HTMesh library
set(HTMesh_LIB_SRCS
    ${kstars_SOURCE_DIR}/kstars/htmesh/MeshIterator.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/HtmRange.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/HtmRangeIterator.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/RangeConvex.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/SkipList.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/SkipListElement.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/SpatialConstraint.cpp
#    ${kstars_SOURCE_DIR}/kstars/htmesh/SpatialDomain.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/SpatialEdge.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/SpatialException.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/SpatialIndex.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/SpatialVector.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/HTMesh.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/MeshBuffer.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/MeshIterator.cpp
    ${kstars_SOURCE_DIR}/kstars/htmesh/cc_aux.c
)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}")
#TODO is there KF5_ENABLE_EXCEPTIONS?
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")

add_library(htmesh STATIC ${HTMesh_LIB_SRCS})

add_definitions(-D_EXPORTING)

set_target_properties(htmesh PROPERTIES
    VERSION 1.0.0
    SOVERSION 1)
install(TARGETS htmesh ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )

# If you wish to compile the HTMesh perl wrapper, uncomment this, rebuild and copy the library into /usr/lib/, because we will use it as a shared object. See README in the perl wrapper directory (kstars/kstars/data/tools/HTMesh-*) for more details.
#set_property(TARGET htmesh PROPERTY POSITION_INDEPENDENT_CODE YES)

