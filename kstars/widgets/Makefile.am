noinst_LIBRARIES = libkstarswidgets.a

libkstarswidgets_a_SOURCES = dmsbox.cpp draglistbox.cpp \
	kstarsplotwidget.cpp magnitudespinbox.cpp \
	timespinbox.cpp timestepbox.cpp timeunitbox.cpp 

libkstarswidgets_a_LDFLAGS = $(all_libraries) -no-undefined 
libkstarswidgets_la_LIBADD  = $(LIB_QTDESIGNER) $(LIB_KIO)

AM_CXXFLAGS = -DQT_PLUGIN
kde_widget_LTLIBRARIES = kdepimwidgets.la
kdepimwidgets_la_LDFLAGS = $(KDE_PLUGIN) -module $(all_libraries)
kdepimwidgets_la_LIBADD = $(LIB_KIO) libkdepim.la
kdepimwidgets_la_SOURCES = kdepimwidgets.cpp 

kdepimwidgets.cpp: $(srcdir)/kstars.widgets
        $(MAKEKDEWIDGETS) -o kstarswidgets.cpp $(srcdir)/kstars.widgets

kdepimwidgets.lo: kstarswidgets.moc
kdepimwidgets.moc: kstarswidgets.cpp
        $(MOC) -o kstarswidgets.moc kstarswidgets.cpp

CLEANFILES = kstarswidgets.cpp

METASOURCES = AUTO
