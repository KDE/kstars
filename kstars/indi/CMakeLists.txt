KDE4_NO_ENABLE_FINAL(indi)

include_directories( ${CMAKE_CURRENT_BINARY_DIR}/../.. )
include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs)
include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee)
include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/fli)
include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/webcam)

if (CFITSIO_FOUND)
  include_directories(${CFITSIO_INCLUDE_DIR})
endif (CFITSIO_FOUND)

if (NOVA_FOUND)
    include_directories(${NOVA_INCLUDE_DIR})
endif (NOVA_FOUND)

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
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD3011.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD3011B.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4240.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4240B.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710ALT.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4710B.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4720.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD4720B.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5520.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5520B.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710B.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD5710F.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD7700.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_CCD7700B.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF0261E.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF0261EB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF0401E.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF0401EB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF09000.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF10011105.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1001E.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1001EB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1001ENS.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1301E.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1301EB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1401E.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1602E.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF1602EB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF16801E.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF16803.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF3200E.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF3200EB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF4202.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF6303E.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAF6303EB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000C.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000CL.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000M.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI11000ML.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2001CL.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2001M.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2001ML.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020C.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020CL.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020CLB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020M.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020ML.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI2020MLB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020C.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020CL.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020CLB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020M.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020ML.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_KAI4020MLB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamData_TH7899.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamTable.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamera.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnCamera_USB.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApogeeUsbLinux.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/apogee/ApnGlue.cpp
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
set(GENERIC_LIB_SOVERSION "1")

########### next target ###############
## SBIG Universal Driver Dummy Library ###
if (NOT SBIG_FOUND)
set(sbigudrv_LIB_SRCS
		${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/ccd/sbig_dummy.cpp
)

kde4_add_library(sbigudrv SHARED ${sbigudrv_LIB_SRCS})

target_link_libraries(sbigudrv)

if (NOVA_FOUND)
  target_link_libraries(sbigudrv ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

set_target_properties(sbigudrv PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION} )
install(TARGETS sbigudrv ${INSTALL_TARGETS_DEFAULT_ARGS})

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

if (NOVA_FOUND)
  target_link_libraries(indiserver ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indiserver  ${INSTALL_TARGETS_DEFAULT_ARGS} )

#####################################
########## TELESCOPE GROUP ##########
#####################################

########### LX200 Basic #############
set(lx200basic_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200driver.c
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200basic.cpp )

kde4_add_executable(indi_lx200basic NOGUI ${lx200basic_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_lx200basic  ${KDE4_KDECORE_LIBS})

if (NOVA_FOUND)
  target_link_libraries(indi_lx200basic ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_lx200basic ${INSTALL_TARGETS_DEFAULT_ARGS} )

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

if (NOVA_FOUND)
  target_link_libraries(indi_lx200generic ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_lx200generic ${INSTALL_TARGETS_DEFAULT_ARGS} )

#TODO fix for windows.
if(UNIX)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake 
"exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic ${BIN_INSTALL_DIR}/indi_lx200classic)\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic ${BIN_INSTALL_DIR}/indi_lx200autostar)\n 
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic ${BIN_INSTALL_DIR}/indi_lx200_16)\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic ${BIN_INSTALL_DIR}/indi_lx200gps)\n
")
set_target_properties(indi_lx200generic PROPERTIES POST_INSTALL_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake)
endif(UNIX)

#################################################################################

########### Celestron GPS ############
set(celestrongps_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/celestronprotocol.c
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/celestrongps.cpp )

kde4_add_executable(indi_celestron_gps NOGUI ${celestrongps_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_celestron_gps ${KDE4_KDECORE_LIBS} m )

if (NOVA_FOUND)
  target_link_libraries(indi_celestron_gps ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_celestron_gps ${INSTALL_TARGETS_DEFAULT_ARGS} )

#################################################################################

########### Orion Atlas #############
set(orionatlas_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/orionatlas.cpp )

kde4_add_executable(indi_orion_atlas NOGUI ${orionatlas_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_orion_atlas ${KDE4_KDECORE_LIBS})

if (NOVA_FOUND)
  target_link_libraries(indi_orion_atlas ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_orion_atlas ${INSTALL_TARGETS_DEFAULT_ARGS} )

#################################################################################

########### Takahashi Temma ##########
if (NOVA_FOUND)

set(temma_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/temmadriver.c )

kde4_add_executable(indi_temma NOGUI ${temma_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_temma  ${KDE4_KDECORE_LIBS} ${NOVA_LIBRARIES} m )

install(TARGETS indi_temma ${INSTALL_TARGETS_DEFAULT_ARGS} )

endif (NOVA_FOUND)
#################################################################################

########### Sky Commander #############
set(skycommander_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200driver.c
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/skycommander.c )

kde4_add_executable(indi_skycommander ${skycommander_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_skycommander  ${KDE4_KDECORE_LIBS} m )

if (NOVA_FOUND)
  target_link_libraries(indi_skycommander ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_skycommander  ${INSTALL_TARGETS_DEFAULT_ARGS} )

#################################################################################

########### Intelliscope ###############
set(intelliscope_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/lx200driver.c
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/telescope/intelliscope.c )

kde4_add_executable(indi_intelliscope ${intelliscope_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_intelliscope  ${KDE4_KDECORE_LIBS} m )

if (NOVA_FOUND)
  target_link_libraries(indi_intelliscope ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_intelliscope ${INSTALL_TARGETS_DEFAULT_ARGS} )

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

target_link_libraries(indi_fli_pdf m ${KDE4_KDECORE_LIBS})

if (NOVA_FOUND)
  target_link_libraries(indi_fli_pdf ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_fli_pdf ${INSTALL_TARGETS_DEFAULT_ARGS} )

#################################################################################

################ Robo Focuser ################

set(robofocus_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/focuser/robofocus.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/focuser/robofocusdriver.c
   )

kde4_add_executable(indi_robofocus ${robofocus_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_robofocus ${KDE4_KDECORE_LIBS} m)

if (NOVA_FOUND)
  target_link_libraries(indi_robofocus ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_robofocus ${INSTALL_TARGETS_DEFAULT_ARGS} )

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

target_link_libraries(indi_trutech_wheel  ${KDE4_KDECORE_LIBS} m)

if (NOVA_FOUND)
  target_link_libraries(indi_trutech_wheel ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_trutech_wheel ${INSTALL_TARGETS_DEFAULT_ARGS} )

#################################################################################

########## FLI Filter Wheel ############
set(fliwheel_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/filter_wheel/fli_wheel.c
   )

kde4_add_executable(indi_fli_wheel ${fliwheel_SRCS}  ${libfli_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_fli_wheel ${KDE4_KDECORE_LIBS} m)

if (NOVA_FOUND)
  target_link_libraries(indi_fli_wheel ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_fli_wheel ${INSTALL_TARGETS_DEFAULT_ARGS} )
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

target_link_libraries(indi_fli_ccd ${KDE4_KDECORE_LIBS} ${CFITSIO_LIBRARIES} m)

if (NOVA_FOUND)
  target_link_libraries(indi_fli_ccd ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_fli_ccd ${INSTALL_TARGETS_DEFAULT_ARGS} )

endif (CFITSIO_FOUND)
#################################################################################

########### Apogee Alta ############
if (CFITSIO_FOUND AND LIBUSB_FOUND)

set(apogee_alta_SRCS
	${indimain_SRCS}
	${libapogee_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/ccd/indialta.c
)

kde4_add_executable(indi_apogee_alta ${apogee_alta_SRCS} ${libfli_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_apogee_alta ${KDE4_KDECORE_LIBS} ${LIBUSB_LIBRARIES} ${CFITSIO_LIBRARIES})

if (NOVA_FOUND)
  target_link_libraries(indi_apogee_alta ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_apogee_alta ${INSTALL_TARGETS_DEFAULT_ARGS} )

endif (CFITSIO_FOUND AND LIBUSB_FOUND)

#################################################################################

############# SBIG CCD ###############
if (CFITSIO_FOUND)

set(sbigccd_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/ccd/sbigcam.cpp
   )

kde4_add_executable(indi_sbig_ccd ${sbigccd_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_sbig_ccd ${KDE4_KDECORE_LIBS} ${CFITSIO_LIBRARIES} ${SBIG_LIBRARIES})

if (NOVA_FOUND)
  target_link_libraries(indi_sbig_ccd ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_sbig_ccd ${INSTALL_TARGETS_DEFAULT_ARGS} )

endif (CFITSIO_FOUND)
#################################################################################


#########################################
########### VIDEO GROUP   ###############
#########################################

########### STV #######################
if (CFITSIO_FOUND)
if (NOVA_FOUND)

set(stv_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/stvdriver.c
   ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/drivers/video/stv.c )

kde4_add_executable(indi_sbig_stv NOGUI ${stv_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_sbig_stv ${KDE4_KDECORE_LIBS} ${CFITSIO_LIBRARIES} ${NOVA_LIBRARIES})

install(TARGETS indi_sbig_stv ${INSTALL_TARGETS_DEFAULT_ARGS} )

endif (NOVA_FOUND)
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

if (NOVA_FOUND)
  target_link_libraries(indi_meade_lpi ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_meade_lpi ${INSTALL_TARGETS_DEFAULT_ARGS} )

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

if (NOVA_FOUND)
  target_link_libraries(indi_v4l_philips ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_v4l_philips ${INSTALL_TARGETS_DEFAULT_ARGS} )

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

if (NOVA_FOUND)
  target_link_libraries(indi_v4l_generic ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_v4l_generic ${INSTALL_TARGETS_DEFAULT_ARGS} )

endif (CFITSIO_FOUND)

#################################################################################

ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

install( FILES drivers.xml DESTINATION  ${DATA_INSTALL_DIR}/kstars )
endif(NOT WIN32)
