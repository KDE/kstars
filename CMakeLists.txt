kde4_header()
project(kstars)
add_subdirectory( kstars ) 

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### install files ###############


kde4_create_doxygen_docs( RECURSIVE EXCLUDE kstars/indi/  )

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
