project(kstars)

# some configure checks needed for kstars
include(CheckIncludeFiles)

macro_optional_find_package(CFitsio)
macro_bool_to_01(CFITSIO_FOUND HAVE_CFITSIO_H)
macro_log_feature(CFITSIO_FOUND "libcfitsio0" "A library for reading and writing data files in FITS (Flexible Image Transport System) data format" "http://indi.sf.net" FALSE "3.09" "Gives KStars support for FITS images.")

# INDI is a Linux-specific addon
IF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
macro_optional_find_package(INDI)
macro_bool_to_01(INDI_FOUND HAVE_INDI_H)
macro_log_feature(INDI_FOUND "libindi" "A framework for controlling astronomical devices such as telescopes, CCDs, filter wheels..etc." "http://www.indilib.org" FALSE "0.6.2" "Gives KStars support for controlling astronomical devices.")
ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

check_include_files(linux/videodev2.h HAVE_LINUX_VIDEODEV2_H)
check_include_files(termios.h TERMIOS_FOUND)
macro_bool_to_01(TERMIOS_FOUND HAVE_TERMIOS_H)

macro_optional_find_package(Xplanet)
macro_bool_to_01(XPLANET_FOUND HAVE_XPLANET)
macro_log_feature(XPLANET_FOUND "XPlanet" "Renders an image of all the major planets and most satellites" "http://xplanet.sourceforge.net/" FALSE "1.0" "Gives KStars support for xplanet.")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kstars.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kstars.h )

add_subdirectory( kstars )
