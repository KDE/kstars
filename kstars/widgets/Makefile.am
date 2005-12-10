INCLUDES = -I$(top_srcdir) $(all_includes)

lib_LTLIBRARIES = libkstarswidgets.la

libkstarswidgets_la_SOURCES = clicklabel.cpp ../dms.cpp dmsbox.cpp \
	draglistbox.cpp kstarsplotwidget.cpp logedit.cpp \
	magnitudespinbox.cpp mapcanvas.cpp thumbimage.cpp \
	timespinbox.cpp timestepbox.cpp timeunitbox.cpp 

#libkstarswidgets_a_LDFLAGS = $(all_libraries) -no-undefined 
#libkstarswidgets_la_LIBADD  = $(LIB_QTDESIGNER) $(LIB_KIO)

##Uncomment for designer plugin library
#AM_CXXFLAGS = -DQT_PLUGIN
#kde_widget_LTLIBRARIES = kstarswidgets.la
#kstarswidgets_la_LDFLAGS = $(KDE_PLUGIN) -module $(all_libraries)
#kstarswidgets_la_LIBADD = $(LIB_KIO) libkstarswidgets.la
#kstarswidgets_la_SOURCES = kstarswidgets.cpp 
#
#kstarswidgets.cpp: $(srcdir)/kstars.widgets
#	$(MAKEKDEWIDGETS) -o kstarswidgets.cpp $(srcdir)/kstars.widgets
#
#kstarswidgets.lo: kstarswidgets.moc
#kstarswidgets.moc: kstarswidgets.cpp
#	$(MOC) -o kstarswidgets.moc kstarswidgets.cpp
#
#CLEANFILES = kstarswidgets.cpp

METASOURCES = AUTO
