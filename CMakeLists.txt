project(kstars)

# some configure checks needed for kstars
include(CheckIncludeFiles)

macro_optional_find_package(CFitsio)
macro_optional_find_package(SBIG)

check_include_files(linux/videodev2.h HAVE_LINUX_VIDEODEV2_H)
check_include_files(termios.h TERMIOS_FOUND)
macro_bool_to_01(TERMIOS_FOUND HAVE_TERMIOS_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kstars.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kstars.h )

macro_log_feature(SBIG_FOUND "libsbigudrv" "SBIG Universal Driver is needed by KStars" "http://indi.sourceforge.net/" FALSE "1.00" "")

add_subdirectory( kstars ) 

