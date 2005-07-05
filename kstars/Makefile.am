bin_PROGRAMS = kstars

kstars_SOURCES = addcatdialog.cpp addlinkdialog.cpp colorscheme.cpp \
			conbridlg.cpp csegment.cpp deepskyobject.cpp detaildialog.cpp devicemanager.cpp \
			dms.cpp dmsbox.cpp filesource.cpp finddialog.cpp fitshistogram.cpp ccdpreviewwg.cpp customcatalog.cpp \
			fitsimage.cpp fitsprocess.cpp fitsviewer.cpp focusdialog.cpp fov.cpp fovdialog.cpp \
			geolocation.cpp imagereductiondlg.cpp imagesequence.cpp imageviewer.cpp indidevice.cpp \
			indidriver.cpp indielement.cpp indifitsconf.cpp indigroup.cpp indimenu.cpp \
			indiproperty.cpp indistd.cpp infobox.cpp infoboxes.cpp jupitermoons.cpp ksasteroid.cpp \
			kscomet.cpp ksfilereader.cpp ksmoon.cpp ksnewstuff.cpp ksnumbers.cpp ksplanet.cpp \
			ksplanetbase.cpp kspluto.cpp kspopupmenu.cpp kssun.cpp kstars.cpp kstarsactions.cpp \
			kstarsdata.cpp kstarsdatetime.cpp kstarsdcop.cpp kstarsinit.cpp kstarssplash.cpp \
			ksutils.cpp kswizard.cpp locationdialog.cpp magnitudespinbox.cpp main.cpp \
			mapcanvas.cpp objectnamelist.cpp opsadvanced.cpp opscatalog.cpp opscolors.cpp \
			opsguides.cpp opssolarsystem.cpp planetcatalog.cpp simclock.cpp skymap.cpp \
			skymapdraw.cpp skymapevents.cpp skyobject.cpp skyobjectname.cpp skypoint.cpp \
			stardatasink.cpp starobject.cpp starpixmap.cpp streamwg.cpp telescopewizardprocess.cpp \
			telescopeprop.cpp timebox.cpp timedialog.cpp timespinbox.cpp timestepbox.cpp timeunitbox.cpp \
			timezonerule.cpp toggleaction.cpp thumbnailpicker.cpp thumbnaileditor.cpp draglistbox.cpp \
			addcatdialogui.ui addlinkdialogui.ui ccdpreviewui.ui contrastbrightnessgui.ui details_data.ui \
			details_position.ui details_links.ui details_database.ui details_log.ui devmanager.ui \
			fitsheaderdialog.ui focusdialogdlg.ui fovdialogui.ui histdialog.ui imagereductionui.ui \
			imgsequencedlgui.ui indiconf.ui indihostconf.ui kswizardui.ui newfovui.ui opsadvancedui.ui \
			opscatalogui.ui opscolorsui.ui opsguidesui.ui opssolarsystemui.ui statform.ui \
			streamformui.ui telescopewizard.ui thumbnailpickerui.ui thumbnaileditorui.ui telescopepropui.ui \
			Options.kcfgc kstarsinterface.skel simclockinterface.skel 

kde_kcfg_DATA = kstars.kcfg
kde_conf_DATA = kstarsrc

kstars_LDADD = indi/liblilxml.a indi/libindicom.a tools/libkstarstools.a $(top_builddir)/libkdeedu/kdeeduplot/libkdeeduplot.la $(top_builddir)/libkdeedu/kdeeduui/libkdeeduui.la $(top_builddir)/libkdeedu/extdate/libextdate.la -lkdeprint -lknewstuff -lm -lz

SUBDIRS = indi tools . data icons 

KDE_ICON=AUTO

kstars_COMPILE_FIRST = Options.kcfgc

EXTRA_DIST = kstars.desktop kstarsui.rc fitsviewer.rc

# needed for sincos() GNU extension to math.h
KDE_CXXFLAGS = -D_GNU_SOURCE

xdg_apps_DATA = kstars.desktop

rcdir = $(kde_datadir)/kstars/
rc_DATA = kstarsui.rc fitsviewer.rc

include_HEADERS = kstarsinterface.h simclockinterface.h

# set the include path for X, qt and KDE
INCLUDES= -I$(top_srcdir) -Itools -I$(srcdir)/tools $(all_includes)

METASOURCES = AUTO

# the library search path.
kstars_LDFLAGS = $(all_libraries) $(KDE_RPATH)

# Uncomment the following two lines if you add a ui.rc file for your application to make use of
# KDE
#rcdir = $(kde_datadir)/kstars
#rc_DATA = kstarsui.rc

#
# (LW 18/04/2002) Stripped trailing slashes from comments, to keep make happy
# (JH 16/08/2002) Patch submitted by Stefan Asserhall to deal with diacritic characters properly
# (JH 16/08/2002) modified to sort strings alphabetically and filter through uniq.
# (HE 31/08/2002) treat cities, regions, countries separatedly
stringsfile:
	cd $(srcdir);
	rm -f kstars_i18n.cpp;
	rm -f cities.tmp;
	rm -f regions.tmp;
	rm -f countries.tmp;

	echo "#if 0" >> kstars_i18n.cpp;
# extract constellations
	sed -e "s/\([0-9].*[a-z]\)//" < data/cnames.dat | sed 's/^[A-B] //' | \
	sed 's/\([A-Z].*\)/i18n("Constellation name (optional)", "\1");/' | sed 's/\ "/"/g' >> "kstars_i18n.cpp";
# extract cities
	awk 'BEGIN {FS=":"}; {print "\"" $$1 "\""; }' < data/Cities.dat | \
	sed 's/ *\"$$/\");/g' | sed 's/^\" */i18n(\"City name (optional, probably does not need a translation)\",\"/g' | sed 's/i18n(.*,"");//' >> "cities.tmp";
	sort cities.tmp | uniq >> kstars_i18n.cpp;
# extract regions
	awk 'BEGIN {FS=":"}; {print "\"" $$2 "\""; }' < data/Cities.dat | \
	sed 's/ *\"$$/\");/g' | sed 's/^\" */i18n(\"Region\/state name (optional, rarely needs a translation)\",\"/g' | sed 's/i18n(.*,"");//' >> "regions.tmp";
	sort regions.tmp | uniq >> kstars_i18n.cpp;
# extract countries
	awk 'BEGIN {FS=":"}; {print "\"" $$3 "\""; }' < data/Cities.dat | \
	sed 's/ *\"$$/\");/g' | sed 's/^\" */i18n(\"Country name (optional, but should be translated)\",\"/g' | sed 's/i18n(.*,"");//' >> "countries.tmp";
	sort countries.tmp | uniq >> kstars_i18n.cpp;
# extract image/info menu items
	awk 'BEGIN {FS=":"}; {print "i18n(\"Image/info menu item (should be translated)\",\"" $$2 "\");"; }' < data/image_url.dat | \
	sed 's/i18n(.*,"");//' >> "image_url.tmp";
	sort image_url.tmp | uniq >> kstars_i18n.cpp;
	awk 'BEGIN {FS=":"}; {print "i18n(\"Image/info menu item (should be translated)\",\"" $$2 "\");"; }' < data/info_url.dat | \
	sed 's/i18n(.*,"");//' >> "info_url.tmp";
	sort info_url.tmp | uniq >> kstars_i18n.cpp;
# star names : some might be different in other languages, or they might have to be adapted to non-Latin alphabets
	cat data/hip*.dat | perl -e 'while ( $$line=<STDIN> ) { $$starname = substr ($$line,72);    chop $$starname; if ( $$starname =~ /(.*)\:/ ) { $$starname = $$1 . " ";   }   if ( $$starname =~ /(.*\w)(\s+)/) { $$starname = $$1;	$$starnames{$$starname} = 1;   } } foreach $$star( sort keys %starnames) { printf "i18n(\"star name\",\"%s\");\n", $$star; }' >> kstars_i18n.cpp;
# extract deep-sky object names (sorry, I don't know perl-fu ;( ...using AWK )
	cat data/ngcic*.dat | gawk '{ split(substr( $$0, 77 ), name, " "); \
	if ( name[1]!="" ) { \
	printf( "%s", name[1] ); i=2; \
	while( name[i]!="" ) { printf( " %s", name[i] ); i++; } \
	printf( "\n" ); } }' | uniq | sort | gawk '{ \
	printf( "i18n(\"object name (optional)\", \"%s\");\n", $$0 ); }' >> kstars_i18n.cpp
# extract strings from file containing advanced URLs:
	cat data/advinterface.dat | gawk '( match( $$0, "KSLABEL" ) ) { \
	name=substr($$0,10); \
	printf( "i18n(\"Advanced URLs: description or category\", \"%s\")\n", name ); }' >> kstars_i18n.cpp

# finish file
	echo "#endif" >> kstars_i18n.cpp;
# cleanup temporary files
	rm -f cities.tmp;
	rm -f regions.tmp;
	rm -f countries.tmp;
	rm -f image_url.tmp;
	rm -f info_url.tmp;

messages: rc.cpp stringsfile
	rm -f tips.cpp
	$(EXTRACTRC) *.ui tools/*.ui *.rc > rc.cpp
	(cd data && $(PREPARETIPS) > ../tips.cpp)
	$(XGETTEXT) *.cpp *.h tools/*.cpp tools/*.h -o $(podir)/kstars.pot;
	rm -f tips.cpp
	rm -f kstars_i18n.cpp

noinst_HEADERS = fovdialog.h

kstarsactions.o: tools/planetviewerui.h
