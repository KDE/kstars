bin_PROGRAMS = kstars

kstars_SOURCES = addcatdialog.cpp addlinkdialog.cpp astrocalc.cpp colorscheme.cpp detaildialog.cpp dmsbox.cpp dms.cpp altvstime.cpp filesource.cpp finddialog.cpp focusdialog.cpp geolocation.cpp imageviewer.cpp indidevice.cpp indidriver.cpp indimenu.cpp infobox.cpp infoboxes.cpp jmoontool.cpp jupitermoons.cpp ksasteroid.cpp kscomet.cpp ksfilereader.cpp ksmoon.cpp ksnumbers.cpp ksplanetbase.cpp ksplanet.cpp kspluto.cpp kspopupmenu.cpp kssun.cpp kstarsactions.cpp kstars.cpp kstarsdata.cpp kstarsdcop.cpp kstarsinit.cpp kstarsmessagebox.cpp kstarsoptions.cpp kstars_options_handling.cpp kstarsplotwidget.cpp kstarssplash.cpp ksutils.cpp lcgenerator.cpp locationdialog.cpp magnitudespinbox.cpp main.cpp mapcanvas.cpp modcalcapcoord.cpp modcalcazel.cpp modcalcdaylength.cpp modcalcgalcoord.cpp modcalcgeodcoord.cpp modcalcjd.cpp modcalcprec.cpp modcalcsidtime.cpp objectnamelist.cpp planetcatalog.cpp planetviewer.cpp scriptbuilder.cpp scriptfunction.cpp simclock.cpp skymap.cpp skymapdraw.cpp skymapevents.cpp skyobject.cpp skyobjectname.cpp skypoint.cpp stardatasink.cpp starobject.cpp starpixmap.cpp telescopewizardprocess.cpp timebox.cpp timedialog.cpp timespinbox.cpp timestepbox.cpp timeunitbox.cpp timezonerule.cpp toggleaction.cpp viewopsdialog.cpp wutdialog.cpp argchangeviewoption.ui arglooktoward.ui argsetaltaz.ui argsetgeolocation.ui argsetlocaltime.ui argsetradec.ui argsettrack.ui argtimescale.ui argwaitforkey.ui argwaitfor.ui argzoom.ui devmanager.ui focusdialogdlg.ui indiconf.ui indihostconf.ui modcalcapcoorddlg.ui modcalcazeldlg.ui modcalcdaylengthdlg.ui modcalcgalcoorddlg.ui modcalcgeoddlg.ui modcalcjddlg.ui modcalcprecdlg.ui modcalcsidtimedlg.ui opsadvanced.ui opscatalog.ui opscolors.ui opsguides.ui opssolarsystem.ui optionstreeview.ui scriptbuilderui.ui scriptnamedialog.ui telescopewizard.ui kstarsinterface.skel simclockinterface.skel deepskyobject.cpp wutdialogui.ui fovdialog.cpp fovdialogui.ui newfovui.ui fov.cpp

kstars_LDADD = indi/liblilxml.a indi/libindicom.a $(top_builddir)/libkdeedu/kdeeduplot/libkdeeduplot.la -lkdeprint -lm

SUBDIRS = data indi

EXTRA_DIST = kstars.desktop kstarsui.rc


xdg_apps_DATA = kstars.desktop

rcdir = $(kde_datadir)/kstars/
rc_DATA = kstarsui.rc

include_HEADERS = kstarsinterface.h simclockinterface.h

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)

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
	cat data/ngcic*.dat | gawk '{ split(substr( $$0, 78 ), name, " "); \
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
	(cd data && $(PREPARETIPS) > ../tips.cpp)
	$(XGETTEXT) *.cpp *.h -o $(podir)/kstars.pot;
	rm -f tips.cpp

noinst_HEADERS =  fovdialog.h
