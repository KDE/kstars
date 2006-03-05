kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#noinst_LIBRARIES = libkstarswidgets.a
#
#libkstarswidgets_a_SOURCES = clicklabel.cpp ../dms.cpp dmsbox.cpp \
#	draglistbox.cpp kstarsplotwidget.cpp logedit.cpp \
#	magnitudespinbox.cpp mapcanvas.cpp thumbimage.cpp \
#	timebox.cpp timespinbox.cpp timestepbox.cpp timeunitbox.cpp 
#
#INCLUDES = -I.. -I../tools -I$(srcdir)/.. -I$(top_srcdir) $(all_includes)
#
##Dependencies on uic-generated headers
#mapcanvas.o: ../locationdialogui.h
#
#METASOURCES = AUTO
