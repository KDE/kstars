####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = kstars
kstars_SOURCES = kspluto.cpp starpixmap.cpp starobject.cpp timespinbox.cpp imageviewer.cpp ksmoon.cpp skyobjectname.cpp magnitudespinbox.cpp kstarsoptions.cpp kstarssplash.cpp kstarsdata.cpp kssun.cpp ksplanet.cpp finddialog.cpp viewopsdialog.cpp mapcanvas.cpp locationdialog.cpp timedialog.cpp skypoint.cpp dms.cpp skyobject.cpp geolocation.cpp skymap.cpp kstars.cpp main.cpp 
kstars_LDADD   = -lkio -lm $(LIB_KFILE) $(LIB_KDEUI) $(LIB_KDECORE) $(LIB_QT) $(LIBSOCKET)

SUBDIRS = data 

EXTRA_DIST = main.cpp kstars.cpp kstars.h kstars.desktop skymap.cpp skymap.h geolocation.cpp geolocation.h skyobject.cpp skyobject.h dms.cpp dms.h skypoint.cpp skypoint.h timedialog.cpp timedialog.h locationdialog.cpp locationdialog.h mapcanvas.cpp mapcanvas.h viewopsdialog.cpp viewopsdialog.h finddialog.cpp finddialog.h ksplanet.cpp ksplanet.h kssun.cpp kssun.h kstarsdata.cpp kstarsdata.h kstarssplash.cpp kstarssplash.h kstarsoptions.h kstarsoptions.cpp magnitudespinbox.cpp magnitudespinbox.h skyobjectname.cpp skyobjectname.h ksmoon.cpp ksmoon.h imageviewer.cpp imageviewer.h timespinbox.cpp timespinbox.h starobject.cpp starobject.h starpixmap.cpp starpixmap.h kspluto.cpp kspluto.h 

install-data-local:
	$(mkinstalldirs) $(kde_appsdir)/Applications/
	$(INSTALL_DATA) $(srcdir)/kstars.desktop $(kde_appsdir)/Applications/kstars.desktop

uninstall-local:
	-rm -f $(kde_appsdir)/Applications/kstars.desktop

####### kdevelop will overwrite this part!!! (end)############
# this 10 paths are KDE specific. Use them:
# kde_htmldir       Where your docs should go to. (contains lang subdirs)
# kde_appsdir       Where your application file (.kdelnk) should go to. 
# kde_icondir       Where your icon should go to.
# kde_minidir       Where your mini icon should go to.
# kde_datadir       Where you install application data. (Use a subdir)
# kde_locale        Where translation files should go to.(contains lang subdirs)
# kde_cgidir        Where cgi-bin executables should go to.
# kde_confdir       Where config files should go to.
# kde_mimedir       Where mimetypes should go to.
# kde_toolbardir    Where general toolbar icons should go to.
# kde_wallpaperdir  Where general wallpapers should go to.

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)

METASOURCES = AUTO

# the library search path. 
kstars_LDFLAGS = $(all_libraries) $(KDE_RPATH)

# Uncomment the following two lines if you add a ui.rc file for your application to make use of
# KDE´s XML GUI builing
#rcdir = $(kde_datadir)/kstars
#rc_DATA = kstarsui.rc

#WARNING: if you use a ui.rc file above, use:

# messages: rc.cpp

# instead of 

# messages:

messages: 
	LIST=`find . -name \*.h -o -name \*.hh -o -name \*.H -o -name \*.hxx -o -name \*.hpp -o -name \*.cpp -o -name \*.cc -o -name \*.cxx -o -name \*.ecpp -o -name \*.C`; \
	if test -n "$$LIST"; then \
	 $(XGETTEXT) $$LIST -o $(podir)/kstars.pot; \
	fi

