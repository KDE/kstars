find_package(pybind11)
set(PYBIND11_CPP_STANDARD -std=c++11)

# Include KStars Headers
include_directories(
    ${kstars_SOURCE_DIR}/kstars/tools
    ${kstars_SOURCE_DIR}/kstars/skyobjects
    ${kstars_SOURCE_DIR}/kstars/skycomponents
    ${kstars_SOURCE_DIR}/kstars/auxiliary
    ${kstars_SOURCE_DIR}/kstars/time
    ${kstars_SOURCE_DIR}/kstars/catalogsdb
    )

find_package(Qt5 REQUIRED COMPONENTS Core Sql)
# Set libraries to link against
SET(PY_LIBRARIES
  htmesh
  KStarsLib
  Qt5::Sql
  Qt5::Core
  )

# remove --no-undefined because that does not make sense here
# gc-sections to remove bloat
string(REPLACE "-Wl,--no-undefined" "-Wl,--gc-sections" CMAKE_MODULE_LINKER_FLAGS ${CMAKE_MODULE_LINKER_FLAGS})
string(REPLACE "-fPIE " "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

pybind11_add_module(pykstars MODULE src/pykstars.cpp)
set_target_properties(pykstars PROPERTIES POSITION_INDEPENDENT_CODE OFF)
TARGET_LINK_LIBRARIES(pykstars PRIVATE ${PY_LIBRARIES})
add_custom_command(TARGET pykstars
                   POST_BUILD
                   COMMAND stubgen -o stubs -p pykstars
                   COMMAND find -name "*.pyi" | xargs -I {} bash -c 'echo QString = str >> {}')
