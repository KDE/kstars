add_subdirectory( skycomponents ) 
add_subdirectory( widgets ) 
add_subdirectory( indi ) 
add_subdirectory( tools ) 
add_subdirectory( data ) 
add_subdirectory( icons ) 
add_subdirectory( satlib ) 

include_directories( ${CMAKE_SOURCE_DIR}/kstars/kstars/tools 
	${CMAKE_SOURCE_DIR}/kstars/kstars/satlib 
	${CMAKE_SOURCE_DIR}/libkdeedu/extdate 
	${CMAKE_SOURCE_DIR}/libkdeedu/kdeeduplot
)


########### next target ###############
set(liblilxml_SRCS  ${CMAKE_SOURCE_DIR}/kstars/kstars/indi/lilxml.c )

set(libindicom_SRCS 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/indicom.c 
	${CMAKE_SOURCE_DIR}/kstars/kstars/indi/base64.c 
	)

if (NOT CFITSIO_FOUND)
  set (CFITSIO_LIBRARIES cfitsio)
endif(NOT CFITSIO_FOUND)

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
	widgets/calcframe.cpp 
	widgets/clicklabel.cpp 
	widgets/dmsbox.cpp 
	widgets/draglistbox.cpp 
	widgets/logedit.cpp
	widgets/magnitudespinbox.cpp 
	widgets/mapcanvas.cpp 
	widgets/thumbimage.cpp 
	widgets/timebox.cpp 
	widgets/timespinbox.cpp 
	widgets/timestepbox.cpp 
	widgets/timeunitbox.cpp
	widgets/fitshistogramdraw.cpp
)

set(kstars_SRCS ${liblilxml_SRCS} ${libindicom_SRCS} ${libcfitsio_SRCS} 
	${libkstarswidgets_SRCS} ${libkstarscomponents_SRCS} ${libkstarstools_SRCS} 
	addcatdialog.cpp addlinkdialog.cpp colorscheme.cpp csegment.cpp 
	deepskyobject.cpp detaildialog.cpp devicemanager.cpp dms.cpp finddialog.cpp 
	fitshistogram.cpp ccdpreviewwg.cpp fitsimage.cpp fitsviewer.cpp 
	focusdialog.cpp fov.cpp fovdialog.cpp geolocation.cpp imagesequence.cpp
	imageviewer.cpp indidevice.cpp indidriver.cpp indielement.cpp 
	indifitsconf.cpp indigroup.cpp indimenu.cpp indiproperty.cpp indistd.cpp 
	infobox.cpp infoboxes.cpp jupitermoons.cpp ksasteroid.cpp kscomet.cpp 
	ksfilereader.cpp ksmoon.cpp ksnewstuff.cpp ksnumbers.cpp ksplanet.cpp 
	ksplanetbase.cpp kspluto.cpp kspopupmenu.cpp kssun.cpp kstars.cpp 
	kstarsactions.cpp kstarsdata.cpp kstarsdatetime.cpp kstarsdcop.cpp kstarsinit.cpp 
	kstarssplash.cpp ksutils.cpp kswizard.cpp locationdialog.cpp main.cpp 
	opsadvanced.cpp opscatalog.cpp opscolors.cpp opsguides.cpp opssolarsystem.cpp 
	simclock.cpp skymap.cpp skymapdraw.cpp skymapevents.cpp 
	skyobject.cpp skypoint.cpp skyline.cpp starobject.cpp streamwg.cpp 
	telescopewizardprocess.cpp telescopeprop.cpp timedialog.cpp timezonerule.cpp 
	toggleaction.cpp thumbnailpicker.cpp thumbnaileditor.cpp quaternion.cpp
)

set( kstars_KCFG_SRCS Options.kcfgc )

kde4_add_kcfg_files(kstars_SRCS ${kstars_KCFG_SRCS})

kde4_automoc(${kstars_SRCS})

##DCOP_IS_DEAD
#kde4_add_dcop_skels(kstars_SRCS kstarsinterface.h simclockinterface.h )

kde4_add_ui_files(kstars_SRCS addcatdialog.ui addlinkdialog.ui ccdpreview.ui 
       details_data.ui details_position.ui details_links.ui details_database.ui details_log.ui 
       devmanager.ui finddialog.ui fitsheaderdialog.ui focusdialog.ui 
       fovdialog.ui newfov.ui locationdialog.ui 
       fitshistogramui.ui imgsequencedlg.ui indiconf.ui indihostconf.ui 
       wizwelcome.ui wizlocation.ui wizdevices.ui wizdownload.ui 
       opsadvanced.ui opscatalog.ui opscolors.ui opsguides.ui opssolarsystem.ui 
       statform.ui streamform.ui telescopewizard.ui telescopeprop.ui 
       thumbnailpicker.ui thumbnaileditor.ui 
)

kde4_add_executable(kstars ${kstars_SRCS})

target_link_libraries(kstars ${KDE4_KDECORE_LIBS} kdeeduplot kdeeduui extdate ${KDE4_KDEPRINT_LIBS} ${KDE4_KNEWSTUFF_LIBS} ${CFITSIO_LIBRARIES} SatLib m z )

install(TARGETS kstars  DESTINATION ${BIN_INSTALL_DIR} )


########### install files ###############

install( FILES kstars.desktop  DESTINATION  ${XDG_APPS_DIR} )
install( FILES kstars.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES kstarsrc  DESTINATION  ${CONFIG_INSTALL_DIR} )
install( FILES kstarsui.rc fitsviewer.rc  DESTINATION  ${DATA_INSTALL_DIR}/kstars/ )
if (WIN32)
  install( FILES kstarsui-win.rc  DESTINATION  ${DATA_INSTALL_DIR}/kstars/ )
endif (WIN32)
##DCOP_IS_DEAD
install( FILES  DESTINATION  ${INCLUDE_INSTALL_DIR} )

kde4_install_icons( ${ICON_INSTALL_DIR}   )


