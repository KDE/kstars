noinst_LIBRARIES = libkstarswidgets.a

libkstarswidgets_a_SOURCES = clicklabel.cpp ../dms.cpp dmsbox.cpp \
	draglistbox.cpp kstarsplotwidget.cpp logedit.cpp \
	magnitudespinbox.cpp mapcanvas.cpp thumbimage.cpp \
	timespinbox.cpp timestepbox.cpp timeunitbox.cpp 

INCLUDES = -I$(srcdir)/.. -I$(top_srcdir) $(all_includes)

#Dependencies on uic-generated headers
mapcanvas.o: ../locationdialogui.h

METASOURCES = AUTO
