kde4_header()

add_subdirectory( skycomponents ) 
add_subdirectory( widgets ) 
add_subdirectory( indi ) 
add_subdirectory( tools ) 
add_subdirectory( data ) 
add_subdirectory( icons ) 

include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/tools/ ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(liblilxml_SRCS  ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/lilxml.c )

set(libindicom_SRCS 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/indicom.c 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/base64.c 
	)

set(libkstarstools_SRCS 
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/altvstime.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/astrocalc.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/jmoontool.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/lcgenerator.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcangdist.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcapcoord.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcazel.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcdaylength.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalceclipticcoords.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcequinox.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcgalcoord.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcgeodcoord.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcjd.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcplanets.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcprec.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcsidtime.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcvlsr.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/observinglist.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/obslistwizard.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/planetviewer.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/pvplotwidget.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/scriptargwidgets.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/scriptbuilder.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/scriptfunction.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/wutdialog.cpp
	)

kde4_add_ui_files(libkstarstools_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/altvstimeui.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argchangeviewoption.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argexportimage.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argloadcolorscheme.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/arglooktoward.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argprintimage.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetactionindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetaltaz.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetccdtempindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetcolor.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetdeviceindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetfilternumindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetfocusspeedindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetfocustimeoutindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetframetypeindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetgeolocation.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetgeolocationindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetlocaltime.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetportindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetradec.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetscopeactionindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsettargetcoordindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsettargetnameindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsettrack.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argsetutcindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argshutdownindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argstartexposureindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argstartfocusindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argstartindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argswitchindi.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argtimescale.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argwaitfor.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argwaitforkey.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/argzoom.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/lcgeneratorui.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcangdistdlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcapcoorddlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcazeldlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcdaylengthdlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalceclipticcoordsdlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcequinoxdlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcgalcoorddlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcgeoddlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcjddlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcplanetsdlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcprecdlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcsidtimedlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/modcalcvlsrdlg.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/observinglistui.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/obslistwizardui.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/optionstreeview.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/planetviewerui.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/scriptbuilderui.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/scriptnamedialog.ui
	${CMAKE_SOURCE_DIR}/kstars/kstars/tools/wutdialogui.ui )


set(libkstarscomponents_SRCS 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/singlecomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/listcomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/pointlistcomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/solarsystemsinglecomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/solarsystemlistcomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/asteroidscomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/cometscomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/jupitermoonscomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/solarsystemcomposite.cpp 
    ${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/starcomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/deepskycomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/customcatalogcomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/constellationlinescomposite.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/constellationlinescomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/constellationnamescomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/constellationboundarycomponent.cpp 
    ${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/coordinategridcomposite.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/coordinategridcomponent.cpp 
    ${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/eclipticcomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/equatorcomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/horizoncomponent.cpp 
    ${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/milkywaycomposite.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/milkywaycomponent.cpp 
    ${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/skycomponent.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/skycomposite.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/skycomponents/skymapcomposite.cpp 
)


set(libkstarswidgets_SRCS 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/clicklabel.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/dmsbox.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/draglistbox.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/kstarsplotwidget.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/logedit.cpp
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/magnitudespinbox.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/mapcanvas.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/thumbimage.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/timebox.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/timespinbox.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/timestepbox.cpp 
	${CMAKE_SOURCE_DIR}/kstars/kstars/widgets/timeunitbox.cpp
)


set( kstars_KCFG_SRCS Options.kcfgc )

set(kstars_SRCS ${liblilxml_SRCS} ${libindicom_SRCS} ${libkstarswidgets_SRCS} ${libkstarscomponents_SRCS} ${libkstarstools_SRCS} addcatdialog.cpp addlinkdialog.cpp colorscheme.cpp 
           conbridlg.cpp csegment.cpp deepskyobject.cpp detaildialog.cpp devicemanager.cpp 
           dms.cpp finddialog.cpp fitshistogram.cpp ccdpreviewwg.cpp 
           fitsimage.cpp fitsprocess.cpp fitsviewer.cpp focusdialog.cpp fov.cpp fovdialog.cpp 
           geolocation.cpp imagereductiondlg.cpp imagesequence.cpp imageviewer.cpp indidevice.cpp 
           indidriver.cpp indielement.cpp indifitsconf.cpp indigroup.cpp indimenu.cpp 
           indiproperty.cpp indistd.cpp infobox.cpp infoboxes.cpp jupitermoons.cpp ksasteroid.cpp 
           kscomet.cpp ksfilereader.cpp ksmoon.cpp ksnewstuff.cpp ksnumbers.cpp ksplanet.cpp 
           ksplanetbase.cpp kspluto.cpp kspopupmenu.cpp kssun.cpp kstars.cpp kstarsactions.cpp 
           kstarsdata.cpp kstarsdatetime.cpp kstarsdcop.cpp kstarsinit.cpp kstarssplash.cpp 
           ksutils.cpp kswizard.cpp locationdialog.cpp main.cpp 
           opsadvanced.cpp opscatalog.cpp opscolors.cpp 
           opsguides.cpp opssolarsystem.cpp simclock.cpp skymap.cpp 
           skymapdraw.cpp skymapevents.cpp skyobject.cpp skypoint.cpp 
           starobject.cpp streamwg.cpp telescopewizardprocess.cpp telescopeprop.cpp 
           timedialog.cpp timezonerule.cpp 
           toggleaction.cpp thumbnailpicker.cpp thumbnaileditor.cpp
)

set( kstars_KCFG_SRCS Options.kcfgc )

kde4_add_kcfg_files(kstars_SRCS ${kstars_KCFG_SRCS})

kde4_automoc(${kstars_SRCS})

kde4_add_dcop_skels(kstars_SRCS kstarsinterface.h simclockinterface.h )

kde4_add_ui_files(kstars_SRCS addcatdialogui.ui addlinkdialogui.ui ccdpreviewui.ui contrastbrightnessgui.ui 
       details_data.ui details_position.ui details_links.ui details_database.ui details_log.ui 
       devmanager.ui finddialogui.ui fitsheaderdialog.ui focusdialogui.ui 
       fovdialogui.ui newfovui.ui locationdialogui.ui 
       histdialog.ui imagereductionui.ui imgsequencedlgui.ui indiconf.ui indihostconf.ui 
       wizwelcomeui.ui wizlocationui.ui wizdevicesui.ui wizdownloadui.ui 
       opsadvancedui.ui opscatalogui.ui opscolorsui.ui opsguidesui.ui opssolarsystemui.ui 
       statform.ui streamformui.ui telescopewizard.ui telescopepropui.ui 
       thumbnailpickerui.ui thumbnaileditorui.ui 
)

kde4_add_executable(kstars ${kstars_SRCS})

target_link_libraries(kstars  ${KDE4_KDECORE_LIBS} cfitsio kdeeduplot kdeeduui extdate kdeprint knewstuff m z )

install_targets(/bin kstars )


########### install files ###############

install_files( ${XDG_APPS_DIR} FILES kstars.desktop )
install_files( ${KCFG_INSTALL_DIR} FILES kstars.kcfg )
install_files( ${CONFIG_INSTALL_DIR} FILES kstarsrc )
install_files( ${DATA_INSTALL_DIR}/kstars/ FILES kstarsui.rc fitsviewer.rc )
install_files( /include FILES kstarsinterface.h simclockinterface.h )

kde4_install_icons( ${ICON_INSTALL_DIR}  hicolor )

kde4_footer()



#original Makefile.am contents follow:

#bin_PROGRAMS = kstars
#
#kstars_ui = addcatdialogui.ui addlinkdialogui.ui ccdpreviewui.ui contrastbrightnessgui.ui \
#		details_data.ui details_position.ui details_links.ui details_database.ui details_log.ui \
#		devmanager.ui finddialogui.ui fitsheaderdialog.ui focusdialogui.ui \
#		fovdialogui.ui newfovui.ui locationdialogui.ui \
#		histdialog.ui imagereductionui.ui imgsequencedlgui.ui indiconf.ui indihostconf.ui \
#		wizwelcomeui.ui wizlocationui.ui wizdevicesui.ui wizdownloadui.ui \
#		opsadvancedui.ui opscatalogui.ui opscolorsui.ui opsguidesui.ui opssolarsystemui.ui \
#		statform.ui streamformui.ui telescopewizard.ui telescopepropui.ui \
#		thumbnailpickerui.ui thumbnaileditorui.ui \
#
#kstars_SOURCES = addcatdialog.cpp addlinkdialog.cpp colorscheme.cpp \
#			conbridlg.cpp csegment.cpp deepskyobject.cpp detaildialog.cpp devicemanager.cpp \
#			dms.cpp finddialog.cpp fitshistogram.cpp ccdpreviewwg.cpp \
#			fitsimage.cpp fitsprocess.cpp fitsviewer.cpp focusdialog.cpp fov.cpp fovdialog.cpp \
#			geolocation.cpp imagereductiondlg.cpp imagesequence.cpp imageviewer.cpp indidevice.cpp \
#			indidriver.cpp indielement.cpp indifitsconf.cpp indigroup.cpp indimenu.cpp \
#			indiproperty.cpp indistd.cpp infobox.cpp infoboxes.cpp jupitermoons.cpp ksasteroid.cpp \
#			kscomet.cpp ksfilereader.cpp ksmoon.cpp ksnewstuff.cpp ksnumbers.cpp ksplanet.cpp \
#			ksplanetbase.cpp kspluto.cpp kspopupmenu.cpp kssun.cpp kstars.cpp kstarsactions.cpp \
#			kstarsdata.cpp kstarsdatetime.cpp kstarsdcop.cpp kstarsinit.cpp kstarssplash.cpp \
#			ksutils.cpp kswizard.cpp locationdialog.cpp main.cpp \
#			opsadvanced.cpp opscatalog.cpp opscolors.cpp \
#			opsguides.cpp opssolarsystem.cpp simclock.cpp skymap.cpp \
#			skymapdraw.cpp skymapevents.cpp skyobject.cpp skypoint.cpp \
#			starobject.cpp streamwg.cpp telescopewizardprocess.cpp telescopeprop.cpp \
#			timedialog.cpp timezonerule.cpp \
#			toggleaction.cpp thumbnailpicker.cpp thumbnaileditor.cpp \
#			Options.kcfgc kstarsinterface.skel simclockinterface.skel $(kstars_ui)
#
#kde_kcfg_DATA = kstars.kcfg
#kde_conf_DATA = kstarsrc
#
#kstars_LDADD = indi/liblilxml.a indi/libindicom.a indi/cfitsio/libcfitsio.la skycomponents/libkstarscomponents.a widgets/libkstarswidgets.a tools/libkstarstools.a $(top_builddir)/libkdeedu/kdeeduplot/libkdeeduplot.la $(top_builddir)/libkdeedu/kdeeduui/libkdeeduui.la $(top_builddir)/libkdeedu/extdate/libextdate.la -lkdeprint -lknewstuff -lm -lz
#
#SUBDIRS = skycomponents widgets indi tools . data icons 
#
#KDE_ICON=AUTO
#
#kstars_COMPILE_FIRST = Options.kcfgc $(kstars_ui)
#
#EXTRA_DIST = kstars.desktop kstarsui.rc fitsviewer.rc
#
## needed for sincos() GNU extension to math.h
#KDE_CXXFLAGS = -D_GNU_SOURCE
#
#xdg_apps_DATA = kstars.desktop
#
#rcdir = $(kde_datadir)/kstars/
#rc_DATA = kstarsui.rc fitsviewer.rc
#
#include_HEADERS = kstarsinterface.h simclockinterface.h
#
## set the include path for X, qt and KDE
#INCLUDES= -I$(top_srcdir) -Itools -I$(srcdir)/tools $(all_includes)
#
#METASOURCES = AUTO
#
## the library search path.
#kstars_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#
## Uncomment the following two lines if you add a ui.rc file for your application to make use of
## KDE
##rcdir = $(kde_datadir)/kstars
##rc_DATA = kstarsui.rc
#
##
## (LW 18/04/2002) Stripped trailing slashes from comments, to keep make happy
## (JH 16/08/2002) Patch submitted by Stefan Asserhall to deal with diacritic characters properly
## (JH 16/08/2002) modified to sort strings alphabetically and filter through uniq.
## (HE 31/08/2002) treat cities, regions, countries separatedly
#stringsfile:
#	cd $(srcdir);
#	rm -f kstars_i18n.cpp;
#	rm -f cities.tmp;
#	rm -f regions.tmp;
#	rm -f countries.tmp;
#
#	echo "#if 0" >> kstars_i18n.cpp;
## extract constellations
#	sed -e "s/\([0-9].*[a-z]\)//" < data/cnames.dat | sed 's/^[A-B] //' | \
#	sed 's/\([A-Z].*\)/i18n("Constellation name (optional)", "\1");/' | sed 's/\ "/"/g' >> "kstars_i18n.cpp";
## extract cities
#	awk 'BEGIN {FS=":"}; {print "\"" $$1 "\""; }' < data/Cities.dat | \
#	sed 's/ *\"$$/\");/g' | sed 's/^\" */i18n(\"City name (optional, probably does not need a translation)\",\"/g' | sed 's/i18n(.*,"");//' >> "cities.tmp";
#	sort cities.tmp | uniq >> kstars_i18n.cpp;
## extract regions
#	awk 'BEGIN {FS=":"}; {print "\"" $$2 "\""; }' < data/Cities.dat | \
#	sed 's/ *\"$$/\");/g' | sed 's/^\" */i18n(\"Region\/state name (optional, rarely needs a translation)\",\"/g' | sed 's/i18n(.*,"");//' >> "regions.tmp";
#	sort regions.tmp | uniq >> kstars_i18n.cpp;
## extract countries
#	awk 'BEGIN {FS=":"}; {print "\"" $$3 "\""; }' < data/Cities.dat | \
#	sed 's/ *\"$$/\");/g' | sed 's/^\" */i18n(\"Country name (optional, but should be translated)\",\"/g' | sed 's/i18n(.*,"");//' >> "countries.tmp";
#	sort countries.tmp | uniq >> kstars_i18n.cpp;
## extract image/info menu items
#	awk 'BEGIN {FS=":"}; {print "i18n(\"Image/info menu item (should be translated)\",\"" $$2 "\");"; }' < data/image_url.dat | \
#	sed 's/i18n(.*,"");//' >> "image_url.tmp";
#	sort image_url.tmp | uniq >> kstars_i18n.cpp;
#	awk 'BEGIN {FS=":"}; {print "i18n(\"Image/info menu item (should be translated)\",\"" $$2 "\");"; }' < data/info_url.dat | \
#	sed 's/i18n(.*,"");//' >> "info_url.tmp";
#	sort info_url.tmp | uniq >> kstars_i18n.cpp;
## star names : some might be different in other languages, or they might have to be adapted to non-Latin alphabets
#	cat data/hip*.dat | perl -e 'while ( $$line=<STDIN> ) { $$starname = substr ($$line,72);    chop $$starname; if ( $$starname =~ /(.*)\:/ ) { $$starname = $$1 . " ";   }   if ( $$starname =~ /(.*\w)(\s+)/) { $$starname = $$1;	$$starnames{$$starname} = 1;   } } foreach $$star( sort keys %starnames) { printf "i18n(\"star name\",\"%s\");\n", $$star; }' >> kstars_i18n.cpp;
## extract deep-sky object names (sorry, I don't know perl-fu ;( ...using AWK )
#	cat data/ngcic*.dat | gawk '{ split(substr( $$0, 77 ), name, " "); \
#	if ( name[1]!="" ) { \
#	printf( "%s", name[1] ); i=2; \
#	while( name[i]!="" ) { printf( " %s", name[i] ); i++; } \
#	printf( "\n" ); } }' | uniq | sort | gawk '{ \
#	printf( "i18n(\"object name (optional)\", \"%s\");\n", $$0 ); }' >> kstars_i18n.cpp
## extract strings from file containing advanced URLs:
#	cat data/advinterface.dat | gawk '( match( $$0, "KSLABEL" ) ) { \
#	name=substr($$0,10); \
#	printf( "i18n(\"Advanced URLs: description or category\", \"%s\")\n", name ); }' >> kstars_i18n.cpp
#
## finish file
#	echo "#endif" >> kstars_i18n.cpp;
## cleanup temporary files
#	rm -f cities.tmp;
#	rm -f regions.tmp;
#	rm -f countries.tmp;
#	rm -f image_url.tmp;
#	rm -f info_url.tmp;
#
#messages: rc.cpp stringsfile
#	rm -f tips.cpp
#	$(EXTRACTRC) *.ui tools/*.ui *.rc >> rc.cpp
#	(cd data && $(PREPARETIPS) > ../tips.cpp)
#	$(XGETTEXT) *.cpp *.h tools/*.cpp tools/*.h -o $(podir)/kstars.pot;
#	rm -f tips.cpp
#	rm -f kstars_i18n.cpp
#
#noinst_HEADERS = fovdialog.h
#
#kstarsactions.o: tools/planetviewerui.h
