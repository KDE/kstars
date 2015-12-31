add_subdirectory( data )
add_subdirectory( icons )
add_subdirectory( htmesh )
#FIXME Port to QML2
#add_subdirectory( tools/whatsinteresting/qml)

Find_package(ZLIB REQUIRED)
Find_package(Threads REQUIRED)

if(MSVC)
    add_definitions(-D_USE_MATH_DEFINES=1)
    add_definitions(-DNOMINMAX)
endif()

include_directories(
    ${kstars_SOURCE_DIR}/kstars/tools
    ${kstars_SOURCE_DIR}/kstars/skyobjects
    ${kstars_SOURCE_DIR}/kstars/skycomponents
    ${kstars_SOURCE_DIR}/kstars/auxiliary
    ${kstars_SOURCE_DIR}/kstars/time
)

if (CFITSIO_FOUND)
  set (fits_SRCS
    fitsviewer/fitshistogram.cpp
    fitsviewer/fitsdata.cpp
    fitsviewer/fitsview.cpp
    fitsviewer/fitsviewer.cpp
    fitsviewer/fitstab.cpp
    fitsviewer/fitsdebayer.cpp
    fitsviewer/bayer.c
    )
  set (fitsui_SRCS
    fitsviewer/fitsheaderdialog.ui
    fitsviewer/statform.ui
    fitsviewer/fitsdebayer.ui
    indi/streamform.ui
    fitsviewer/fitshistogramui.ui
    )
  include_directories(${CFITSIO_INCLUDE_DIR})
endif(CFITSIO_FOUND)

if (INDI_FOUND)
  set (indi_SRCS
    indi/drivermanager.cpp
    indi/servermanager.cpp
    indi/clientmanager.cpp
    indi/guimanager.cpp
    indi/driverinfo.cpp
    indi/deviceinfo.cpp
    indi/indidevice.cpp
    indi/indigroup.cpp
    indi/indiproperty.cpp
    indi/indielement.cpp
    indi/indistd.cpp
    indi/indilistener.cpp
    indi/inditelescope.cpp
    indi/indiccd.cpp
    indi/indifocuser.cpp
    indi/indifilter.cpp
    indi/indidome.cpp
    indi/indiweather.cpp
    indi/indicap.cpp
    indi/indilightbox.cpp
    indi/indidbus.cpp
    indi/opsindi.cpp
    indi/telescopewizardprocess.cpp
    indi/streamwg.cpp
    )
  set(indiui_SRCS
    indi/drivermanager.ui
    indi/opsindi.ui
    indi/indihostconf.ui
    indi/streamform.ui
    indi/telescopewizard.ui
    )

if (CFITSIO_FOUND)
   set(ekosui_SRCS
      ekos/scheduler.ui
      ekos/mosaic.ui
      ekos/opsekos.ui
      ekos/ekosmanager.ui
      ekos/capture.ui
      ekos/calibrationoptions.ui
      ekos/align.ui
      ekos/focus.ui
      ekos/guide.ui
      ekos/mount.ui
      ekos/guide/guider.ui
      ekos/guide/rcalibration.ui
   )

   set(ekos_SRCS
      ekos/schedulerjob.cpp
      ekos/scheduler.cpp
      ekos/mosaic.cpp
      ekos/ekosmanager.cpp
      ekos/capture.cpp
      ekos/sequencejob.cpp
      ekos/focus.cpp
      ekos/guide.cpp
      ekos/align.cpp
      ekos/mount.cpp
      ekos/dome.cpp
      ekos/weather.cpp
      ekos/dustcap.cpp
      ekos/astrometryparser.cpp
      ekos/offlineastrometryparser.cpp
      ekos/onlineastrometryparser.cpp
      ekos/opsekos.cpp
      ekos/QProgressIndicator.cpp
      ekos/guide/common.cpp
      ekos/guide/gmath.cpp
      ekos/guide/guider.cpp
      ekos/guide/matr.cpp
      ekos/guide/rcalibration.cpp
      ekos/guide/scroll_graph.cpp
      ekos/guide/vect.cpp

   )

endif(CFITSIO_FOUND)

  include_directories(${INDI_INCLUDE_DIR})
endif(INDI_FOUND)

if (WCSLIB_FOUND)
  include_directories( ${WCSLIB_INCLUDE_DIR} )
endif (WCSLIB_FOUND)

if ( XPLANET_FOUND )
  set (xplanet_SRCS
    xplanet/opsxplanet.cpp
    )
  set(xplanetui_SRCS
    xplanet/opsxplanet.ui
    )
endif ( XPLANET_FOUND )

#FIXME Enable OpenGL Later
#if ( OPENGL_FOUND )
#  include_directories( ${OPENGL_INCLUDE_DIRS} )
#  set(kstars_gl_SRCS
#    skyglpainter.cpp skymapgldraw.cpp
#    )
#  add_definitions(-DHAVE_OPENGL)
#endif( OPENGL_FOUND )

########### next target ###############
set(libkstarstools_SRCS
	tools/altvstime.cpp	
	tools/avtplotwidget.cpp
	tools/calendarwidget.cpp
	tools/conjunctions.cpp	
	tools/jmoontool.cpp
	tools/ksconjunct.cpp
        tools/eqplotwidget.cpp
        tools/astrocalc.cpp
        tools/modcalcangdist.cpp
        tools/modcalcapcoord.cpp
        tools/modcalcaltaz.cpp
        tools/modcalcdaylength.cpp
        tools/modcalceclipticcoords.cpp
        tools/modcalcvizequinox.cpp
        tools/modcalcgalcoord.cpp
        tools/modcalcgeodcoord.cpp
        tools/modcalcjd.cpp
        tools/modcalcplanets.cpp
        tools/modcalcsidtime.cpp
        tools/modcalcvlsr.cpp
        tools/observinglist.cpp
        tools/obslistpopupmenu.cpp
	tools/sessionsortfilterproxymodel.cpp
	tools/obslistwizard.cpp
	tools/planetviewer.cpp
	tools/pvplotwidget.cpp
	tools/scriptargwidgets.cpp
	tools/scriptbuilder.cpp
	tools/scriptfunction.cpp
	tools/skycalendar.cpp
	tools/wutdialog.cpp
        tools/flagmanager.cpp
        tools/horizonmanager.cpp

        #FIXME Port to QML2
        #tools/whatsinteresting/skyobjlistmodel.cpp
        #tools/whatsinteresting/wiview.cpp
        #tools/whatsinteresting/modelmanager.cpp
        #tools/whatsinteresting/skyobjitem.cpp
        #tools/whatsinteresting/wilpsettings.cpp
        #tools/whatsinteresting/wiequipsettings.cpp
        #tools/whatsinteresting/obsconditions.cpp
        #tools/whatsinteresting/skyobjdescription.cpp

        #FIXME Port to KF5
        #tools/moonphasetool.cpp
	tools/starhopper.cpp
	tools/eyepiecefield.cpp
        tools/starhopperdialog.cpp
	)

ki18n_wrap_ui(libkstarstools_SRCS
	tools/altvstime.ui
	tools/argchangeviewoption.ui
	tools/argexportimage.ui
	tools/argloadcolorscheme.ui
	tools/arglooktoward.ui
	tools/argfindobject.ui
	tools/argprintimage.ui
	tools/argsetaltaz.ui
	tools/argsetcolor.ui
	tools/argsetgeolocation.ui
	tools/argsetlocaltime.ui
	tools/argsetradec.ui
	tools/argsettrack.ui
	tools/argtimescale.ui
	tools/argwaitfor.ui
	tools/argwaitforkey.ui
	tools/argzoom.ui
	tools/conjunctions.ui
        tools/modcalcangdist.ui
        tools/modcalcapcoord.ui
        tools/modcalcaltaz.ui
        tools/modcalcdaylength.ui
        tools/modcalceclipticcoords.ui
        tools/modcalcvizequinox.ui
        tools/modcalcgalcoord.ui
        tools/modcalcgeod.ui
        tools/modcalcjd.ui
        tools/modcalcplanets.ui
        tools/modcalcsidtime.ui
        tools/modcalcvlsr.ui
	tools/observinglist.ui
	tools/obslistwizard.ui
	tools/optionstreeview.ui
	tools/planetviewer.ui
	tools/scriptbuilder.ui
	tools/scriptnamedialog.ui
	tools/skycalendar.ui
	tools/wutdialog.ui
	tools/flagmanager.ui
        tools/whatsinteresting/wilpsettings.ui
        tools/whatsinteresting/wiequipsettings.ui
        tools/starhopperdialog.ui
        tools/horizonmanager.ui
        )


set(libkstarscomponents_SRCS
   skycomponents/skylabeler.cpp
   skycomponents/highpmstarlist.cpp
   skycomponents/skymapcomposite.cpp
   skycomponents/skymesh.cpp
   skycomponents/linelistindex.cpp
   skycomponents/linelistlabel.cpp
   skycomponents/noprecessindex.cpp
   skycomponents/listcomponent.cpp
   skycomponents/pointlistcomponent.cpp
   skycomponents/solarsystemsinglecomponent.cpp
   skycomponents/solarsystemlistcomponent.cpp
   skycomponents/asteroidscomponent.cpp
   skycomponents/cometscomponent.cpp
   skycomponents/planetmoonscomponent.cpp
   skycomponents/solarsystemcomposite.cpp
   skycomponents/satellitescomponent.cpp
   skycomponents/starcomponent.cpp
   skycomponents/deepstarcomponent.cpp
   skycomponents/deepskycomponent.cpp
   skycomponents/catalogcomponent.cpp
   skycomponents/constellationartcomponent.cpp
   skycomponents/constellationboundarylines.cpp
   skycomponents/constellationlines.cpp
   skycomponents/constellationnamescomponent.cpp
   skycomponents/supernovaecomponent.cpp
   skycomponents/coordinategrid.cpp
   skycomponents/equatorialcoordinategrid.cpp
   skycomponents/horizontalcoordinategrid.cpp
   skycomponents/ecliptic.cpp
   skycomponents/equator.cpp
   skycomponents/artificialhorizoncomponent.cpp
   skycomponents/horizoncomponent.cpp
   skycomponents/milkyway.cpp
   skycomponents/skycomponent.cpp
   skycomponents/skycomposite.cpp
   skycomponents/starblock.cpp
   skycomponents/starblocklist.cpp
   skycomponents/starblockfactory.cpp
   skycomponents/culturelist.cpp
   skycomponents/flagcomponent.cpp
   skycomponents/targetlistcomponent.cpp
   skycomponents/notifyupdatesui.cpp
)

set(libkstarswidgets_SRCS
	widgets/clicklabel.cpp
	widgets/dmsbox.cpp
	widgets/draglistbox.cpp
	widgets/fovwidget.cpp
	widgets/logedit.cpp
	widgets/magnitudespinbox.cpp
	widgets/mapcanvas.cpp
	widgets/thumbimage.cpp
	widgets/timespinbox.cpp
	widgets/timestepbox.cpp
	widgets/timeunitbox.cpp
	widgets/infoboxwidget.cpp
#        widgets/genericcalendarwidget.cpp
#        widgets/moonphasecalendarwidget.cpp
	widgets/kshelplabel.cpp
    widgets/unitspinboxwidget.cpp
)

ki18n_wrap_ui(libkstarswidgets_SRCS
#  widgets/genericcalendarwidget.ui
    widgets/unitspinboxwidget.ui
)

set( kstars_KCFG_SRCS Options.kcfgc )

set(kstars_options_SRCS
  options/opsadvanced.cpp
  options/opscatalog.cpp
  options/opscolors.cpp
  options/opsguides.cpp
  options/opssolarsystem.cpp
  options/opssatellites.cpp
  options/opssupernovae.cpp
)

set(kstars_optionsui_SRCS
  options/opsadvanced.ui
  options/opscatalog.ui
  options/opscolors.ui
  options/opsguides.ui
  options/opssolarsystem.ui
  options/opssatellites.ui
  options/opssupernovae.ui
)

set(kstars_skyobjects_SRCS
  skyobjects/constellationsart.cpp
  skyobjects/deepskyobject.cpp
  skyobjects/jupitermoons.cpp
  skyobjects/planetmoons.cpp
  skyobjects/ksasteroid.cpp
  skyobjects/kscomet.cpp
  skyobjects/ksmoon.cpp
  skyobjects/ksplanetbase.cpp
  skyobjects/ksplanet.cpp
  #skyobjects/kspluto.cpp
  skyobjects/kssun.cpp
  skyobjects/skyline.cpp
  skyobjects/skyobject.cpp
  skyobjects/skypoint.cpp
  skyobjects/starobject.cpp
  skyobjects/trailobject.cpp
  skyobjects/satellite.cpp
  skyobjects/satellitegroup.cpp
  skyobjects/supernova.cpp
)

set(kstars_dialogs_SRCS
  dialogs/addcatdialog.cpp
  dialogs/addlinkdialog.cpp
  dialogs/detaildialog.cpp
  dialogs/finddialog.cpp
  dialogs/focusdialog.cpp
  dialogs/fovdialog.cpp
  dialogs/locationdialog.cpp
  dialogs/timedialog.cpp
  dialogs/exportimagedialog.cpp
)

set(kstars_dialogsui_SRCS
  dialogs/addcatdialog.ui
  dialogs/addlinkdialog.ui
  dialogs/details_database.ui
  dialogs/details_data.ui
  dialogs/details_data_comet.ui
  dialogs/details_links.ui
  dialogs/details_log.ui
  dialogs/details_position.ui
  dialogs/finddialog.ui
  dialogs/focusdialog.ui
  dialogs/fovdialog.ui
  dialogs/locationdialog.ui
  dialogs/wizwelcome.ui
  dialogs/wizlocation.ui
  dialogs/wizdownload.ui
  dialogs/newfov.ui
  dialogs/exportimagedialog.ui
)

set(kstars_projection_SRCS
    projections/projector.cpp
    projections/lambertprojector.cpp
    projections/gnomonicprojector.cpp
    projections/stereographicprojector.cpp
    projections/orthographicprojector.cpp
    projections/azimuthalequidistantprojector.cpp
    projections/equirectangularprojector.cpp
)

set(kstars_extra_SRCS
        auxiliary/colorscheme.cpp
        auxiliary/dms.cpp
        auxiliary/fov.cpp
        auxiliary/geolocation.cpp
        auxiliary/imageviewer.cpp
        auxiliary/ksfilereader.cpp
        auxiliary/ksuserdb.cpp
        auxiliary/thumbnailpicker.cpp
        auxiliary/thumbnaileditor.cpp
        auxiliary/binfilehelper.cpp
        auxiliary/imageexporter.cpp
        auxiliary/ksutils.cpp
        auxiliary/kswizard.cpp
        auxiliary/qcustomplot.cpp
        time/simclock.cpp
        time/kstarsdatetime.cpp
        time/timezonerule.cpp
        kstarsdbus.cpp
        ksnumbers.cpp
        kspopupmenu.cpp
        ksalmanac.cpp
        kstarsactions.cpp
        kstarsdata.cpp
        kstarsinit.cpp
        kstars.cpp
        kstarssplash.cpp
        skymap.cpp
        skymapdrawabstract.cpp
        skymapqdraw.cpp
        skymapevents.cpp
        skypainter.cpp
        skyqpainter.cpp
	texturemanager.cpp		
)

set(oal_SRCS
    oal/log.cpp
    oal/observer.cpp
    oal/site.cpp
    oal/session.cpp
    oal/scope.cpp
    oal/eyepiece.cpp
    oal/filter.cpp
    oal/observation.cpp
    oal/lens.cpp
    oal/equipmentwriter.cpp
    oal/observeradd.cpp
    oal/execute.cpp
)

set(printing_SRCS
    printing/detailstable.cpp
    printing/finderchart.cpp
    printing/foveditordialog.cpp
    printing/fovsnapshot.cpp
    printing/kstarsdocument.cpp
    printing/legend.cpp
    printing/loggingform.cpp
    printing/printingwizard.cpp
    printing/pwizchartconfig.cpp
    printing/pwizchartcontents.cpp
    printing/pwizfovbrowse.cpp
    printing/pwizfovconfig.cpp
    printing/pwizfovmanual.cpp
    printing/pwizfovsh.cpp
    printing/pwizfovtypeselection.cpp
    printing/pwizobjectselection.cpp
    printing/pwizprint.cpp
    printing/shfovexporter.cpp
    printing/simplefovexporter.cpp
)

set(printingui_SRCS
    printing/foveditordialog.ui
    printing/pwizchartconfig.ui
    printing/pwizchartcontents.ui
    printing/pwizfovbrowse.ui
    printing/pwizfovconfig.ui
    printing/pwizfovmanual.ui
    printing/pwizfovsh.ui
    printing/pwizfovtypeselection.ui
    printing/pwizobjectselection.ui
    printing/pwizprint.ui
    printing/pwizwelcome.ui
)

set(kstars_SRCS ${indi_SRCS} ${fits_SRCS} ${ekos_SRCS} ${onlineparser_SRCS}
	${libkstarswidgets_SRCS} ${libkstarscomponents_SRCS} ${libkstarstools_SRCS}
	${kstars_extra_SRCS}  ${kstars_gl_SRCS} ${kstars_projection_SRCS} ${xplanet_SRCS}
	${kstars_options_SRCS} ${kstars_skyobjects_SRCS} ${kstars_dialogs_SRCS} ${oal_SRCS}
        ${printing_SRCS}
)

qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.xml kstars.h KStars)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.SimClock.xml simclock.h SimClock)

if (INDI_FOUND)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.xml indi/indidbus.h INDIDBus)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.xml ekos/ekosmanager.h EkosManager)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Capture.xml ekos/capture.h Ekos::Capture)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Focus.xml ekos/focus.h Ekos::Focus)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Guide.xml ekos/guide.h Ekos::Guide)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Align.xml ekos/align.h Ekos::Align)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Mount.xml ekos/mount.h Ekos::Mount)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Dome.xml ekos/dome.h Ekos::Dome)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Weather.xml ekos/weather.h Ekos::Weather)
qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.DustCap.xml ekos/dustcap.h Ekos::DustCap)
endif(INDI_FOUND)

kconfig_add_kcfg_files(kstars_SRCS ${kstars_KCFG_SRCS})

ki18n_wrap_ui(kstars_SRCS
       ${indiui_SRCS} ${ui_SRCS} ${fitsui_SRCS} ${ekosui_SRCS} ${xplanetui_SRCS} ${kstars_optionsui_SRCS} ${kstars_dialogsui_SRCS}
       ${printingui_SRCS}
       auxiliary/thumbnailpicker.ui
       auxiliary/thumbnaileditor.ui
       oal/observeradd.ui
       oal/equipmentwriter.ui
       oal/execute.ui
       skycomponents/notifyupdatesui.ui
)

#FIXME We need to port this
#kde4_add_app_icon(kstars_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/hi*-app-kstars.png")
add_executable(kstars main.cpp)
add_library( KStarsLib STATIC ${kstars_SRCS})

# FIXME TODO
#target_include_directories(KStarsLib INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}/>")

target_link_libraries(KStarsLib
        LibKSDataHandlers
        htmesh
        KF5::I18n
        KF5::NewStuff
        KF5::KIOFileWidgets
        KF5::WidgetsAddons
        KF5::Plotting
#        KF5::TextEditor
#        KF5::DBusAddons
        KF5::IconThemes
        Qt5::Gui
        Qt5::PrintSupport
        Qt5::Sql
        Qt5::Svg
        #Qt5::Qml
        #Qt5::Quick
	${ZLIB_LIBRARIES}
        )

if(NOT WIN32)
  target_link_libraries(KStarsLib m)
endif(NOT WIN32)
if (CFITSIO_FOUND)
  target_link_libraries(KStarsLib ${CFITSIO_LIBRARIES})
endif (CFITSIO_FOUND)
if (INDI_FOUND)
  target_link_libraries(KStarsLib ${CMAKE_THREAD_LIBS_INIT}
  ${INDI_LIBRARIES} ${INDI_CLIENT_LIBRARIES}
  KF5::Notifications z)
endif (INDI_FOUND)
if (WCSLIB_FOUND)
    target_link_libraries(KStarsLib ${WCSLIB_LIBRARIES})
endif (WCSLIB_FOUND)
#FIXME Enable OpenGL Later
#if( OPENGL_FOUND )
#    target_link_libraries(KStarsLib
#    ${OPENGL_LIBRARIES}
#    ${QT_QTOPENGL_LIBRARY}
#    )
#endif( OPENGL_FOUND )

target_link_libraries(kstars KStarsLib)

install(TARGETS kstars ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############
ecm_install_icons(${KDE_INSTALL_ICONDIR})
install( PROGRAMS org.kde.kstars.desktop  DESTINATION  ${KDE_INSTALL_APPDIR} )
install( FILES kstars.kcfg  DESTINATION  ${KDE_INSTALL_KCFGDIR} )
install( FILES kstars.knsrc  DESTINATION  ${KDE_INSTALL_CONFDIR} )
if (INDI_FOUND)
install( FILES kstarsui-indi.rc DESTINATION  ${KDE_INSTALL_KXMLGUI5DIR}/kstars RENAME kstarsui.rc)
else (INDI_FOUND)
install( FILES kstarsui.rc DESTINATION  ${KDE_INSTALL_KXMLGUI5DIR}/kstars )
endif(INDI_FOUND)

install( FILES fitsviewer.rc  DESTINATION  ${KDE_INSTALL_KXMLGUI5DIR}/kstars )
if (WIN32)
  install( FILES kstarsui-win.rc  DESTINATION  ${KDE_INSTALL_KXMLGUI5DIR}/kstars )
endif (WIN32)
install(FILES kstars.notifyrc DESTINATION ${KNOTIFYRC_INSTALL_DIR})
