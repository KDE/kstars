####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = kstars

## INCLUDES were found outside kdevelop specific part

kstars_SOURCES = modcalcdaylength.cpp infoboxes.cpp infobox.cpp colorscheme.cpp detaildialog.cpp timeunitbox.cpp timestepbox.cpp modcalcapcoord.cpp timezonerule.cpp kstars_options_handling.cpp focusdialog.cpp skymapevents.cpp kstarsmessagebox.cpp addcatdialog.cpp kstarsinit.cpp kstarsactions.cpp objectnamelist.cpp planetcatalog.cpp stardatasink.cpp filesource.cpp modcalcgeodcoord.cpp modcalcsidtime.cpp modcalcgalcoord.cpp modcalcprec.cpp modcalcjd.cpp astrocalc.cpp timebox.cpp dmsbox.cpp ksplanetbase.cpp ksnumbers.cpp ksutils.cpp simclock.cpp toggleaction.cpp addlinkdialog.cpp kspluto.cpp starpixmap.cpp starobject.cpp timespinbox.cpp imageviewer.cpp ksmoon.cpp skyobjectname.cpp magnitudespinbox.cpp kstarsoptions.cpp kstarssplash.cpp kstarsdata.cpp kssun.cpp ksplanet.cpp finddialog.cpp viewopsdialog.cpp mapcanvas.cpp locationdialog.cpp timedialog.cpp skypoint.cpp dms.cpp skyobject.cpp geolocation.cpp skymap.cpp kstars.cpp main.cpp kstarsinterface.skel simclockinterface.skel
include_HEADERS = kstarsinterface.h simclockinterface.h
kstars_LDADD   = -lkio -lkdeprint -lm $(LIB_KFILE) $(LIB_KDEUI) $(LIB_KDECORE) $(LIB_QT) $(LIBSOCKET)

SUBDIRS = data 

EXTRA_DIST = main.cpp kstars.cpp kstars.h kstars.desktop skymap.cpp skymap.h geolocation.cpp geolocation.h skyobject.cpp skyobject.h dms.cpp dms.h skypoint.cpp skypoint.h timedialog.cpp timedialog.h locationdialog.cpp locationdialog.h mapcanvas.cpp mapcanvas.h viewopsdialog.cpp viewopsdialog.h finddialog.cpp finddialog.h ksplanet.cpp ksplanet.h kssun.cpp kssun.h kstarsdata.cpp kstarsdata.h kstarssplash.cpp kstarssplash.h kstarsoptions.h kstarsoptions.cpp magnitudespinbox.cpp magnitudespinbox.h skyobjectname.cpp skyobjectname.h ksmoon.cpp ksmoon.h imageviewer.cpp imageviewer.h timespinbox.cpp timespinbox.h starobject.cpp starobject.h starpixmap.cpp starpixmap.h kspluto.cpp kspluto.h addlinkdialog.cpp addlinkdialog.h toggleaction.h toggleaction.cpp simclock.h simclock.cpp ksutils.h ksutils.cpp ksnumbers.cpp ksnumbers.h kstarsui.rc ksplanetbase.cpp ksplanetbase.h dmsbox.cpp dmsbox.h timebox.cpp timebox.h astrocalc.cpp astrocalc.h modcalcjd.cpp modcalcjd.h modcalcprec.cpp modcalcprec.h modcalcgalcoord.cpp modcalcgalcoord.h modcalcsidtime.cpp modcalcsidtime.h modcalcgeodcoord.cpp modcalcgeodcoord.h filesource.cpp filesource.h stardatasink.cpp stardatasink.h planetcatalog.h planetcatalog.cpp objectnamelist.cpp objectnamelist.h kstarsactions.cpp kstarsinit.cpp addcatdialog.cpp addcatdialog.h kstarsmessagebox.cpp kstarsmessagebox.h skymapevents.cpp focusdialog.cpp focusdialog.h kstars_options_handling.cpp timezonerule.cpp timezonerule.h modcalcapcoord.cpp modcalcapcoord.h timestepbox.cpp timestepbox.h timeunitbox.cpp timeunitbox.h detaildialog.cpp detaildialog.h colorscheme.cpp colorscheme.h infobox.cpp infoboxes.cpp infoboxes.h infobox.h modcalcdaylength.cpp modcalcdaylength.h 

install-data-local:
	$(mkinstalldirs) $(kde_appsdir)/Applications/
	$(INSTALL_DATA) $(srcdir)/kstars.desktop $(kde_appsdir)/Applications/kstars.desktop
	$(mkinstalldirs) $(kde_datadir)/kstars/
	$(INSTALL_DATA) $(srcdir)/kstarsui.rc $(kde_datadir)/kstars/kstarsui.rc

uninstall-local:
	-rm -f $(kde_appsdir)/Applications/kstars.desktop
	-rm -f $(kde_datadir)/kstars/kstarsui.rc

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
# KDE
#rcdir = $(kde_datadir)/kstars
#rc_DATA = kstarsui.rc

#WARNING: if you use a ui.rc file above, use:

# messages: rc.cpp

# instead of 

# messages:

#commenting out for now (JH 30/03/2002)
#contains an error in which leading and trailing 'A's are removed from a
#constellation name; also doesn't insert required comment string for
#constellation names.
#
# (LW 18/04/2002) Stripped trailing slashes from comments, to keep make happy
stringsfile:
	cd $(srcdir);
	rm -f kstars_i18n.cpp; 
	echo "#if 0" >> kstars_i18n.cpp; 
	sed 's/\(.\{33\}\): \(.\{22\}\): \(.\{22\}\):.*/i18n("\1"); i18n("\2"); i18n("\3");/' < data/Cities.dat | sed 's/[ ]\+/ /g' | sed 's/ "/"/g' | sed 's/i18n("");//' >>"kstars_i18n.cpp";
#	sed -e "s/\([0-9].*[a-z]\)//" < data/cnames.dat | sed 's/[A-B]//' | sed 's/\([A-Z].*\)/i18n("\1");/' | sed 's/\ "/"/' >>"kstars_i18n.cpp";
	echo "#endif" >>kstars_i18n.cpp;

messages: 
	$(XGETTEXT) *.cpp *.h -o $(podir)/kstars.pot;

