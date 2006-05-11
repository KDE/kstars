kde4_header()
project(kstars)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kstars.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kstars.h )

add_subdirectory( kstars ) 




########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

######## kdevelop will overwrite this part!!! (begin)##########
#
#SUBDIRS = kstars 
#
#EXTRA_DIST = kstars.kdevprj AUTHORS COPYING ChangeLog INSTALL README TODO kstars.lsm README.customize 
#
######## kdevelop will overwrite this part!!! (end)############
## not a GNU package. You can remove this line, if 
## have all needed files, that a GNU package needs
#AUTOMAKE_OPTIONS = foreign 
#
#DOXYGEN_SET_RECURSIVE = YES
#DOXYGEN_EXCLUDE = kstars/indi/
#include ../admin/Doxyfile.am
