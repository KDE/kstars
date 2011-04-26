project(kstars)

find_package(KDE4 REQUIRED)
include(KDE4Defaults)

include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES} ${QT4_INCLUDES})

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

# some configure checks needed for kstars
include(CheckIncludeFiles)

find_package(Eigen2 2.0.3)
macro_log_feature(EIGEN2_FOUND "Eigen2" "Required to build KStars." "http://eigen.tuxfamily.org" TRUE "2.0.3" "")

macro_optional_find_package(CFitsio)
macro_bool_to_01(CFITSIO_FOUND HAVE_CFITSIO_H)
macro_log_feature(CFITSIO_FOUND "libcfitsio0" "Support for the FITS (Flexible Image Transport System) data format in KStars." "http://indi.sf.net" FALSE "3.09" "")

# INDI is a Linux-specific addon
IF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
macro_optional_find_package(INDI)
macro_bool_to_01(INDI_FOUND HAVE_INDI_H)
macro_log_feature(INDI_FOUND "libindi" "Support for controlling astronomical devices with KStars." "http://www.indilib.org" FALSE "0.6.2" "")
ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

check_include_files(linux/videodev2.h HAVE_LINUX_VIDEODEV2_H)
check_include_files(termios.h TERMIOS_FOUND)
macro_bool_to_01(TERMIOS_FOUND HAVE_TERMIOS_H)

macro_optional_find_package(Xplanet)
macro_bool_to_01(XPLANET_FOUND HAVE_XPLANET)
macro_log_feature(XPLANET_FOUND "XPlanet" "Renders an image of all the major planets and most satellites" "http://xplanet.sourceforge.net/" FALSE "1.0" "Gives KStars support for xplanet.")

macro_optional_find_package(OpenGL)
macro_bool_to_01(OPENGL_FOUND HAVE_OPENGL)
macro_log_feature(OPENGL_FOUND "OpenGL" "Support for hardware rendering in KStars." "http://www.opengl.org/" FALSE "" "OpenGL rendering is generally faster on hardware with graphics acceleration.")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kstars.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kstars.h )

add_subdirectory( doc )
add_subdirectory( kstars )

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
   macro_display_feature_log()
endif (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
