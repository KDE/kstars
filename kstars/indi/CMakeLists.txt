KDE4_NO_ENABLE_FINAL(indi)

include_directories( ${CMAKE_CURRENT_BINARY_DIR}/../.. )
include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs)
include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee)
include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli)
include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/webcam)

if (CFITSIO_FOUND)
  include_directories(${CFITSIO_INCLUDE_DIR})
endif (CFITSIO_FOUND)

option(ssh_tunnel "INDI Server binds locally. Remote clients may only connect via SSH Tunneling." OFF)

if (ssh_tunnel)
   add_definition(-DSSH_TUNNEL)
endif(ssh_tunnel)

set(liblilxml_SRCS  ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/lilxml.c )

set(libindicom_SRCS 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/indicom.c 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/base64.c 
	)

set(libapogee_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD3011HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD3011LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4240HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4240LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710LS2.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710LS3.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710LS4.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710LS5.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4720HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4720LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5520HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5520LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD7700HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD7700LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF0261E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF0401E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1001E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1301E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1401E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF3200E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF4202.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1602E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF16801E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF6303E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_TH7899.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamTable.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamera.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamera_USB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamera_Linux.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApogeeUsbLinux.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnGlue.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF09000.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020CLB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020CLB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020ML.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020MLB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4720B.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020MLB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000ML.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnGlue.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF0261EB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000C.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2001M.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020CL.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000M.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD3011B.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2001ML.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4240B.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5520.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD7700.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF10011105.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020C.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020M.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000C.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4240.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/usb_apple.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5520B.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710B.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710B.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF10011105.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4240B.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5520.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710F.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF3200EB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF6303EB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020CLB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2001CL.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD3011.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020MLB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4720B.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF3200EB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2001CL.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020CL.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD3011.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF16803.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD7700B.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020C.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020C.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1602EB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020M.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020M.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020ML.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1001ENS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4720.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710B.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF0401EB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD7700B.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1001EB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710F.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020C.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000ML.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2001M.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/usb.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1602EB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020CL.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020M.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2001ML.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD3011B.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020ML.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000CL.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4720.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000M.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1001EB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710ALT.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD7700.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1301EB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1001ENS.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF0401EB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020CLB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020CL.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF09000.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF16803.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020MLB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710ALT.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020ML.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4240.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000CL.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5520B.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710B.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF0261EB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1301EB.h
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF6303EB.cpp
	)

set(libfli_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-camera.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-camera-parport.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-camera-usb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-filter-focuser.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-mem.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-serial.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-sys.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-usb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-debug.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-usb-sys-linux.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli/libfli-parport.c
	)

set(libwebcam_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/webcam/PPort.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/webcam/port.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/webcam/v4l2_base.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/webcam/ccvt_c2.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/webcam/ccvt_misc.c
	)

set (indimain_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/indidrivermain.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/eventloop.c
    )

set(GENERIC_LIB_VERSION "1.0.0")

########### next target ###############
## SBIG Universal Driver Dummy Library ###
if (NOT SBIG_FOUND)
set(sbigudrv_LIB_SRCS
		${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/ccd/sbig_dummy.cpp
)

kde4_add_library(sbigudrv SHARED ${sbigudrv_LIB_SRCS})

target_link_libraries(sbigudrv)

set_target_properties(sbigudrv PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION} )
install(TARGETS sbigudrv  DESTINATION ${LIB_INSTALL_DIR} )

# is this right to do?
set(SBIG_LIBRARIES "sbigudrv")

endif(NOT SBIG_FOUND)
if (NOT WIN32)

######################################
########### INDI SERVER ##############
######################################

set(indiserver_SRCS indiserver.c fq.c)

kde4_add_executable(indiserver NOGUI ${indiserver_SRCS}  ${liblilxml_SRCS})

target_link_libraries(indiserver  ${KDE4_KDECORE_LIBS} pthread )

install(TARGETS indiserver  DESTINATION ${BIN_INSTALL_DIR} )

#####################################
########## TELESCOPE GROUP ##########
#####################################

########### LX200 Basic #############
set(lx200basic_SRCS 
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200driver.c 
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200basic.cpp )

kde4_add_executable(indi_lx200basic NOGUI ${lx200basic_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_lx200basic  ${KDE4_KDECORE_LIBS} )

install(TARGETS indi_lx200basic DESTINATION ${BIN_INSTALL_DIR} )

#################################################################################

########### LX200 Generic ###########
set(lx200generic_SRCS 
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200driver.c 
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200autostar.cpp 
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200_16.cpp 
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200gps.cpp 
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200generic.cpp 
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200classic.cpp )

kde4_add_executable(indi_lx200generic NOGUI ${lx200generic_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_lx200generic  ${KDE4_KDECORE_LIBS} m )

install(TARGETS indi_lx200generic DESTINATION ${BIN_INSTALL_DIR} )

ADD_CUSTOM_COMMAND(TARGET indi_lx200generic POST_BUILD COMMAND rm ARGS -f ${BIN_INSTALL_DIR}/indi_lx200classic)
ADD_CUSTOM_COMMAND(TARGET indi_lx200generic POST_BUILD COMMAND ln ARGS -s indi_lx200generic ${BIN_INSTALL_DIR}/indi_lx200classic)

ADD_CUSTOM_COMMAND(TARGET indi_lx200generic POST_BUILD COMMAND rm ARGS -f ${BIN_INSTALL_DIR}/indi_lx200autostar)
ADD_CUSTOM_COMMAND(TARGET indi_lx200generic POST_BUILD COMMAND ln ARGS -s indi_lx200generic ${BIN_INSTALL_DIR}/indi_lx200autostar)

ADD_CUSTOM_COMMAND(TARGET indi_lx200generic POST_BUILD COMMAND rm ARGS -f ${BIN_INSTALL_DIR}/indi_lx200_16)
ADD_CUSTOM_COMMAND(TARGET indi_lx200generic POST_BUILD COMMAND ln ARGS -s indi_lx200generic ${BIN_INSTALL_DIR}/indi_lx200_16)

ADD_CUSTOM_COMMAND(TARGET indi_lx200generic POST_BUILD COMMAND rm ARGS -f ${BIN_INSTALL_DIR}/indi_lx200gps)
ADD_CUSTOM_COMMAND(TARGET indi_lx200generic POST_BUILD COMMAND ln ARGS -s indi_lx200generic ${BIN_INSTALL_DIR}/indi_lx200gps)

#################################################################################

########### Celestron GPS ############
set(celestrongps_SRCS 
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/celestronprotocol.c 
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/celestrongps.cpp )

kde4_add_executable(indi_celestron_gps NOGUI ${celestrongps_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_celestron_gps ${KDE4_KDECORE_LIBS} m )

install(TARGETS indi_celestron_gps DESTINATION ${BIN_INSTALL_DIR} )

#################################################################################

########### Orion Atlas #############
set(orionatlas_SRCS 
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/orionatlas.cpp )

kde4_add_executable(indi_orion_atlas NOGUI ${orionatlas_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_orion_atlas ${KDE4_KDECORE_LIBS} )

install(TARGETS indi_orion_atlas DESTINATION ${BIN_INSTALL_DIR} )

#################################################################################

########### Takahashi Temma ##########
set(temma_SRCS 
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/temmadriver.c )

kde4_add_executable(indi_temma NOGUI ${temma_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_temma  ${KDE4_KDECORE_LIBS} m )

install(TARGETS indi_temma DESTINATION ${BIN_INSTALL_DIR} )

#################################################################################

########### Sky Commander #############
set(skycommander_SRCS 
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200driver.c 
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/skycommander.c )

kde4_add_executable(indi_skycommander ${skycommander_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_skycommander  ${KDE4_KDECORE_LIBS} m )

install(TARGETS indi_skycommander  DESTINATION ${BIN_INSTALL_DIR} )

#################################################################################

########### Intelliscope ###############
set(intelliscope_SRCS 
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200driver.c 
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/intelliscope.c )

kde4_add_executable(indi_intelliscope ${intelliscope_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_intelliscope  ${KDE4_KDECORE_LIBS} m )

install(TARGETS indi_intelliscope DESTINATION ${BIN_INSTALL_DIR} )

#################################################################################

########### LINUX SPECIFC ###############
#### Build the following targets on Linux Only ####

IF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

ADD_DEFINITIONS(-DLINUX)

#####################################
########## FOCUSER GROUP ############
#####################################

###### FLI Precision Digital Focuser ######

set(flipdf_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/focuser/fli_pdf.c
)

kde4_add_executable(indi_fli_pdf ${flipdf_SRCS} ${libfli_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_fli_pdf m ${KDE4_KDECORE_LIBS} )

install(TARGETS indi_fli_pdf DESTINATION ${BIN_INSTALL_DIR} )

#################################################################################

################ Robo Focuser ################

set(robofocus_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/focuser/robofocus.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/focuser/robofocusdriver.c
   )

kde4_add_executable(indi_robofocus ${robofocus_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_robofocus ${KDE4_KDECORE_LIBS} )

install(TARGETS indi_robofocus DESTINATION ${BIN_INSTALL_DIR} )

#################################################################################

#####################################
######## FILTER WHEEL GROUP #########
#####################################

########## True Technology Wheel ############
set(trutechwheel_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/filter_wheel/trutech_wheel.c
   )

kde4_add_executable(indi_trutech_wheel ${trutechwheel_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_trutech_wheel  ${KDE4_KDECORE_LIBS} )

install(TARGETS indi_trutech_wheel DESTINATION ${BIN_INSTALL_DIR} )

#################################################################################

########## FLI Filter Wheel ############
set(fliwheel_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/filter_wheel/fli_wheel.c
   )

kde4_add_executable(indi_fli_wheel ${fliwheel_SRCS}  ${libfli_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_fli_wheel ${KDE4_KDECORE_LIBS} )

install(TARGETS indi_fli_wheel DESTINATION ${BIN_INSTALL_DIR} )
#################################################################################

#######################################
############# CCD GROUP ###############
#######################################

############# FLI CCD ###############
if (CFITSIO_FOUND)

set(fliccd_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/ccd/fli_ccd.c
)

kde4_add_executable(indi_fli_ccd ${fliccd_SRCS} ${libfli_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_fli_ccd ${KDE4_KDECORE_LIBS} ${CFITSIO_LIBRARIES})

install(TARGETS indi_fli_ccd DESTINATION ${BIN_INSTALL_DIR} )

endif (CFITSIO_FOUND)
#################################################################################

########### Apogee Alta ############
if (CFITSIO_FOUND)

set(apogee_alta_SRCS
	${indimain_SRCS}
	${libapogee_SRCS}	
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/ccd/indialta.c
)

kde4_add_executable(indi_apogee_alta ${apogee_alta_SRCS} ${libfli_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_apogee_alta ${KDE4_KDECORE_LIBS} ${LIBUSB_LIBRARIES} ${CFITSIO_LIBRARIES})

install(TARGETS indi_apogee_alta DESTINATION ${BIN_INSTALL_DIR} )

endif (CFITSIO_FOUND)

#################################################################################

############# SBIG CCD ###############
if (CFITSIO_FOUND)

set(sbigccd_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/ccd/sbigcam.cpp
   )

kde4_add_executable(indi_sbig_ccd ${sbigccd_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_sbig_ccd ${KDE4_KDECORE_LIBS} ${CFITSIO_LIBRARIES} ${SBIG_LIBRARIES})

install(TARGETS indi_sbig_ccd DESTINATION ${BIN_INSTALL_DIR} )

endif (CFITSIO_FOUND)
#################################################################################


#########################################
########### VIDEO GROUP   ###############
#########################################

########### STV #######################
if (CFITSIO_FOUND)

set(stv_SRCS 
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/stvdriver.c 
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/stv.c )

kde4_add_executable(indi_sbig_stv NOGUI ${stv_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_sbig_stv ${KDE4_KDECORE_LIBS} ${CFITSIO_LIBRARIES})

install(TARGETS indi_sbig_stv DESTINATION ${BIN_INSTALL_DIR} )

endif(CFITSIO_FOUND)

#################################################################################

### Meade Lunar Planetary Imager ########
if (CFITSIO_FOUND)

ADD_DEFINITIONS(-DHAVE_LINUX_VIDEODEV2_H)

set(meade_lpi_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/v4ldriver.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/indi_lpi.cpp
   )

kde4_add_executable(indi_meade_lpi ${meade_lpi_SRCS} ${libwebcam_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_meade_lpi ${KDE4_KDECORE_LIBS}  ${CFITSIO_LIBRARIES})

install(TARGETS indi_meade_lpi DESTINATION ${BIN_INSTALL_DIR} )

endif (CFITSIO_FOUND)

#################################################################################

########### V4L Philips ##############
if (CFITSIO_FOUND)

set(v4lphilips_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/v4ldriver.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/v4lphilips.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/indi_philips.cpp
)

kde4_add_executable(indi_v4l_philips ${v4lphilips_SRCS} ${libwebcam_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_v4l_philips m z ${KDE4_KDECORE_LIBS}  ${CFITSIO_LIBRARIES})

install(TARGETS indi_v4l_philips DESTINATION ${BIN_INSTALL_DIR} )

endif (CFITSIO_FOUND)

#################################################################################

########### Generic V4L Driver ###############
if (CFITSIO_FOUND)

set(v4ldriver_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/v4ldriver.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/indi_v4l.cpp
   )

kde4_add_executable(indi_v4l_generic ${v4ldriver_SRCS} ${libwebcam_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_v4l_generic m z ${KDE4_KDECORE_LIBS} ${CFITSIO_LIBRARIES})

install(TARGETS indi_v4l_generic DESTINATION ${BIN_INSTALL_DIR} )

endif (CFITSIO_FOUND)

#################################################################################

ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

install( FILES drivers.xml DESTINATION  ${DATA_INSTALL_DIR}/kstars )
endif(NOT WIN32)
