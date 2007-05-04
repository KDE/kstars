project(kstars)

# some configure checks needed for kstars
include(CheckIncludeFiles)

find_package(ZLIB REQUIRED)
macro_optional_find_package(CFitsio)
macro_optional_find_package(SBIG)

macro_bool_to_01(CFITSIO_FOUND HAVE_CFITSIO_H)

check_include_files(linux/videodev2.h HAVE_LINUX_VIDEODEV2_H)
check_include_files(termios.h TERMIOS_FOUND)
macro_bool_to_01(TERMIOS_FOUND HAVE_TERMIOS_H)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
# The generated file is called config.h since indi is non-kde code and expects a config.h
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kstars.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kstars.h )

macro_log_feature(SBIG_FOUND "libsbigudrv" "SBIG Universal Driver is needed by KStars" "http://indi.sourceforge.net/" FALSE "1.00" "")
macro_log_feature(CFITSIO_FOUND "libcfisio" "CFITSIO Library is needed by KStars for many INDI drivers & for KStars FITS Viewer." "http://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.html" FALSE "3.03" "")

add_subdirectory( kstars )

