## SatLib Library ###
set(SatLib_LIB_SRCS 
	${CMAKE_SOURCE_DIR}/kstars/kstars/satlib/SatLib.c
)

kde4_add_library(SatLib SHARED ${SatLib_LIB_SRCS})

target_link_libraries(SatLib m)

set_target_properties(SatLib PROPERTIES VERSION 1.0.0 SOVERSION 1 )
install(TARGETS SatLib  DESTINATION ${LIB_INSTALL_DIR} )
