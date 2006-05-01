bin_PROGRAMS = kstars

kstars_ui = addcatdialogui.ui addlinkdialogui.ui ccdpreviewui.ui contrastbrightnessgui.ui \
		details_data.ui details_position.ui details_links.ui details_database.ui details_log.ui \
		devmanager.ui finddialogui.ui fitsheaderdialog.ui focusdialogui.ui \
		fovdialogui.ui newfovui.ui locationdialogui.ui \
		histdialog.ui imagereductionui.ui imgsequencedlgui.ui indiconf.ui indihostconf.ui \
		wizwelcomeui.ui wizlocationui.ui wizdevicesui.ui wizdownloadui.ui \
		opsadvancedui.ui opscatalogui.ui opscolorsui.ui opsguidesui.ui opssolarsystemui.ui \
		statform.ui streamformui.ui telescopewizard.ui telescopepropui.ui \
		thumbnailpickerui.ui thumbnaileditorui.ui 

kstars_SOURCES = addcatdialog.cpp addlinkdialog.cpp colorscheme.cpp \
			conbridlg.cpp csegment.cpp deepskyobject.cpp detaildialog.cpp devicemanager.cpp \
			dms.cpp finddialog.cpp fitshistogram.cpp ccdpreviewwg.cpp \
			fitsimage.cpp fitsprocess.cpp fitsviewer.cpp focusdialog.cpp fov.cpp fovdialog.cpp \
			geolocation.cpp imagereductiondlg.cpp imagesequence.cpp imageviewer.cpp indidevice.cpp \
			indidriver.cpp indielement.cpp indifitsconf.cpp indigroup.cpp indimenu.cpp \
			indiproperty.cpp indistd.cpp infobox.cpp infoboxes.cpp jupitermoons.cpp ksasteroid.cpp \
			kscomet.cpp ksfilereader.cpp ksmoon.cpp ksnewstuff.cpp ksnumbers.cpp ksplanet.cpp \
			ksplanetbase.cpp kspluto.cpp kspopupmenu.cpp kssun.cpp kstars.cpp kstarsactions.cpp \
			kstarsdata.cpp kstarsdatetime.cpp kstarsdcop.cpp kstarsinit.cpp kstarssplash.cpp \
			ksutils.cpp kswizard.cpp locationdialog.cpp main.cpp \
			opsadvanced.cpp opscatalog.cpp opscolors.cpp \
			opsguides.cpp opssolarsystem.cpp simclock.cpp skymap.cpp \
			skymapdraw.cpp skymapevents.cpp skyobject.cpp skypoint.cpp \
			starobject.cpp streamwg.cpp telescopewizardprocess.cpp telescopeprop.cpp \
			timedialog.cpp timezonerule.cpp \
			toggleaction.cpp thumbnailpicker.cpp thumbnaileditor.cpp \
			Options.kcfgc kstarsinterface.skel simclockinterface.skel $(kstars_ui)

kde_kcfg_DATA = kstars.kcfg
kde_conf_DATA = kstarsrc

kstars_LDADD = indi/liblilxml.a indi/libindicom.a indi/cfitsio/libcfitsio.la skycomponents/libkstarscomponents.a widgets/libkstarswidgets.a tools/libkstarstools.a $(top_builddir)/libkdeedu/kdeeduplot/libkdeeduplot.la $(top_builddir)/libkdeedu/kdeeduui/libkdeeduui.la $(top_builddir)/libkdeedu/extdate/libextdate.la -lkdeprint -lknewstuff -lm -lz

SUBDIRS = skycomponents widgets indi tools . data icons 

KDE_ICON=AUTO

kstars_COMPILE_FIRST = Options.kcfgc $(kstars_ui)

EXTRA_DIST = kstars.desktop kstarsui.rc fitsviewer.rc

# needed for sincos() GNU extension to math.h
KDE_CXXFLAGS = -D_GNU_SOURCE

xdg_apps_DATA = kstars.desktop

rcdir = $(kde_datadir)/kstars/
rc_DATA = kstarsui.rc fitsviewer.rc

include_HEADERS = kstarsinterface.h simclockinterface.h

# set the include path for X, qt and KDE
INCLUDES= -I$(top_srcdir) -I$(top_srcdir)/libkdeedu/extdate -I$(srcdir)/tools $(all_includes)

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

noinst_HEADERS = fovdialog.h

kstarsactions.o: tools/planetviewerui.h
