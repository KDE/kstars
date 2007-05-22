## SatLib Library ###
set(SatLib_LIB_SRCS 
	${CMAKE_SOURCE_DIR}/kstars/kstars/satlib/SatLib.c
)

add_definitions(-DUSE_KDEWIN32_LIB -DBUILD_DLL)

kde4_add_library(SatLib SHARED ${SatLib_LIB_SRCS})

if(NOT WIN32)
  target_link_libraries(SatLib m)
else(NOT WIN32)
  target_link_libraries(SatLib ${KDEWIN32_LIBRARIES})
endif(NOT WIN32)

set_target_properties(SatLib PROPERTIES VERSION 1.0.0 SOVERSION 1 )
install(TARGETS SatLib  DESTINATION ${LIB_INSTALL_DIR} )
