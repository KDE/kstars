
add_subdirectory( fli ) 
add_subdirectory( webcam ) 
add_subdirectory( apogee ) 
add_subdirectory( cfitsio ) 

include_directories( ${CMAKE_CURRENT_BINARY_DIR}/../..  )


option(ssh_tunnel "INDI Server binds locally. Remote clients may only connect via SSH Tunneling." OFF)

if (ssh_tunnel)
   add_definition(-DSSH_TUNNEL)
endif(ssh_tunnel)

set(liblilxml_SRCS  ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/lilxml.c )

set(libindicom_SRCS 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/indicom.c 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/base64.c 
	)

set(libcfitsio_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/buffers.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/cfileio.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/checksum.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/compress.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/drvrfile.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/drvrmem.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/drvrnet.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/drvrsmem.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/editcol.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/edithdu.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/eval_l.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/eval_y.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/eval_f.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/fitscore.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcol.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcolb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcold.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcole.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcoli.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcolj.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcolk.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcoll.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcols.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcolsb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcoluk.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcolui.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcoluj.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getkey.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/group.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/grparser.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/histo.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/iraffits.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/modkey.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcol.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcold.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcole.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcoli.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolj.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolk.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcoluk.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcoll.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcols.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolsb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolu.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolui.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcoluj.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putkey.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/region.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/scalnull.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/swapproc.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/wcssub.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/wcsutil.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/imcompress.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/quantize.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/ricecomp.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/pliocomp.c
	)

set(libapogee_usb_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD3011HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD3011LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD4240HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD4240LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD4710HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD4710LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD4710LS2.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD4710LS3.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD4710LS4.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD4710LS5.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD4720HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD4720LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD5520HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD5520LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD5710HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD5710LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD7700HS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_CCD7700LS.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_KAF0261E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_KAF0401E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_KAF1001E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_KAF1301E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_KAF1401E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_KAF3200E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_KAF4202.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_KAF1602E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_KAF16801E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_KAF6303E.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamData_TH7899.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamTable.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamera.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamera_USB.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApnCamera_Linux.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/ApogeeUsbLinux.cpp
	)

set(libapogee_ISA_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/CameraIO_Linux.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/CameraIO_LinuxISA.cpp
	)

set(libapogee_PCI_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/CameraIO_Linux.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/CameraIO_LinuxPCI.cpp
	)

set(libapogee_PPI_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/CameraIO_Linux.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/apogee/CameraIO_LinuxPPI.cpp
	)

set(libfli_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-camera.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-camera-parport.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-camera-usb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-filter-focuser.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-mem.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-serial.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-sys.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-usb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-debug.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-usb-sys-linux.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/fli/libfli-parport.c
	)

set(libwebcam_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/webcam/PPort.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/webcam/port.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/webcam/v4l2_base.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/webcam/ccvt_c2.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/webcam/ccvt_misc.c
	)

########### next target ###############
## SBIG Universal Driver Dummy Library ###
set(sbigudrv_LIB_SRCS sbig_dummy.cpp)

kde4_add_library(sbigudrv SHARED ${sbigudrv_LIB_SRCS})

target_link_libraries(sbigudrv)

set_target_properties(sbigudrv PROPERTIES VERSION 1.0.0 SOVERSION 1 )
install(TARGETS sbigudrv  DESTINATION ${LIB_INSTALL_DIR} )

########### next target ###############
########### INDI Server ##############
set(indiserver_SRCS indiserver.c fq.c )

kde4_add_executable(indiserver ${indiserver_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indiserver  ${KDE4_KDECORE_LIBS} pthread )

install(TARGETS indiserver  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
########### LX200 Basic #############
set(lx200basic_SRCS 
   indidrivermain.c 
   eventloop.c 
   lx200driver.c 
   lx200basic.cpp )

kde4_add_executable(lx200basic ${lx200basic_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(lx200basic  ${KDE4_KDECORE_LIBS} )

install(TARGETS lx200basic  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
########### LX200 Generic ###########
set(lx200generic_SRCS 
   indidrivermain.c 
   eventloop.c 
   lx200driver.c 
   lx200autostar.cpp 
   lx200_16.cpp 
   lx200gps.cpp 
   lx200generic.cpp 
   lx200classic.cpp )

kde4_add_executable(lx200generic ${lx200generic_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(lx200generic  ${KDE4_KDECORE_LIBS} m )

install(TARGETS lx200generic DESTINATION ${BIN_INSTALL_DIR} )

## Make symbolic links to lx200generic ##
ADD_CUSTOM_COMMAND(TARGET lx200generic POST_INSTALL COMMAND rm ARGS -f ${BIN_INSTALL_DIR}/lx200classic)
ADD_CUSTOM_COMMAND(TARGET lx200generic POST_INSTALL COMMAND ln ARGS -s lx200generic ${BIN_INSTALL_DIR}/lx200classic)

ADD_CUSTOM_COMMAND(TARGET lx200generic POST_INSTALL COMMAND rm ARGS -f ${BIN_INSTALL_DIR}/lx200autostar)
ADD_CUSTOM_COMMAND(TARGET lx200generic POST_INSTALL COMMAND ln ARGS -s lx200generic ${BIN_INSTALL_DIR}/lx200autostar)

ADD_CUSTOM_COMMAND(TARGET lx200generic POST_INSTALL COMMAND rm ARGS -f ${BIN_INSTALL_DIR}/lx200_16)
ADD_CUSTOM_COMMAND(TARGET lx200generic POST_INSTALL COMMAND ln ARGS -s lx200generic ${BIN_INSTALL_DIR}/lx200_16)

ADD_CUSTOM_COMMAND(TARGET lx200generic POST_INSTALL COMMAND rm ARGS -f ${BIN_INSTALL_DIR}/lx200gps)
ADD_CUSTOM_COMMAND(TARGET lx200generic POST_INSTALL COMMAND ln ARGS -s lx200generic ${BIN_INSTALL_DIR}/lx200gps)

########### next target ###############
########### Celestron GPS ############
set(celestrongps_SRCS 
   indidrivermain.c 
   eventloop.c 
   celestronprotocol.c 
   celestrongps.cpp )

kde4_add_executable(celestrongps ${celestrongps_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(celestrongps ${KDE4_KDECORE_LIBS} m )

install(TARGETS celestrongps  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
########### Orion Atlas ########
set(orionatlas_SRCS 
   indidrivermain.c 
   eventloop.c 
   orionatlas.cpp )

kde4_add_executable(orionatlas ${orionatlas_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(orionatlas  ${KDE4_KDECORE_LIBS} )

install(TARGETS orionatlas  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
########### Takahashi Temma #########
set(temma_SRCS 
   indidrivermain.c 
   eventloop.c 
   temmadriver.c )

kde4_add_executable(temma ${temma_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(temma  ${KDE4_KDECORE_LIBS} m )

install(TARGETS temma  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
########### Sky Commander ##########
set(skycommander_SRCS 
   indidrivermain.c 
   eventloop.c 
   lx200driver.c 
   skycommander.c )

kde4_add_executable(skycommander ${skycommander_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(skycommander  ${KDE4_KDECORE_LIBS} m )

install(TARGETS skycommander  DESTINATION ${BIN_INSTALL_DIR} )


########### next target ###############
########### Intelliscope ###############
set(intelliscope_SRCS 
   indidrivermain.c 
   eventloop.c 
   lx200driver.c 
   intelliscope.c )

kde4_add_executable(intelliscope ${intelliscope_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(intelliscope  ${KDE4_KDECORE_LIBS} m )

install(TARGETS intelliscope  DESTINATION ${BIN_INSTALL_DIR} )

########### LINUX SPECIFC ###############
#### Build the following targets on Linux Only ####

IF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

ADD_DEFINITIONS(-DTOP_DATADIR=\\"${KDE4_INSTALL_DIR}${DATA_INSTALL_DIR}/kstars\\")

########### next target ###############
########### Apgoee PPI ##############
set(apogee_ppi_SRCS
	apogee_ppi.cpp
	eventloop.c
	indidrivermain.c )

# If we have libusb
IF(LIBUSB_FOUND)

kde4_add_executable(apogee_ppi ${apogee_ppi_SRCS} ${libapogee_usb_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(apogee_ppi usb ${KDE4_KDECORE_LIBS} )

ENDIF(LIBUSB_FOUND)

kde4_add_executable(apogee_ppi ${apogee_ppi_SRCS} ${libapogee_PPI_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(apogee_ppi ${KDE4_KDECORE_LIBS} )

install(TARGETS apogee_ppi  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
###### FLI Precision Digital Focuser ######

set(flipdf_SRCS eventloop.c fli_pdf.c indidrivermain.c)

kde4_add_executable(flipdf ${flipdf_SRCS} ${libfli_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(flipdf m ${KDE4_KDECORE_LIBS} )

install(TARGETS flipdf  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
###### FLI Precision Digital Focuser ######

set(robofocus_SRCS eventloop.c robofocus.c robofocusdriver.c indidrivermain.c)

kde4_add_executable(robofocus ${robofocus_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(robofocus ${KDE4_KDECORE_LIBS} )

install(TARGETS robofocus  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
########## True Technology Wheel ############
set(trutechwheel_SRCS eventloop.c trutech_wheel.c indidrivermain.c )

kde4_add_executable(trutechwheel ${trutechwheel_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(trutechwheel  ${KDE4_KDECORE_LIBS} )

install(TARGETS trutechwheel  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
########## FLI Filter Wheel ############
set(fliwheel_SRCS eventloop.c fli_wheel.c indidrivermain.c )

kde4_add_executable(fliwheel ${fliwheel_SRCS}  ${libfli_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(fliwheel  ${KDE4_KDECORE_LIBS} )

install(TARGETS fliwheel  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
############# FLI CCD ###############
set(fliccd_SRCS eventloop.c fli_ccd.c indidrivermain.c)

kde4_automoc(${fliccd_SRCS})

kde4_add_executable(fliccd ${fliccd_SRCS} ${libfli_SRCS} ${libcfitsio_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(fliccd m z ${KDE4_KDECORE_LIBS} )

install(TARGETS fliccd  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
### Meade Lunar Planetary Imager ########
ADD_DEFINITIONS(-DHAVE_LINUX_VIDEODEV2_H)

set(meade_lpi_SRCS eventloop.c indidrivermain.c v4ldriver.cpp indi_lpi.cpp )

kde4_add_executable(meade_lpi ${meade_lpi_SRCS} ${libwebcam_SRCS} ${libcfitsio_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(meade_lpi z ${KDE4_KDECORE_LIBS} )

install(TARGETS meade_lpi  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
########### V4L Philips ##############
set(v4lphilips_SRCS eventloop.c indidrivermain.c v4ldriver.cpp v4lphilips.cpp indi_philips.cpp)

kde4_add_executable(v4lphilips ${v4lphilips_SRCS} ${libwebcam_SRCS} ${libcfitsio_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(v4lphilips m z ${KDE4_KDECORE_LIBS} )

install(TARGETS v4lphilips  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
########### V4L Driver ###############
set(v4ldriver_SRCS eventloop.c indidrivermain.c v4ldriver.cpp indi_v4l.cpp)

kde4_add_executable(v4ldriver ${v4ldriver_SRCS} ${libwebcam_SRCS} ${libcfitsio_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(v4ldriver m z ${KDE4_KDECORE_LIBS} )

install(TARGETS v4ldriver  DESTINATION ${BIN_INSTALL_DIR} )

########### next target ###############
########### SBIG CCD ###############
set(sbigccd_SRCS sbigcam.cpp eventloop.c indidrivermain.c)

kde4_add_executable(sbigccd ${sbigccd_SRCS} ${libcfitsio_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(sbigccd sbigudrv ${KDE4_KDECORE_LIBS} )

install(TARGETS sbigccd  DESTINATION ${BIN_INSTALL_DIR} )

ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

install( FILES drivers.xml apogee_caminfo.xml  DESTINATION  ${DATA_INSTALL_DIR}/kstars )


#original Makefile.am contents follow:

#appdatadir = $(kde_datadir)/kstars
#appdata_DATA = drivers.xml apogee_caminfo.xml
#
#AM_CPPFLAGS = -DTOP_DATADIR=\"$(appdatadir)\"
#
#if LINUX
#bin_add   =  apogee_ppi sbigccd v4ldriver v4lphilips meade_lpi fliccd fliwheel flipdf
#endif
#
#bin_PROGRAMS = indiserver lx200basic lx200generic celestrongps apmount temma skycommander intelliscope $(bin_add)
#
#liblilxml_a_SOURCES = lilxml.c
#libindicom_a_SOURCES = indicom.c base64.c
#
#noinst_LIBRARIES =  liblilxml.a libindicom.a
#
#indiserver_SOURCES = indiserver.c fq.c
#indiserver_LDADD = liblilxml.a -lpthread $(LIBSOCKET)
#
#lx200generic_SOURCES = indidrivermain.c base64.c eventloop.c lx200driver.c lx200autostar.cpp lx200_16.cpp lx200gps.cpp lx200generic.cpp lx200classic.cpp
#lx200generic_LDADD = liblilxml.a libindicom.a -lm
#
#lx200basic_SOURCES = indidrivermain.c base64.c eventloop.c lx200driver.c lx200basic.cpp
#lx200basic_LDADD = liblilxml.a libindicom.a
#
#celestrongps_SOURCES =  indidrivermain.c base64.c eventloop.c celestronprotocol.c celestrongps.cpp
#celestrongps_LDADD = liblilxml.a libindicom.a -lm
#
#apmount_SOURCES = indidrivermain.c base64.c eventloop.c  lx200driver.c apmount.cpp
#apmount_LDADD =  liblilxml.a libindicom.a
#
#fliccd_SOURCES =  eventloop.c base64.c fli_ccd.c indidrivermain.c
#fliccd_LDADD = fli/libfli.la libindicom.a liblilxml.a cfitsio/libcfitsio.la -lm -lz $(LIBSOCKET)
#
#fliwheel_SOURCES =  eventloop.c base64.c fli_wheel.c indidrivermain.c
#fliwheel_LDADD = fli/libfli.la libindicom.a liblilxml.a -lm
#
#flipdf_SOURCES = eventloop.c base64.c fli_pdf.c indidrivermain.c
#flipdf_LDADD = fli/libfli.la libindicom.a liblilxml.a -lm
#
#v4ldriver_SOURCES = eventloop.c base64.c indidrivermain.c v4ldriver.cpp indi_v4l.cpp
#v4ldriver_LDADD = libindicom.a liblilxml.a webcam/libwebcam.la cfitsio/libcfitsio.la -lm -lz
#
#v4lphilips_SOURCES = eventloop.c base64.c indidrivermain.c v4ldriver.cpp v4lphilips.cpp indi_philips.cpp
#v4lphilips_LDADD = libindicom.a liblilxml.a webcam/libwebcam.la cfitsio/libcfitsio.la -lm -lz
#
#meade_lpi_SOURCES = eventloop.c base64.c indidrivermain.c v4ldriver.cpp indi_lpi.cpp
#meade_lpi_LDADD = libindicom.a liblilxml.a webcam/libwebcam.la cfitsio/libcfitsio.la -lz
#
#temma_SOURCES =  indidrivermain.c base64.c eventloop.c temmadriver.c
#temma_LDADD = liblilxml.a libindicom.a -lm
#
#skycommander_SOURCES =  indidrivermain.c base64.c eventloop.c lx200driver.c skycommander.c
#skycommander_LDADD = liblilxml.a libindicom.a -lm
#
#intelliscope_SOURCES =  indidrivermain.c base64.c eventloop.c lx200driver.c intelliscope.c
#intelliscope_LDADD = liblilxml.a libindicom.a -lm
#
#apogee_ppi_SOURCES = apogee_ppi.cpp base64.c eventloop.c indidrivermain.c
#apogee_ppi_LDADD = libindicom.a liblilxml.a apogee/libapogee_PPI.la -lz
#
#sbigccd_SOURCES = sbigccd.cpp base64.c eventloop.c indidrivermain.c
#sbigccd_LDADD = libindicom.a liblilxml.a -lz
#
#install-exec-hook:
#	$(mkinstalldirs) $(DESTDIR)$(bindir)
#	rm -f $(DESTDIR)$(bindir)/lx200classic
#	$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200classic
#	rm -f $(DESTDIR)$(bindir)/lx200autostar
#	$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200autostar
#	rm -f $(DESTDIR)$(bindir)/lx200_16
#	$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200_16
#	rm -f $(DESTDIR)$(bindir)/lx200gps
#	$(LN_S) lx200generic $(DESTDIR)$(bindir)/lx200gps
#
#uninstall-local:
#	rm $(DESTDIR)$(bindir)/lx200autostar $(DESTDIR)$(bindir)/lx200_16 $(DESTDIR)$(bindir)/lx200gps
#
#KDE_OPTIONS = nofinal
#
#if LINUX
#SUBDIRS = fli webcam apogee cfitsio
#endif
#
#include $(top_srcdir)/admin/Doxyfile.am
