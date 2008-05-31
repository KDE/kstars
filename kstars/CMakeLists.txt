add_subdirectory( skycomponents ) 
add_subdirectory( widgets ) 
add_subdirectory( indi ) 
add_subdirectory( tools ) 
add_subdirectory( data ) 
add_subdirectory( icons ) 
add_subdirectory( satlib ) 
add_subdirectory( htmesh ) 

Find_package(ZLIB REQUIRED)

if (CFITSIO_FOUND)
  set (fits_SRCS fitshistogram.cpp fitsimage.cpp fitsviewer.cpp)
  set (fitswidgets_SRCS widgets/fitshistogramdraw.cpp)
endif(CFITSIO_FOUND)

include_directories(
    ${CMAKE_SOURCE_DIR}/kstars/kstars/tools 
	${CMAKE_SOURCE_DIR}/kstars/kstars/satlib 
#    ${CMAKE_SOURCE_DIR}/kstars/kstars/htmesh 
#	${CMAKE_SOURCE_DIR}/libkdeedu/extdate 
	${ZLIB_INCLUDE_DIR}
)
if (CFITSIO_FOUND)
  include_directories(${CFITSIO_INCLUDE_DIR})
endif (CFITSIO_FOUND)

if (NOVA_FOUND)
    include_directories(${NOVA_INCLUDE_DIR})
endif (NOVA_FOUND)

add_definitions (-DQT3_SUPPORT -DQT3_SUPPORT_WARNINGS)

########### next target ###############
set(liblilxml_SRCS  ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/lilxml.c )

set(libindicom_SRCS 
    ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/libs/indicom.c
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/base64.c 
	)

set(libkstarstools_SRCS 
	tools/altvstime.cpp
	tools/astrocalc.cpp
	tools/avtplotwidget.cpp
	tools/conjunctions.cpp
	tools/eqplotwidget.cpp
	tools/jmoontool.cpp
	tools/ksconjunct.cpp
	tools/lcgenerator.cpp
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
	tools/argfindobject.ui
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
	tools/conjunctions.ui
	tools/lcgenerator.ui
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
	tools/wutdialog.ui )


set(libkstarscomponents_SRCS 
   skycomponents/skylabeler.cpp 
   skycomponents/highpmstarlist.cpp 
   skycomponents/skymapcomposite.cpp 
   skycomponents/skymesh.cpp
   skycomponents/linelistindex.cpp
   skycomponents/linelistlabel.cpp
   skycomponents/noprecessindex.cpp
   skycomponents/skiplistindex.cpp
   skycomponents/polylistindex.cpp
   skycomponents/singlecomponent.cpp 
   skycomponents/listcomponent.cpp 
   skycomponents/pointlistcomponent.cpp 
   skycomponents/linelistcomponent.cpp 
   skycomponents/solarsystemsinglecomponent.cpp 
   skycomponents/solarsystemlistcomponent.cpp 
   skycomponents/asteroidscomponent.cpp 
   skycomponents/cometscomponent.cpp 
   skycomponents/jupitermoonscomponent.cpp 
   skycomponents/solarsystemcomposite.cpp 
   skycomponents/satellitecomposite.cpp
   skycomponents/satellitecomponent.cpp
   skycomponents/starcomponent.cpp 
   skycomponents/deepskycomponent.cpp 
   skycomponents/customcatalogcomponent.cpp 
   skycomponents/constellationboundary.cpp 
   skycomponents/constellationboundarylines.cpp 
   skycomponents/constellationlines.cpp 
   skycomponents/constellationnamescomponent.cpp 
   skycomponents/coordinategrid.cpp 
   skycomponents/ecliptic.cpp 
   skycomponents/equator.cpp 
   skycomponents/horizoncomponent.cpp 
   skycomponents/milkyway.cpp 
   skycomponents/skycomponent.cpp 
   skycomponents/skycomposite.cpp 
)

set(libkstarswidgets_SRCS ${fitswidgets_SRCS}
	widgets/calcframe.cpp 
	widgets/clicklabel.cpp 
	widgets/dmsbox.cpp 
	widgets/draglistbox.cpp 
	widgets/fovwidget.cpp
	widgets/logedit.cpp
	widgets/magnitudespinbox.cpp 
	widgets/mapcanvas.cpp 
	widgets/thumbimage.cpp 
	widgets/timebox.cpp 
	widgets/timespinbox.cpp 
	widgets/timestepbox.cpp 
	widgets/timeunitbox.cpp
)

set( kstars_KCFG_SRCS Options.kcfgc )

# remove me once we required cmake 2.6
if (WIN32 AND CMAKE_MAJOR_VERSION MATCHES "2" AND CMAKE_MINOR_VERSION MATCHES "4")
  set(KSTARS_CMAKE_HACK 1)
endif (WIN32 AND CMAKE_MAJOR_VERSION MATCHES "2" AND CMAKE_MINOR_VERSION MATCHES "4")
if (KSTARS_CMAKE_HACK)
    # workaround for win32 command line to long problem
    # build intermediate libraries 
    kde4_add_ui_files(libkstarscomponents_SRCS addcatdialog.ui addlinkdialog.ui details_data.ui
       details_position.ui details_links.ui details_database.ui details_log.ui 
       devmanager.ui finddialog.ui fitsheaderdialog.ui focusdialog.ui 
       fovdialog.ui newfov.ui locationdialog.ui 
       fitshistogramui.ui imgsequencedlg.ui indiconf.ui indihostconf.ui 
       wizwelcome.ui wizlocation.ui wizdownload.ui opsadvanced.ui
       opscatalog.ui opscolors.ui opsguides.ui opssolarsystem.ui 
       statform.ui streamform.ui telescopewizard.ui telescopeprop.ui 
       thumbnailpicker.ui thumbnaileditor.ui 
)

    kde4_add_kcfg_files(libkstarscomponents_SRCS ${kstars_KCFG_SRCS})

    kde4_add_library(kstarscomponents ${libkstarscomponents_SRCS} ${libkstarstools_SRCS})
    target_link_libraries (kstarscomponents)
    set (libkstarscomponents_SRCS)

endif (KSTARS_CMAKE_HACK)

set(kstars_extra_SRCS
	addcatdialog.cpp addlinkdialog.cpp colorscheme.cpp 
	deepskyobject.cpp detaildialog.cpp devicemanager.cpp dms.cpp finddialog.cpp 
	focusdialog.cpp fov.cpp fovdialog.cpp geolocation.cpp imagesequence.cpp 
	imageviewer.cpp indidevice.cpp indidriver.cpp indielement.cpp 
	indifitsconf.cpp indigroup.cpp indimenu.cpp indiproperty.cpp indistd.cpp 
	infobox.cpp infoboxes.cpp jupitermoons.cpp ksasteroid.cpp kscomet.cpp 
	ksfilereader.cpp ksmoon.cpp ksnumbers.cpp ksplanet.cpp 
	ksplanetbase.cpp kspluto.cpp kspopupmenu.cpp kssun.cpp kstars.cpp 
	kstarsactions.cpp kstarsdata.cpp kstarsdatetime.cpp kstarsdcop.cpp kstarsinit.cpp 
	kstarssplash.cpp ksutils.cpp kswizard.cpp locationdialog.cpp main.cpp 
	opsadvanced.cpp opscatalog.cpp opscolors.cpp opsguides.cpp opssolarsystem.cpp 
	simclock.cpp skymap.cpp skymapdraw.cpp skymapevents.cpp trailobject.cpp
	skyobject.cpp skypoint.cpp skyline.cpp starobject.cpp streamwg.cpp 
	telescopewizardprocess.cpp telescopeprop.cpp timedialog.cpp timezonerule.cpp 
	thumbnailpicker.cpp thumbnaileditor.cpp quaternion.cpp binfilehelper.cpp
)

set(kstars_SRCS ${liblilxml_SRCS} ${libindicom_SRCS} ${fits_SRCS} 
	${libkstarswidgets_SRCS} ${libkstarscomponents_SRCS} ${libkstarstools_SRCS} ${kstars_extra_SRCS}
)
qt4_add_dbus_adaptor(kstars_SRCS org.kde.kstars.xml kstars.h KStars)
qt4_add_dbus_adaptor(kstars_SRCS org.kde.kstars.SimClock.xml simclock.h SimClock)

kde4_add_kcfg_files(kstars_SRCS ${kstars_KCFG_SRCS})

##DCOP_IS_DEAD
#kde4_add_dcop_skels(kstars_SRCS kstarsinterface.h simclockinterface.h )

kde4_add_ui_files(kstars_SRCS addcatdialog.ui addlinkdialog.ui details_data.ui
       details_position.ui details_links.ui details_database.ui details_log.ui 
       devmanager.ui finddialog.ui fitsheaderdialog.ui focusdialog.ui 
       fovdialog.ui newfov.ui locationdialog.ui 
       fitshistogramui.ui imgsequencedlg.ui indiconf.ui indihostconf.ui 
       wizwelcome.ui wizlocation.ui wizdownload.ui opsadvanced.ui
       opscatalog.ui opscolors.ui opsguides.ui opssolarsystem.ui 
       statform.ui streamform.ui telescopewizard.ui telescopeprop.ui 
       thumbnailpicker.ui thumbnaileditor.ui 
)

#kde4_add_app_icon(kstars_SRCS "${KDE4_ICON_DIR}/oxygen/*/apps/kstars.png")
#kde4_add_app_icon(kstars_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/ox*-apps-kstars.png")
kde4_add_app_icon(kstars_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/hi*-app-kstars.png")

kde4_add_executable(kstars ${kstars_SRCS})

target_link_libraries(kstars
    ${KDE4_KDECORE_LIBS}
    kdeeduui
#	extdate
	${KDE4_KDEPRINT_LIBS}
	${KDE4_KNEWSTUFF2_LIBS}
	${KDE4_KDE3SUPPORT_LIBS}
	SatLib
	htmesh
	${ZLIB_LIBRARIES}
        )

if(KSTARS_CMAKE_HACK)
target_link_libraries(kstars kstarscomponents htmesh SatLib)
endif(KSTARS_CMAKE_HACK)

if(NOT WIN32)
  target_link_libraries(kstars m)
endif(NOT WIN32)
if (CFITSIO_FOUND)
  target_link_libraries(kstars ${CFITSIO_LIBRARIES})
endif (CFITSIO_FOUND)

if (NOVA_FOUND)
  target_link_libraries(kstars ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS kstars ${INSTALL_TARGETS_DEFAULT_ARGS})


########### install files ###############
install( FILES kstars.desktop  DESTINATION  ${XDG_APPS_INSTALL_DIR} )
install( FILES kstars.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES kstars.knsrc  DESTINATION  ${CONFIG_INSTALL_DIR} )
install( FILES kstarsui.rc fitsviewer.rc  DESTINATION  ${DATA_INSTALL_DIR}/kstars/ )
if (WIN32)
  install( FILES kstarsui-win.rc  DESTINATION  ${DATA_INSTALL_DIR}/kstars/ )
endif (WIN32)

kde4_install_icons( ${ICON_INSTALL_DIR}   )


