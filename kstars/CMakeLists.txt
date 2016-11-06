add_subdirectory( data )
add_subdirectory( icons )
add_subdirectory( htmesh )

if (${KF5_VERSION} VERSION_EQUAL 5.18.0 OR ${KF5_VERSION} VERSION_GREATER 5.18.0)
    SET(HAVE_KF5WIT 1)
    if(NOT BUILD_KSTARS_LITE)
        add_subdirectory( tools/whatsinteresting/qml)
    endif(NOT BUILD_KSTARS_LITE)
else()
    SET(HAVE_KF5WIT 0)
endif()

Find_package(ZLIB REQUIRED)
Find_package(Threads REQUIRED)

if(MSVC)
    add_definitions(-D_USE_MATH_DEFINES=1)
    add_definitions(-DNOMINMAX)
endif()

include_directories(
    ${kstars_SOURCE_DIR}/kstars/skyobjects
    ${kstars_SOURCE_DIR}/kstars/skycomponents
    ${kstars_SOURCE_DIR}/kstars/auxiliary
    ${kstars_SOURCE_DIR}/kstars/time
)

if(BUILD_KSTARS_LITE)
    include_directories(
        ${kstars_SOURCE_DIR}/kstars/triangle
    )
else(BUILD_KSTARS_LITE)
    include_directories(
        ${kstars_SOURCE_DIR}/kstars/tools
    )
endif(BUILD_KSTARS_LITE)

if(NOT BUILD_KSTARS_LITE)
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
endif(NOT BUILD_KSTARS_LITE)

if (INDI_FOUND)
    if(BUILD_KSTARS_LITE)
            set (fits_SRCS
                fitsviewer/fitsdata.cpp
                fitsviewer/bayer.c
                )
                include_directories(${CFITSIO_INCLUDE_DIR})
                include_directories(${NOVA_INCLUDE_DIR})

            set (indi_SRCS
                indi/clientmanagerlite.cpp
                indi/inditelescopelite.cpp
                kstarslite/skyitems/skynodes/crosshairnode.cpp
                kstarslite/skyitems/telescopesymbolsitem.cpp
            )
    else(BUILD_KSTARS_LITE)
            set(indiui_SRCS
                indi/streamform.ui
                indi/drivermanager.ui
                indi/opsindi.ui
                indi/indihostconf.ui
                indi/telescopewizard.ui
            )

            set(indi_SRCS
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
                indi/indiwebmanager.cpp
            )

            if (CFITSIO_FOUND)
                set(ekosui_SRCS
                    ekos/opsekos.ui
                    ekos/ekosmanager.ui
                    ekos/profileeditor.ui
                    # Scheduler
                    ekos/scheduler/scheduler.ui
                    ekos/scheduler/mosaic.ui
                    # Capture
                    ekos/capture/capture.ui
                    ekos/capture/calibrationoptions.ui
                    # Align
                    ekos/align/align.ui
                    # Focus
                    ekos/focus/focus.ui
                    # Mount
                    ekos/mount/mount.ui
                    # Guide
                    ekos/guide/guide.ui
                    ekos/guide/opscalibration.ui
                    ekos/guide/opsguide.ui
                    #TODO remove from GIT
                    #ekos/guide/guider.ui
                    #ekos/guide/rcalibration.ui
                )

            set(ekos_SRCS
                       ekos/ekos.cpp
                       ekos/ekosmanager.cpp
                       ekos/profileeditor.cpp
                       ekos/opsekos.cpp

                       # Auxiliary
                       ekos/auxiliary/QProgressIndicator.cpp
                       ekos/auxiliary/dome.cpp
                       ekos/auxiliary/weather.cpp
                       ekos/auxiliary/dustcap.cpp
                       ekos/auxiliary/darklibrary.cpp

                       # Capture
                       ekos/capture/capture.cpp
                       ekos/capture/sequencejob.cpp

                       # Scheduler
                       ekos/scheduler/schedulerjob.cpp
                       ekos/scheduler/scheduler.cpp
                       ekos/scheduler/mosaic.cpp

                       # Focus
                       ekos/focus/focus.cpp

                       # Mount
                       ekos/mount/mount.cpp

                       # Align
                       ekos/align/align.cpp
                       ekos/align/astrometryparser.cpp
                       ekos/align/offlineastrometryparser.cpp
                       ekos/align/onlineastrometryparser.cpp
                       ekos/align/remoteastrometryparser.cpp

                       # Guide
                       ekos/guide/guide.cpp
                       ekos/guide/guideinterface.cpp                       
                       ekos/guide/opscalibration.cpp
                       ekos/guide/opsguide.cpp
                       # Internal Guide                       
                       ekos/guide/internalguide/gmath.cpp
                       ekos/guide/internalguide/internalguider.cpp
                       #ekos/guide/internalguide/guider.cpp
                       ekos/guide/internalguide/matr.cpp
                       #ekos/guide/internalguide/rcalibration.cpp
                       ekos/guide/internalguide/vect.cpp
                       # External Guide
                       ekos/guide/externalguide/phd2.cpp
                       ekos/guide/externalguide/linguider.cpp
           )
            endif(CFITSIO_FOUND)
    endif(BUILD_KSTARS_LITE)

    include_directories(${INDI_INCLUDE_DIR})
endif (INDI_FOUND)

if(NOT BUILD_KSTARS_LITE)
    if(WCSLIB_FOUND)
      include_directories( ${WCSLIB_INCLUDE_DIR} )
    endif(WCSLIB_FOUND)

    if(XPLANET_FOUND)
        set(xplanet_SRCS
            xplanet/opsxplanet.cpp
        )

        set(xplanetui_SRCS
            xplanet/opsxplanet.ui
        )
    endif(XPLANET_FOUND)

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
        tools/nameresolver.cpp

        #FIXME Port to KF5
        #tools/moonphasetool.cpp

        tools/starhopper.cpp
        tools/eyepiecefield.cpp
        tools/exporteyepieceview.cpp
        tools/starhopperdialog.cpp
        tools/adddeepskyobject.cpp
    )

    if(${KF5_VERSION} VERSION_EQUAL 5.18.0 OR ${KF5_VERSION} VERSION_GREATER 5.18.0)
        set(libkstarstools_SRCS
            ${libkstarstools_SRCS}
            tools/whatsinteresting/skyobjlistmodel.cpp
            tools/whatsinteresting/wiview.cpp
            tools/whatsinteresting/modelmanager.cpp
            tools/whatsinteresting/skyobjitem.cpp
            tools/whatsinteresting/wilpsettings.cpp
            tools/whatsinteresting/wiequipsettings.cpp
            tools/whatsinteresting/obsconditions.cpp
            tools/whatsinteresting/skyobjdescription.cpp
        )
    endif()

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
        tools/starhopperdialog.ui
        tools/horizonmanager.ui
        tools/adddeepskyobject.ui
    )

    if (${KF5_VERSION} VERSION_EQUAL 5.18.0 OR ${KF5_VERSION} VERSION_GREATER 5.18.0)
        ki18n_wrap_ui(libkstarstools_SRCS
            tools/whatsinteresting/wilpsettings.ui
            tools/whatsinteresting/wiequipsettings.ui
        )
    endif()

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
    #    widgets/genericcalendarwidget.cpp
    #    widgets/moonphasecalendarwidget.cpp
        widgets/kshelplabel.cpp
        widgets/unitspinboxwidget.cpp
    )

    ki18n_wrap_ui(libkstarswidgets_SRCS
    #    widgets/genericcalendarwidget.ui
        widgets/unitspinboxwidget.ui
    )

    set(kstars_KCFG_SRCS Options.kcfgc)

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
endif(NOT BUILD_KSTARS_LITE)

set( kstars_KCFG_SRCS Options.kcfgc )
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
    skycomponents/syncedcatalogcomponent.cpp
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
    )

if(NOT BUILD_KSTARS_LITE)
    LIST(APPEND libkstarscomponents_SRCS
        skycomponents/notifyupdatesui.cpp
        )
else(NOT BUILD_KSTARS_LITE)
    set(libkstarstools_SRCS
        tools/nameresolver.cpp
        )
endif(NOT BUILD_KSTARS_LITE)

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
    auxiliary/cachingdms.cpp
    auxiliary/geolocation.cpp
    auxiliary/ksfilereader.cpp
    auxiliary/ksuserdb.cpp
    auxiliary/binfilehelper.cpp
    auxiliary/ksutils.cpp
    auxiliary/ksdssimage.cpp
    auxiliary/ksdssdownloader.cpp
    auxiliary/profileinfo.cpp
    auxiliary/filedownloader.cpp
    auxiliary/kspaths.cpp
    auxiliary/QRoundProgressBar.cpp
    auxiliary/skyobjectlistmodel.cpp
    auxiliary/ksnotification.cpp
    time/simclock.cpp
    time/kstarsdatetime.cpp
    time/timezonerule.cpp
    ksnumbers.cpp
    kstarsdata.cpp
    texturemanager.cpp
    #to minimize number of indef KSTARS_LITE
    skypainter.cpp
    )

if(NOT BUILD_KSTARS_LITE)
    LIST(APPEND kstars_extra_SRCS
        auxiliary/imageviewer.cpp
        auxiliary/fov.cpp
        auxiliary/thumbnailpicker.cpp
        auxiliary/thumbnaileditor.cpp
        auxiliary/imageexporter.cpp
        auxiliary/kswizard.cpp
        auxiliary/qcustomplot.cpp
        kstarsdbus.cpp
        kspopupmenu.cpp
        ksalmanac.cpp
        kstarsactions.cpp
        kstarsinit.cpp
        kstars.cpp
        kstarssplash.cpp
        skymap.cpp
        skymapdrawabstract.cpp
        skymapqdraw.cpp
        skymapevents.cpp
        skyqpainter.cpp
        )
endif(NOT BUILD_KSTARS_LITE)

if(BUILD_KSTARS_LITE)
    #Temporary solution to allow use of qml files from source dir DELETE
    add_definitions( -DSOURCE_DIR=\"${kstars_SOURCE_DIR}\" )
    add_definitions(-DQML_IMPORT="${CMAKE_CURRENT_SOURCE_DIR}")

    set(kstarslite_SRCS
        kstarslite.cpp
        kstarsliteinit.cpp
        skymaplite.cpp
        skymapliteevents.cpp
        #Wrappers
        kstarslite/skypointlite.cpp
        kstarslite/skyobjectlite.cpp
        #ImageProvider
        kstarslite/imageprovider.cpp
        #Dialogs
        kstarslite/dialogs/detaildialoglite.cpp
        kstarslite/dialogs/finddialoglite.cpp
        kstarslite/dialogs/locationdialoglite.cpp
        #RootNode
        kstarslite/skyitems/rootnode.cpp
        kstarslite/skyitems/skyopacitynode.cpp
        kstarslite/skyitems/typedeflite.h
        #SkyItems
        kstarslite/skyitems/skyitem.cpp
        kstarslite/skyitems/planetsitem.cpp
        kstarslite/skyitems/asteroidsitem.cpp
        kstarslite/skyitems/cometsitem.cpp
        kstarslite/skyitems/horizonitem.cpp
        kstarslite/skyitems/labelsitem.cpp
        kstarslite/skyitems/constellationnamesitem.cpp
        kstarslite/skyitems/staritem.cpp
        kstarslite/skyitems/deepstaritem.cpp
        kstarslite/skyitems/deepskyitem.cpp
        kstarslite/skyitems/constellationartitem.cpp
        kstarslite/skyitems/satellitesitem.cpp
        kstarslite/skyitems/supernovaeitem.cpp
        kstarslite/skyitems/fovitem.cpp
        kstarslite/skyitems/syncedcatalogitem.cpp
        #Line
        kstarslite/skyitems/lines/linesitem.cpp
        kstarslite/skyitems/lines/equatoritem.cpp
        kstarslite/skyitems/lines/eclipticitem.cpp
        kstarslite/skyitems/lines/milkywayitem.cpp
        #SkyNodes
        kstarslite/skyitems/skynodes/planetnode.cpp
        kstarslite/skyitems/skynodes/skynode.cpp
        kstarslite/skyitems/skynodes/pointsourcenode.cpp
        kstarslite/skyitems/skynodes/planetmoonsnode.cpp
        kstarslite/skyitems/skynodes/horizonnode.cpp
        kstarslite/skyitems/skynodes/labelnode.cpp
        kstarslite/skyitems/skynodes/guidelabelnode.cpp
        kstarslite/skyitems/skynodes/deepskynode.cpp
        kstarslite/skyitems/skynodes/dsosymbolnode.cpp
        kstarslite/skyitems/skynodes/skypolygonnode.cpp
        kstarslite/skyitems/skynodes/constellationartnode.cpp
        kstarslite/skyitems/skynodes/satellitenode.cpp
        kstarslite/skyitems/skynodes/supernovanode.cpp
        kstarslite/skyitems/skynodes/trixelnode.cpp
        kstarslite/skyitems/skynodes/fovsymbolnode.cpp
        #Nodes
        kstarslite/skyitems/skynodes/nodes/pointnode.cpp
        kstarslite/skyitems/skynodes/nodes/polynode.cpp
        kstarslite/skyitems/skynodes/nodes/linenode.cpp
        kstarslite/skyitems/skynodes/nodes/ellipsenode.cpp
        kstarslite/skyitems/skynodes/nodes/rectnode.cpp
        #Material
        #kstarslite/skyitems/skynodes/material/dashedshader.cpp
        #libtess
        libtess/dict.c
        libtess/geom.c
        libtess/gluos.h
        libtess/memalloc.c
        libtess/mesh.c
        libtess/normal.c
        libtess/priorityq-sort.h
        libtess/priorityq.c
        libtess/render.c
        libtess/sweep.c
        libtess/tess.c
        libtess/tessmono.c
        libtess/priorityq-heap.c
        libtess/dict-list.h
        libtess/glu.h
        libtess/tessellate.c
        )
    #Qml files will be probably moved to user's data dir, but for use
    #with QtCreator it is more convenient to have them here
    set(kstarsliteqml_SRCS
        kstarslite/qml/main.qml
        kstarslite/qml/constants/Constants.qml
        kstarslite/qml/modules/SkyMapLiteWrapper.qml
        kstarslite/qml/modules/BottomMenu.qml
        kstarslite/qml/modules/KSPage.qml
        kstarslite/qml/modules/KSListView.qml
        kstarslite/qml/modules/KSLabel.qml
        kstarslite/qml/modules/KSText.qml
        kstarslite/qml/modules/KSTabButton.qml
        kstarslite/qml/modules/KSTab.qml
        kstarslite/qml/modules/KSTextField.qml
        kstarslite/qml/modules/KSButton.qml
        kstarslite/qml/modules/TopMenu.qml
        kstarslite/qml/modules/helpers/TopMenuButton.qml
        kstarslite/qml/modules/helpers/BottomMenuButton.qml
        kstarslite/qml/modules/Splash.qml
        kstarslite/qml/modules/helpers/TimeSpinBox.qml
        kstarslite/qml/modules/TimePage.qml
        #Popups
        kstarslite/qml/modules/popups/ProjectionsPopup.qml
        kstarslite/qml/modules/popups/FOVPopup.qml
        kstarslite/qml/modules/popups/ColorSchemePopup.qml
        #Menus
        kstarslite/qml/modules/menus/ContextMenu.qml
        #Helpers
        kstarslite/qml/modules/helpers/PassiveNotification.qml
        kstarslite/qml/modules/helpers/KSMenuItem.qml
        kstarslite/qml/modules/helpers/TelescopeControl.qml
        #Dialogs
        kstarslite/qml/dialogs/FindDialog.qml
        kstarslite/qml/dialogs/LocationDialog.qml
        kstarslite/qml/dialogs/DetailsDialog.qml

        kstarslite/qml/dialogs/helpers/DetailsItem.qml
        kstarslite/qml/dialogs/helpers/DetailsAddLink.qml
        kstarslite/qml/dialogs/helpers/LocationEdit.qml
        kstarslite/qml/dialogs/helpers/LocationLoading.qml

        kstarslite/qml/dialogs/menus/DetailsLinkMenu.qml
        kstarslite/qml/dialogs/menus/LocationsGeoMenu.qml
        #INDI
        kstarslite/qml/indi/INDIControlPanel.qml
        kstarslite/qml/indi/DevicePanel.qml
        kstarslite/qml/indi/ImagePreview.qml
        kstarslite/qml/indi/modules/MotionControl.qml
        kstarslite/qml/indi/modules/Led.qml
        kstarslite/qml/indi/modules/KSLed.qml
        kstarslite/qml/indi/modules/Property.qml
        kstarslite/qml/indi/modules/KSComboBox.qml
        kstarslite/qml/indi/modules/KSButtonSwitch.qml
        kstarslite/qml/indi/modules/KSCheckBox.qml
        kstarslite/qml/indi/modules/KSINDIText.qml
        kstarslite/qml/indi/modules/KSINDITextField.qml
        kstarslite/qml/indi/modules/KSButtonsSwitchRow.qml
        #Tutorial
        kstarslite/qml/modules/tutorial/TutorialPopup.qml
        kstarslite/qml/modules/tutorial/TutorialExitPopup.qml
        kstarslite/qml/modules/tutorial/TutorialStep1.qml
        kstarslite/qml/modules/tutorial/TutorialStep2.qml
        kstarslite/qml/modules/tutorial/TutorialStep3.qml
        kstarslite/qml/modules/tutorial/TutorialStep4.qml
        kstarslite/qml/modules/tutorial/TutorialStep5.qml
        kstarslite/qml/modules/tutorial/TutorialPane.qml
        )
    add_subdirectory(kstarslite/qml)
    ADD_CUSTOM_TARGET(kstarsliteqml SOURCES ${kstarsliteqml_SRCS})

    if(ANDROID)
        add_subdirectory(kstarslite/res)
    endif(ANDROID)

endif(BUILD_KSTARS_LITE)

set(kstars_SRCS
    ${indi_SRCS}
    ${fits_SRCS}
    ${ekos_SRCS}
    ${onlineparser_SRCS}
    ${libkstarswidgets_SRCS}
    ${libkstarscomponents_SRCS}
    ${libkstarstools_SRCS}
    ${kstars_extra_SRCS}
    ${kstars_gl_SRCS}
    ${kstars_projection_SRCS}
    ${xplanet_SRCS}
    ${kstars_options_SRCS}
    ${kstars_skyobjects_SRCS}
    ${kstars_dialogs_SRCS}
    ${oal_SRCS}
    ${printing_SRCS}
    #KStars Lite
    ${kstarslite_SRCS}
    ${indi_lite_SRCS}
)

kconfig_add_kcfg_files(kstars_SRCS ${kstars_KCFG_SRCS})

if(NOT BUILD_KSTARS_LITE)
    qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.xml kstars.h KStars)
    qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.SimClock.xml simclock.h SimClock)

    if (INDI_FOUND)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.xml indi/indidbus.h INDIDBus)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.xml ekos/ekosmanager.h EkosManager)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Capture.xml ekos/capture/capture.h Ekos::Capture)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Focus.xml ekos/focus/focus.h Ekos::Focus)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Guide.xml ekos/guide/guide.h Ekos::Guide)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Align.xml ekos/align/align.h Ekos::Align)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Mount.xml ekos/mount/mount.h Ekos::Mount)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Dome.xml ekos/auxiliary/dome.h Ekos::Dome)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Weather.xml ekos/auxiliary/weather.h Ekos::Weather)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.DustCap.xml ekos/auxiliary/dustcap.h Ekos::DustCap)
        qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Scheduler.xml ekos/scheduler/scheduler.h Ekos::Scheduler)
    endif(INDI_FOUND)

    ki18n_wrap_ui(kstars_SRCS
        ${indiui_SRCS}
        ${ui_SRCS}
        ${fitsui_SRCS}
        ${ekosui_SRCS}
        ${xplanetui_SRCS}
        ${kstars_optionsui_SRCS}
        ${kstars_dialogsui_SRCS}
        ${printingui_SRCS}
        auxiliary/thumbnailpicker.ui
        auxiliary/thumbnaileditor.ui
        oal/observeradd.ui
        oal/equipmentwriter.ui
        oal/execute.ui
        skycomponents/notifyupdatesui.ui
    )
endif(NOT BUILD_KSTARS_LITE)

add_library(KStarsLib STATIC ${kstars_SRCS})

if(BUILD_KSTARS_LITE)
    target_link_libraries(KStarsLib
        LibKSDataHandlers
        htmesh
        KF5::I18n
        KF5::Plotting
        KF5::ConfigGui
        Qt5::Gui
        Qt5::Sql
        Qt5::Qml
        Qt5::Quick
        Qt5::QuickControls2
        Qt5::Positioning
        Qt5::Concurrent
        ${ZLIB_LIBRARIES}
        )
    if(INDI_FOUND)
        if(ANDROID)
            #RAWExtractor is needed for converting RAW photos to JPEG in INDI Lite
            add_library(RAWExtractor SHARED IMPORTED)
            include_directories(${BUILD_KSTARSLITE_DIR}/include/libraw)
            set_property(TARGET RAWExtractor PROPERTY IMPORTED_LOCATION ${BUILD_KSTARSLITE_DIR}/android_libs/${ANDROID_ARCHITECTURE}/libRAWExtractor.so)
            target_link_libraries(KStarsLib RAWExtractor)
        endif(ANDROID)
    endif(INDI_FOUND)

else(BUILD_KSTARS_LITE)
    target_link_libraries(KStarsLib
        LibKSDataHandlers
        htmesh
        KF5::Crash
        KF5::I18n
        KF5::NewStuff
        KF5::KIOFileWidgets
        KF5::WidgetsAddons
        KF5::Plotting
        Qt5::Gui
        Qt5::PrintSupport
        Qt5::Sql
        Qt5::Svg
        Qt5::Qml
        Qt5::Quick
        Qt5::Network
        Qt5::Concurrent
        ${ZLIB_LIBRARIES}
        )
endif(BUILD_KSTARS_LITE)

if(NOT WIN32)
    target_link_libraries(KStarsLib m)
endif(NOT WIN32)

if (CFITSIO_FOUND)
    target_link_libraries(KStarsLib ${CFITSIO_LIBRARIES})
endif(CFITSIO_FOUND)

if(INDI_FOUND)
    if(BUILD_KSTARS_LITE)
#        find_package(Nova REQUIRED)
#        target_link_libraries(KStarsLib ${CMAKE_THREAD_LIBS_INIT} ${NOVA_LIBRARIES})
        target_link_libraries(KStarsLib ${CMAKE_THREAD_LIBS_INIT} )
    else(BUILD_KSTARS_LITE)
        target_link_libraries(KStarsLib ${CMAKE_THREAD_LIBS_INIT} KF5::Notifications)
    endif(BUILD_KSTARS_LITE)

    if(WIN32 OR ANDROID)
        add_definitions(-DUSE_QT5_INDI)
        find_package(Nova REQUIRED)
        if(ANDROID)
            target_link_libraries(KStarsLib ${NOVA_LIBRARIES} ${INDI_CLIENT_ANDROID_LIBRARIES})
        else(ANDROID)
            target_link_libraries(KStarsLib ${NOVA_LIBRARIES} ${INDI_LIBRARIES} ${INDI_CLIENT_QT_LIBRARIES})
        endif(ANDROID)
    else(WIN32 OR ANDROID)
        # If INDI Qt5 Client is found, use it.
        # Do not use Qt5 client on Linux for now (But KStars Lite needs it - polaris 31.10.2016)
        if(BUILD_KSTARS_LITE)
            add_definitions(-DUSE_QT5_INDI)
            target_link_libraries(KStarsLib ${INDI_LIBRARIES} ${INDI_CLIENT_QT_LIBRARIES} z)
        else(BUILD_KSTARS_LITE)
            target_link_libraries(KStarsLib ${INDI_LIBRARIES} ${INDI_CLIENT_LIBRARIES} z)
        endif(BUILD_KSTARS_LITE)
    endif(WIN32 OR ANDROID)

endif(INDI_FOUND)

if(WCSLIB_FOUND)
    target_link_libraries(KStarsLib ${WCSLIB_LIBRARIES})
endif (WCSLIB_FOUND)

if(LibRaw_FOUND)
if(NOT BUILD_KSTARS_LITE)
    target_link_libraries(KStarsLib ${LibRaw_LIBRARIES})
endif(NOT BUILD_KSTARS_LITE)
endif (LibRaw_FOUND)

#FIXME Enable OpenGL Later
#if( OPENGL_FOUND )
#    target_link_libraries(KStarsLib
#    ${OPENGL_LIBRARIES}
#    ${QT_QTOPENGL_LIBRARY}
#    )
#endif( OPENGL_FOUND )

set (KSTARS_APP_SRCS
    main.cpp
)

if(NOT BUILD_KSTARS_LITE)
# add icon to application sources
ecm_add_app_icon(KSTARS_APP_SRCS ICONS
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/16-apps-kstars.png
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/32-apps-kstars.png
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/48-apps-kstars.png
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/64-apps-kstars.png
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/128-apps-kstars.png
)

qt5_add_resources(KSTARS_APP_SRCS data/kstars.qrc)
endif(NOT BUILD_KSTARS_LITE)

add_executable(kstars ${KSTARS_APP_SRCS})
target_link_libraries(kstars KStarsLib)

if(NOT BUILD_KSTARS_LITE)
    install(TARGETS kstars ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

    ########### install files ###############
    install(PROGRAMS org.kde.kstars.desktop DESTINATION ${KDE_INSTALL_APPDIR})
    install(FILES kstars.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
    install(FILES kstars.notifyrc DESTINATION ${KNOTIFYRC_INSTALL_DIR})
endif(NOT BUILD_KSTARS_LITE)
