kde4_header()

add_subdirectory( skycomponents ) 
add_subdirectory( widgets ) 
add_subdirectory( indi ) 
add_subdirectory( tools ) 
add_subdirectory( data ) 
add_subdirectory( icons ) 

include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/tools ${CMAKE_SOURCE_DIR}/libkdeedu/extdate ${CMAKE_SOURCE_DIR}/libkdeedu/kdeeduplot  )


########### next target ###############
set(liblilxml_SRCS  ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/lilxml.c )

set(libindicom_SRCS 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/indicom.c 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/base64.c 
	)

set(libcfitsio_SRCS
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/buffers.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/cfileio.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/checksum.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/compress.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/drvrfile.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/drvrmem.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/drvrnet.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/drvrsmem.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/editcol.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/edithdu.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/eval_l.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/eval_y.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/eval_f.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/fitscore.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcol.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcolb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcold.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcole.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcoli.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcolj.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcolk.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcoll.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcols.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcolsb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcoluk.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcolui.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getcoluj.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/getkey.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/group.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/grparser.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/histo.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/iraffits.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/modkey.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcol.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcold.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcole.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcoli.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolj.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolk.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcoluk.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcoll.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcols.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolsb.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolu.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcolui.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putcoluj.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/putkey.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/region.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/scalnull.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/swapproc.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/wcssub.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/wcsutil.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/imcompress.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/quantize.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/ricecomp.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/cfitsio/pliocomp.c
	)

set(libkstarstools_SRCS 
	tools/altvstime.cpp
	tools/astrocalc.cpp
	tools/jmoontool.cpp
	tools/lcgenerator.cpp
	tools/modcalcangdist.cpp
	tools/modcalcapcoord.cpp
	tools/modcalcazel.cpp
	tools/modcalcdaylength.cpp
	tools/modcalceclipticcoords.cpp
	tools/modcalcequinox.cpp
	tools/modcalcgalcoord.cpp
	tools/modcalcgeodcoord.cpp
	tools/modcalcjd.cpp
	tools/modcalcplanets.cpp
	tools/modcalcprec.cpp
	tools/modcalcsidtime.cpp
	tools/modcalcvlsr.cpp
	tools/observinglist.cpp
	tools/obslistwizard.cpp
	tools/planetviewer.cpp
	tools/pvplotwidget.cpp
	tools/scriptargwidgets.cpp
	tools/scriptbuilder.cpp
	tools/scriptfunction.cpp
	tools/wutdialog.cpp
	)

kde4_add_ui_files(libkstarstools_SRCS
	tools/altvstime.ui
	tools/argchangeviewoption.ui
	tools/argexportimage.ui
	tools/argloadcolorscheme.ui
	tools/arglooktoward.ui
	tools/argprintimage.ui
	tools/argsetactionindi.ui
	tools/argsetaltaz.ui
	tools/argsetccdtempindi.ui
	tools/argsetcolor.ui
	tools/argsetdeviceindi.ui
	tools/argsetfilternumindi.ui
	tools/argsetfocusspeedindi.ui
	tools/argsetfocustimeoutindi.ui
	tools/argsetframetypeindi.ui
	tools/argsetgeolocation.ui
	tools/argsetgeolocationindi.ui
	tools/argsetlocaltime.ui
	tools/argsetportindi.ui
	tools/argsetradec.ui
	tools/argsetscopeactionindi.ui
	tools/argsettargetcoordindi.ui
	tools/argsettargetnameindi.ui
	tools/argsettrack.ui
	tools/argsetutcindi.ui
	tools/argshutdownindi.ui
	tools/argstartexposureindi.ui
	tools/argstartfocusindi.ui
	tools/argstartindi.ui
	tools/argswitchindi.ui
	tools/argtimescale.ui
	tools/argwaitfor.ui
	tools/argwaitforkey.ui
	tools/argzoom.ui
	tools/lcgenerator.ui
	tools/modcalcangdist.ui
	tools/modcalcapcoord.ui
	tools/modcalcazel.ui
	tools/modcalcdaylength.ui
	tools/modcalceclipticcoords.ui
	tools/modcalcequinox.ui
	tools/modcalcgalcoord.ui
	tools/modcalcgeod.ui
	tools/modcalcjd.ui
	tools/modcalcplanets.ui
	tools/modcalcprec.ui
	tools/modcalcsidtime.ui
	tools/modcalcvlsr.ui
	tools/observinglist.ui
	tools/obslistwizard.ui
	tools/optionstreeview.ui
	tools/planetviewer.ui
	tools/scriptbuilder.ui
	tools/scriptnamedialog.ui
	tools/wutdialog.ui )


set(libkstarscomponents_SRCS 
   skycomponents/singlecomponent.cpp 
   skycomponents/listcomponent.cpp 
   skycomponents/pointlistcomponent.cpp 
   skycomponents/solarsystemsinglecomponent.cpp 
   skycomponents/solarsystemlistcomponent.cpp 
   skycomponents/asteroidscomponent.cpp 
   skycomponents/cometscomponent.cpp 
   skycomponents/jupitermoonscomponent.cpp 
   skycomponents/solarsystemcomposite.cpp 
   skycomponents/starcomponent.cpp 
   skycomponents/deepskycomponent.cpp 
   skycomponents/customcatalogcomponent.cpp 
   skycomponents/constellationlinescomposite.cpp 
   skycomponents/constellationlinescomponent.cpp 
   skycomponents/constellationnamescomponent.cpp 
   skycomponents/constellationboundarycomponent.cpp 
   skycomponents/coordinategridcomposite.cpp 
   skycomponents/coordinategridcomponent.cpp 
   skycomponents/eclipticcomponent.cpp 
   skycomponents/equatorcomponent.cpp 
   skycomponents/horizoncomponent.cpp 
   skycomponents/milkywaycomposite.cpp 
   skycomponents/milkywaycomponent.cpp 
   skycomponents/skycomponent.cpp 
   skycomponents/skycomposite.cpp 
   skycomponents/skymapcomposite.cpp 
)


set(libkstarswidgets_SRCS 
	widgets/clicklabel.cpp 
	widgets/dmsbox.cpp 
	widgets/draglistbox.cpp 
	widgets/kstarsplotwidget.cpp 
	widgets/logedit.cpp
	widgets/magnitudespinbox.cpp 
	widgets/mapcanvas.cpp 
	widgets/thumbimage.cpp 
	widgets/timebox.cpp 
	widgets/timespinbox.cpp 
	widgets/timestepbox.cpp 
	widgets/timeunitbox.cpp
)



set(kstars_SRCS ${liblilxml_SRCS} ${libindicom_SRCS} ${libcfitsio_SRCS} ${libkstarswidgets_SRCS} ${libkstarscomponents_SRCS} ${libkstarstools_SRCS} addcatdialog.cpp addlinkdialog.cpp colorscheme.cpp 
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

kde4_add_ui_files(kstars_SRCS addcatdialog.ui addlinkdialog.ui ccdpreview.ui contrastbrightnessg.ui 
       details_data.ui details_position.ui details_links.ui details_database.ui details_log.ui 
       devmanager.ui finddialog.ui fitsheaderdialog.ui focusdialog.ui 
       fovdialog.ui newfov.ui locationdialog.ui 
       histdialog.ui imagereduction.ui imgsequencedlg.ui indiconf.ui indihostconf.ui 
       wizwelcome.ui wizlocation.ui wizdevices.ui wizdownload.ui 
       opsadvanced.ui opscatalog.ui opscolors.ui opsguides.ui opssolarsystem.ui 
       statform.ui streamform.ui telescopewizard.ui telescopeprop.ui 
       thumbnailpicker.ui thumbnaileditor.ui 
)

kde4_add_executable(kstars ${kstars_SRCS})

target_link_libraries(kstars  ${KDE4_KDECORE_LIBS} kdeeduplot kdeeduui extdate kdeprint knewstuff m z )

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
#kstars_ui = addcatdialog.ui addlinkdialog.ui ccdpreview.ui contrastbrightnessg.ui \
#		details_data.ui details_position.ui details_links.ui details_database.ui details_log.ui \
#		devmanager.ui finddialog.ui fitsheaderdialog.ui focusdialog.ui \
#		fovdialog.ui newfov.ui locationdialog.ui \
#		histdialog.ui imagereduction.ui imgsequencedlg.ui indiconf.ui indihostconf.ui \
#		wizwelcome.ui wizlocation.ui wizdevices.ui wizdownload.ui \
#		opsadvanced.ui opscatalog.ui opscolors.ui opsguides.ui opssolarsystem.ui \
#		statform.ui streamform.ui telescopewizard.ui telescopeprop.ui \
#		thumbnailpicker.ui thumbnaileditor.ui \
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
