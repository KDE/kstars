## SatLib Library ###
set(SatLib_LIB_SRCS 
	${kstars_SOURCE_DIR}/kstars/satlib/SatLib.c
)

add_definitions(-DUSE_KDEWIN32_LIB -DBUILD_DLL)

kde4_add_library(SatLib SHARED ${SatLib_LIB_SRCS})

if(NOT WIN32)
  target_link_libraries(SatLib m)
else(NOT WIN32)
  target_link_libraries(SatLib ${KDEWIN32_LIBRARIES})
endif(NOT WIN32)

set_target_properties(SatLib PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION} )
install(TARGETS SatLib  ${INSTALL_TARGETS_DEFAULT_ARGS})
