if(NOT APPLE) #This is not needed on MacOS, since the data directory and icons are now installed to the app bundle first using the code at the bottom
    add_subdirectory( data )
    add_subdirectory( icons )
endif(NOT APPLE)

add_subdirectory( htmesh )

if (ANDROID AND CMAKE_TOOLCHAIN_FILE)
    include(${CMAKE_TOOLCHAIN_FILE})
endif ()

if (NOT ANDROID)
    find_package(ZLIB REQUIRED)
    include_directories(${ZLIB_INCLUDE_DIR})
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
endif ()

if(MSVC)
    add_definitions(-D_USE_MATH_DEFINES=1)
    add_definitions(-DNOMINMAX)
endif()

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(sincos "math.h" HAVE_SINCOS)
if(HAVE_SINCOS)
    add_definitions(-DHAVE_SINCOS)
endif()

include_directories(
    ${kstars_SOURCE_DIR}/kstars
    ${kstars_SOURCE_DIR}/kstars/skyobjects
    ${kstars_SOURCE_DIR}/kstars/skycomponents
    ${kstars_SOURCE_DIR}/kstars/auxiliary
    ${kstars_SOURCE_DIR}/kstars/time
    ${kstars_SOURCE_DIR}/kstars/tools
    ${kstars_SOURCE_DIR}/kstars/catalogsdb
    ${kstars_SOURCE_DIR}/kstars/polyfills
    )

if (INDI_FOUND)
    if(BUILD_KSTARS_LITE)
            set (fits_klite_SRCS
                fitsviewer/fitsdata.cpp
                )
            set (fits2_klite_SRCS
                fitsviewer/bayer.c
                fitsviewer/fpack.c
                fitsviewer/fpackutil.c
                )
                include_directories(${CFITSIO_INCLUDE_DIR})
                include_directories(${NOVA_INCLUDE_DIR})

            set (indi_klite_SRCS
                indi/clientmanagerlite.cpp
                indi/inditelescopelite.cpp
                kstarslite/skyitems/skynodes/crosshairnode.cpp
                kstarslite/skyitems/telescopesymbolsitem.cpp
            )
    endif ()

    set(indiui_SRCS
        indi/streamform.ui
        indi/drivermanager.ui
        indi/opsindi.ui
        indi/indihostconf.ui
        indi/customdrivers.ui
        #indi/telescopewizard.ui
    )

    set(indi_SRCS
        indi/drivermanager.cpp
        indi/servermanager.cpp
        indi/clientmanager.cpp
        indi/blobmanager.cpp
        indi/guimanager.cpp
        indi/driverinfo.cpp
        indi/deviceinfo.cpp
        indi/indidevice.cpp
        indi/indigroup.cpp
        indi/wsmedia.cpp
        indi/indiproperty.cpp
        indi/indielement.cpp
        indi/indistd.cpp
        indi/indilistener.cpp
        indi/indiconcretedevice.cpp
        indi/indicommon.cpp
        indi/indiguider.cpp
        indi/indimount.cpp
        indi/indicamera.cpp
        indi/indicamerachip.cpp
        indi/indifocuser.cpp
        indi/indifilterwheel.cpp
        indi/indidome.cpp
        indi/indigps.cpp
        indi/indiweather.cpp
        indi/indidustcap.cpp
        indi/indilightbox.cpp
        indi/indiauxiliary.cpp
        indi/indidetector.cpp
        indi/indirotator.cpp
        indi/indiadaptiveoptics.cpp
        indi/indicorrelator.cpp
        indi/indispectrograph.cpp
        indi/indidbus.cpp
        indi/opsindi.cpp
        indi/streamwg.cpp
        indi/videowg.cpp
        indi/indiwebmanager.cpp
        indi/customdrivers.cpp
        indi/collimationoverlayoptions.cpp
        indi/collimationoverlaytypes.h
    )

    if (CFITSIO_FOUND)
        set(ekosui_SRCS
            ekos/opsekos.ui
            ekos/manager.ui
            ekos/manager/focusprogresswidget.ui
            ekos/manager/guidemanager.ui
            ekos/profileeditor.ui
            ekos/profilewizard.ui
            # Analyze
            ekos/analyze/analyze.ui
            ekos/analyze/yaxistool.ui
            # Scheduler
            ekos/scheduler/scheduler.ui
            ekos/scheduler/framingassistant.ui
            ekos/scheduler/scheduleraltitudegraph.ui
            ekos/scheduler/opsalignmentsettings.ui
            ekos/scheduler/opsscriptssettings.ui
            ekos/scheduler/opsoffsetsettings.ui
            ekos/scheduler/opsjobssettings.ui
            # Capture
            ekos/capture/capture.ui
            ekos/capture/camera.ui
            ekos/capture/calibrationoptions.ui
            ekos/capture/capturepreviewwidget.ui
            ekos/capture/capturecountswidget.ui
            ekos/capture/captureprocessoverlay.ui
            ekos/capture/dslrinfo.ui
            ekos/capture/rotatorsettings.ui
            ekos/capture/customproperties.ui
            ekos/capture/scriptsmanager.ui
            ekos/capture/limits.ui
            ekos/capture/sequenceeditorui.ui
            ekos/capture/opsdslrsettings.ui
            ekos/capture/opsmiscsettings.ui

            # Exposure Calculator
            ekos/capture/exposurecalculator/exposurecalculatordialog.ui
            ekos/capture/exposurecalculator/fileutilitycameradatadialog.ui            
            # Align
            ekos/align/align.ui
            ekos/align/opsastrometry.ui
            ekos/align/opsprograms.ui
            ekos/align/opsalign.ui
            #ekos/align/opsastrometrycfg.ui
            ekos/align/opsastrometryindexfiles.ui
            ekos/align/manualrotator.ui
            ekos/align/mountmodel.ui
            ekos/align/polaralignmentassistant.ui
            ekos/align/polaralignwidget.ui
            #ekos/align/opsastap.ui
            # Focus
            ekos/focus/focus.ui
            ekos/focus/focusmodule.ui
            ekos/focus/opsfocussettings.ui
            ekos/focus/opsfocusprocess.ui
            ekos/focus/opsfocusmechanics.ui
            ekos/focus/cfz.ui
            ekos/focus/focusadvisor.ui
            ekos/focus/focusadvisorhelp.ui
            # Mount
            ekos/mount/mount.ui
            ekos/mount/mountcontrolpanel.ui
            ekos/mount/mountmotionwidget.ui
            ekos/mount/mountpositionwidget.ui
            ekos/mount/mounttargetwidget.ui
            ekos/mount/meridianflipstatuswidget.ui
            # Guide
            ekos/guide/guide.ui
            ekos/guide/guidestatewidget.ui
            ekos/guide/opscalibration.ui
            ekos/guide/opsguide.ui
            ekos/guide/opsdither.ui
            ekos/guide/opsgpg.ui
            ekos/guide/manualdither.ui
            ekos/guide/manualpulse.ui
            ekos/observatory/observatory.ui
            #TODO remove from GIT
            #ekos/guide/guider.ui
            #ekos/guide/rcalibration.ui
            # Auxiliary
            ekos/auxiliary/filtersettings.ui
            ekos/auxiliary/buildfilteroffsets.ui
            ekos/auxiliary/opslogs.ui
            ekos/auxiliary/serialportassistant.ui
            ekos/auxiliary/darklibrary.ui
            ekos/auxiliary/opticaltrains.ui
            ekos/auxiliary/ledstatuswidget.ui
            # Ekos Live
            ekos/ekoslive/ekoslivedialog.ui
        )
        # Aberration Inspector
        if (Qt5DataVisualization_FOUND OR Qt6DataVisualization_FOUND)
            set(ekosui_SRCS ${ekosui_SRCS} ekos/focus/aberrationinspector.ui)
        endif()

        set(ekos_SRCS
            ekos/ekos.cpp
            ekos/profileeditor.cpp
            ekos/profilewizard.cpp
            ekos/profilescript.cpp
            ekos/profilescriptdialog.cpp
            ekos/qMDNS.cpp
            ekos/opsekos.cpp

            # Manager
            ekos/manager.cpp
            ekos/manager/focusprogresswidget.cpp
            ekos/manager/guidemanager.cpp
            ekos/manager/meridianflipstate.cpp
            ekos/extensions.cpp

            # Auxiliary
            ekos/auxiliary/darklibrary.cpp
            ekos/auxiliary/darkprocessor.cpp
            ekos/auxiliary/darkview.cpp
            ekos/auxiliary/defectmap.cpp
            ekos/auxiliary/opticaltrainmanager.cpp
            ekos/auxiliary/profilesettings.cpp
            ekos/auxiliary/opticaltrainsettings.cpp
            ekos/auxiliary/filtermanager.cpp
            ekos/auxiliary/buildfilteroffsets.cpp
            ekos/auxiliary/tabledelegate.cpp
            ekos/auxiliary/opslogs.cpp
	        ekos/auxiliary/rotatorutils.cpp
            ekos/auxiliary/serialportassistant.cpp
            ekos/auxiliary/portselector.cpp
            ekos/auxiliary/ledstatuswidget.cpp

            # Capture
            ekos/capture/capture.cpp
            ekos/capture/capturemodulestate.cpp
            ekos/capture/cameraprocess.cpp
            ekos/capture/camerastate.cpp
            ekos/capture/capturedeviceadaptor.cpp
            ekos/capture/camera.cpp
            ekos/capture/camera_actions.cpp
            ekos/capture/camera_config.cpp
            ekos/capture/camera_device.cpp
            ekos/capture/camera_jobs.cpp
            ekos/capture/capturepreviewwidget.cpp
            ekos/capture/capturecountswidget.cpp
            ekos/capture/captureprocessoverlay.cpp
            ekos/capture/refocusstate.cpp
            ekos/capture/sequencejob.cpp
            ekos/capture/sequencejobstate.cpp
            ekos/capture/sequencequeue.cpp
            ekos/capture/dslrinfodialog.cpp
            ekos/capture/rotatorsettings.cpp
            ekos/capture/customproperties.cpp
            ekos/capture/scriptsmanager.cpp
            ekos/capture/placeholderpath.cpp
            ekos/capture/sequenceeditor.cpp
            ekos/capture/opsdslrsettings.cpp
            ekos/capture/opsmiscsettings.cpp
            
            # Exposure Calculator
            ekos/capture/exposurecalculator/exposurecalculatordialog.cpp
            ekos/capture/exposurecalculator/calculatedgainsubexposuretime.cpp
            ekos/capture/exposurecalculator/cameraexposureenvelope.cpp
            ekos/capture/exposurecalculator/cameragainreadmode.cpp
            ekos/capture/exposurecalculator/cameragainreadnoise.cpp
            ekos/capture/exposurecalculator/fileutilitycameradata.cpp
            ekos/capture/exposurecalculator/fileutilitycameradatadialog.cpp
            ekos/capture/exposurecalculator/imagingcameradata.cpp
            ekos/capture/exposurecalculator/optimalexposuredetail.cpp
            ekos/capture/exposurecalculator/optimalexposurestack.cpp
            ekos/capture/exposurecalculator/optimalsubexposurecalculator.cpp
	        
            # Analyze
            ekos/analyze/analyze.cpp
            ekos/analyze/yaxistool.cpp

            # Scheduler
            ekos/scheduler/schedulerjob.cpp
            ekos/scheduler/scheduler.cpp
            ekos/scheduler/schedulermodulestate.cpp
            ekos/scheduler/schedulerprocess.cpp
            ekos/scheduler/schedulerutils.cpp
            ekos/scheduler/schedulertypes.cpp
            ekos/scheduler/framingassistantui.cpp
            ekos/scheduler/greedyscheduler.cpp
            ekos/scheduler/scheduleraltitudegraph.cpp
            ekos/scheduler/opsalignmentsettings.cpp
            ekos/scheduler/opsscriptssettings.cpp
            ekos/scheduler/opsjobssettings.cpp
            ekos/scheduler/opsoffsetsettings.cpp

            # Focus
            ekos/focus/focus.cpp
            ekos/focus/focusmodule.cpp
            ekos/focus/focushfrvplot.cpp
            ekos/focus/focusprofileplot.cpp
            ekos/focus/focusalgorithms.cpp
            ekos/focus/polynomialfit.cpp
            ekos/focus/focusstars.cpp
            ekos/focus/curvefit.cpp
            ekos/focus/focusfwhm.cpp
            ekos/focus/focusfourierpower.cpp
            ekos/focus/adaptivefocus.cpp
            ekos/focus/focusadvisor.cpp
            ekos/focus/opsfocusbase.cpp
            ekos/focus/opsfocussettings.cpp
            ekos/focus/opsfocusprocess.cpp
            ekos/focus/opsfocusmechanics.cpp
            ekos/focus/exposurewidget.cpp

            # Mount
            ekos/mount/mount.cpp
            ekos/mount/meridianflipstatuswidget.cpp
            ekos/mount/mountcontrolpanel.cpp
            ekos/mount/mountmotionwidget.cpp
            ekos/mount/mountpositionwidget.cpp
            ekos/mount/mounttargetwidget.cpp

            # Align
            ekos/align/align.cpp
            ekos/align/alignview.cpp
            ekos/align/astrometryparser.cpp
            ekos/align/opsastrometry.cpp
            ekos/align/opsalign.cpp
            ekos/align/opsprograms.cpp
            ekos/align/opsastrometryindexfiles.cpp
            ekos/align/remoteastrometryparser.cpp
            ekos/align/poleaxis.cpp
            ekos/align/polaralign.cpp
            ekos/align/rotations.cpp
            ekos/align/mountmodel.cpp
            ekos/align/polaralignmentassistant.cpp
            ekos/align/manualrotator.cpp
            ekos/align/polaralignwidget.cpp

            # Guide
            ekos/guide/guide.cpp
            ekos/guide/guidestatewidget.cpp
            ekos/guide/guideinterface.cpp
            ekos/guide/opscalibration.cpp
            ekos/guide/opsguide.cpp
            ekos/guide/opsdither.cpp
            ekos/guide/opsgpg.cpp
            ekos/guide/guidedriftgraph.cpp
            ekos/guide/guidetargetplot.cpp
            ekos/guide/manualpulse.cpp
            # Internal Guide
            ekos/guide/internalguide/gmath.cpp
            ekos/guide/internalguide/guidealgorithms.cpp
            ekos/guide/internalguide/calibrationprocess.cpp
            ekos/guide/internalguide/internalguider.cpp
            #ekos/guide/internalguide/guider.cpp
            ekos/guide/internalguide/matr.cpp
            #ekos/guide/internalguide/rcalibration.cpp
            ekos/guide/internalguide/vect.cpp
            ekos/guide/internalguide/imageautoguiding.cpp
            ekos/guide/internalguide/guidelog.cpp
            ekos/guide/internalguide/starcorrespondence.cpp
            ekos/guide/internalguide/gpg.cpp
            ekos/guide/internalguide/calibration.cpp
            ekos/guide/internalguide/guidestars.cpp
            ekos/guide/internalguide/linearguider.cpp
            ekos/guide/internalguide/hysteresisguider.cpp
            ekos/guide/guideview.cpp
            # External Guide
            ekos/guide/externalguide/phd2.cpp
            ekos/guide/externalguide/linguider.cpp

            #Observatory
            # Needs to migrate Observatory
            ekos/observatory/observatory.cpp
#            ekos/observatory/observatorymodel.cpp
#            ekos/observatory/observatorydomemodel.cpp
#            ekos/observatory/observatoryweathermodel.cpp

            # Ekos Live
            ekos/ekoslive/ekosliveclient.cpp
            ekos/ekoslive/message.cpp
            ekos/ekoslive/media.cpp
            ekos/ekoslive/cloud.cpp
            ekos/ekoslive/node.cpp
            ekos/ekoslive/nodemanager.cpp

            # Tools
            tools/imagingplanner.cpp
            tools/imagingplanneroptions.cpp
            
        )
        # Aberration Inspector
        if (Qt5DataVisualization_FOUND OR Qt6DataVisualization_FOUND)
            set(ekos_SRCS ${ekos_SRCS} ekos/focus/aberrationinspector.cpp)
            set(ekos_SRCS ${ekos_SRCS} ekos/focus/aberrationinspectorplot.cpp)
            set(ekos_SRCS ${ekos_SRCS} ekos/focus/sensorgraphic.cpp)
            set(ekos_SRCS ${ekos_SRCS} ekos/focus/abinstablewidget.cpp)
        endif()

        # Focus Blurriness
        if (OpenCV_FOUND)
            set(ekos_SRCS ${ekos_SRCS} ekos/focus/focusblurriness.cpp)
        endif()

    endif(CFITSIO_FOUND)

    include_directories(${INDI_INCLUDE_DIR})
endif (INDI_FOUND)

if (CFITSIO_FOUND)
  set (gpg_SRCS
        ekos/guide/internalguide/MPI_IS_gaussian_process/src/math_tools.cpp
        ekos/guide/internalguide/MPI_IS_gaussian_process/src/covariance_functions.cpp
        ekos/guide/internalguide/MPI_IS_gaussian_process/src/gaussian_process.cpp
        ekos/guide/internalguide/MPI_IS_gaussian_process/src/gaussian_process_guider.cpp
        )

    set (hough_SRCS
        fitsviewer/hough/houghline.cpp
        )

    set (fits_SRCS
        fitsviewer/fitslabel.cpp
        fitsviewer/fitsviewer.cpp
        fitsviewer/stretch.cpp
        fitsviewer/fitstab.cpp
        fitsviewer/platesolve.cpp
        fitsviewer/fitsdebayer.cpp
        fitsviewer/opsfits.cpp
        )

    if (Qt5DataVisualization_FOUND OR Qt6DataVisualization_FOUND)
        set(fits_SRCS ${fits_SRCS} fitsviewer/starprofileviewer.cpp)
    endif()

    set (fits2_SRCS
        fitsviewer/bayer.c
        fitsviewer/fpack.c
        fitsviewer/fpackutil.c
        fitsviewer/fitshistogrameditor.cpp
        fitsviewer/fitshistogramview.cpp
        fitsviewer/fitshistogramcommand.cpp
        fitsviewer/fitsview.cpp
        fitsviewer/summaryfitsview.cpp
        fitsviewer/fitsdata.cpp
        fitsviewer/fitsstardetector.cpp
        fitsviewer/fitsthresholddetector.cpp
        fitsviewer/fitsgradientdetector.cpp
        fitsviewer/fitscentroiddetector.cpp
        fitsviewer/fitssepdetector.cpp
        fitsviewer/fitsbahtinovdetector.cpp
        fitsviewer/fitsskyobject.cpp
        fitsviewer/fitsstretchui.cpp
        )
    set (fitsui_SRCS
        fitsviewer/fitsheaderdialog.ui
        fitsviewer/statform.ui
        fitsviewer/platesolve.ui
        fitsviewer/catalogobject.ui
        fitsviewer/catalogobjecttypefilter.ui
        fitsviewer/fitsdebayer.ui
        indi/streamform.ui
        indi/recordingoptions.ui
        indi/collimationOptions.ui
        fitsviewer/fitshistogramui.ui
        fitsviewer/fitsstretchui.ui
        fitsviewer/opsfits.ui
        )
    set (ekos_SRCS
        ${ekos_SRCS}
	ekos/auxiliary/stellarsolverprofileeditor.cpp
        ekos/auxiliary/stellarsolverprofile.cpp
        ekos/auxiliary/solverutils.cpp
        )
    set (ekosui_SRCS
	${ekosui_SRCS}
        ekos/auxiliary/stellarsolverprofileeditor.ui
        )
    include_directories(${CFITSIO_INCLUDE_DIR})

endif(CFITSIO_FOUND)

IF (CFITSIO_FOUND)
    IF (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
        IF (SANITIZERS)
            SET_SOURCE_FILES_PROPERTIES(fitsviewer/bayer.c PROPERTIES COMPILE_FLAGS "-Wno-cast-align -fno-sanitize=address,undefined -fomit-frame-pointer")
            SET_SOURCE_FILES_PROPERTIES(fitsviewer/fitsdata.cpp PROPERTIES COMPILE_FLAGS "-fno-sanitize=address,undefined -fomit-frame-pointer")
            SET_SOURCE_FILES_PROPERTIES(fitsviewer/fitshistogram.cpp PROPERTIES COMPILE_FLAGS "-fno-sanitize=address,undefined -fomit-frame-pointer")
            SET_SOURCE_FILES_PROPERTIES(fitsviewer/fitsview.cpp PROPERTIES COMPILE_FLAGS "-fno-sanitize=address,undefined -fomit-frame-pointer")
            SET_SOURCE_FILES_PROPERTIES(fitsviewer/hough/houghline.cpp PROPERTIES COMPILE_FLAGS "-fno-sanitize=address,undefined -fomit-frame-pointer")
        ELSE ()
            SET_SOURCE_FILES_PROPERTIES(fitsviewer/bayer.c PROPERTIES COMPILE_FLAGS "-Wno-cast-align")
        ENDIF ()
        SET_SOURCE_FILES_PROPERTIES(fitsviewer/fpack.c PROPERTIES COMPILE_FLAGS "-Wno-error")
        SET_SOURCE_FILES_PROPERTIES(fitsviewer/fpackutil.c PROPERTIES COMPILE_FLAGS "-Wno-error")
    ELSEIF (NOT WIN32)
        SET_SOURCE_FILES_PROPERTIES(fitsviewer/fpack.c PROPERTIES COMPILE_FLAGS "-Wno-error")
        SET_SOURCE_FILES_PROPERTIES(fitsviewer/fpackutil.c PROPERTIES COMPILE_FLAGS "-Wno-error")
        SET_SOURCE_FILES_PROPERTIES(fitsviewer/sep/aperture.c PROPERTIES COMPILE_FLAGS "-Wno-pointer-arith")
        SET_SOURCE_FILES_PROPERTIES(fitsviewer/sep/deblend.c PROPERTIES COMPILE_FLAGS "-Wno-discarded-qualifiers")
        SET_SOURCE_FILES_PROPERTIES(fitsviewer/sep/util.c PROPERTIES COMPILE_FLAGS "-Wno-discarded-qualifiers")
    ENDIF ()
ENDIF ()

if(WCSLIB_FOUND)
    include_directories( ${WCSLIB_INCLUDE_DIR} )
endif(WCSLIB_FOUND)

set(xplanet_SRCS
    xplanet/opsxplanet.cpp
)

set(xplanetui_SRCS
    xplanet/opsxplanet.ui
)

########### next target ###############
set(libkstarstools_SRCS
    tools/altvstime.cpp
    tools/avtplotwidget.cpp
    tools/calendarwidget.cpp
    tools/conjunctions.cpp
    tools/eclipsetool.cpp
    tools/eclipsehandler.cpp

    tools/eclipsetool/lunareclipsehandler.cpp

    tools/jmoontool.cpp
    tools/approachsolver.cpp
    tools/ksconjunct.cpp
    tools/eqplotwidget.cpp
    tools/astrocalc.cpp
    tools/modcalcangdist.cpp
    tools/modcalcapcoord.cpp
    tools/modcalcaltaz.cpp
    tools/modcalcsimple.cpp
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
    tools/polarishourangle.cpp
    #FIXME Port to KF5
    #tools/moonphasetool.cpp

    tools/starhopper.cpp
    tools/eyepiecefield.cpp
    tools/starhopperdialog.cpp
    tools/greatcircle.cpp

    tools/import_skycomp.cpp
)

if(${KFX_VERSION} VERSION_EQUAL 5.18.0 OR ${KFX_VERSION} VERSION_GREATER 5.18.0)
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

ki18n_wrap_ui(libkstarstools_ui_SRCS
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
    tools/eclipsetool.ui

    tools/modcalcangdist.ui
    tools/modcalcapcoord.ui
    tools/modcalcaltaz.ui
    tools/modcalcsimple.ui
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
    tools/imagingplanner.ui
    tools/imagingplanneroptions.ui
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
    tools/polarishourangle.ui
)

if (${KFX_VERSION} VERSION_EQUAL 5.18.0 OR ${KFX_VERSION} VERSION_GREATER 5.18.0)
    ki18n_wrap_ui(libkstarstools_ui_SRCS
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

ki18n_wrap_ui(libkstarswidgets_ui_SRCS
#    widgets/genericcalendarwidget.ui
    widgets/unitspinboxwidget.ui
)

set(kstars_options_SRCS
    options/opsadvanced.cpp
    options/opscatalog.cpp
    options/opscolors.cpp
    options/opsguides.cpp
    options/opssolarsystem.cpp
    options/opssatellites.cpp
    options/opssupernovae.cpp
    options/opsterrain.cpp
    options/opsimageoverlay.cpp
    options/opsdeveloper.cpp
)

set(kstars_optionsui_SRCS
    options/opsadvanced.ui
    options/opscatalog.ui
    options/opscolors.ui
    options/opsguides.ui
    options/opssolarsystem.ui
    options/opssatellites.ui
    options/opssupernovae.ui
    options/opsterrain.ui
    options/opsimageoverlay.ui
    options/opsdeveloper.ui
)

set(kstars_dialogs_SRCS
    dialogs/addcatalogobject.cpp
    dialogs/catalogsdbui.cpp
    dialogs/catalogeditform.cpp
    dialogs/catalogcoloreditor.cpp
    dialogs/catalogdetails.cpp
    dialogs/catalogcsvimport.cpp
    dialogs/catalogobjectlistmodel.cpp
    dialogs/addlinkdialog.cpp
    dialogs/detaildialog.cpp
    dialogs/finddialog.cpp
    dialogs/focusdialog.cpp
    dialogs/fovdialog.cpp
    dialogs/viewsdialog.cpp
    dialogs/locationdialog.cpp
    dialogs/timedialog.cpp
    dialogs/exportimagedialog.cpp
)

set(kstars_dialogsui_SRCS
    dialogs/addcatalogobject.ui
    dialogs/catalogsdbui.ui
    dialogs/catalogeditform.ui
    dialogs/catalogcoloreditor.ui
    dialogs/catalogdetails.ui
    dialogs/catalogcsvimport.ui
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
    dialogs/viewsdialog.ui
    dialogs/locationdialog.ui
    dialogs/wizwelcome.ui
    dialogs/wizlocation.ui
    dialogs/wizdownload.ui
    dialogs/wizdata.ui
    dialogs/newfov.ui
    dialogs/newview.ui
    dialogs/exportimagedialog.ui
)

set(hips_SRCS
    hips/healpix.cpp
    hips/hipsrenderer.cpp
    hips/hipsfinder.cpp
    hips/scanrender.cpp
    hips/pixcache.cpp
    hips/urlfiledownload.cpp
    hips/opships.cpp
)

set(terrain_SRCS
    terrain/terrainrenderer.cpp
)

set(hips_manager_SRCS
    hips/hipsmanager.cpp
)

set(oal_SRCS
    oal/log.cpp
    oal/observer.cpp
    oal/site.cpp
    oal/session.cpp
    oal/scope.cpp
    oal/dslrlens.cpp
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
    skycomponents/earthshadowcomponent.cpp
    skycomponents/asteroidscomponent.cpp
    skycomponents/cometscomponent.cpp
    skycomponents/planetmoonscomponent.cpp
    skycomponents/solarsystemcomposite.cpp
    skycomponents/satellitescomponent.cpp
    skycomponents/starcomponent.cpp
    skycomponents/deepstarcomponent.cpp
    skycomponents/catalogscomponent.cpp
    skycomponents/constellationartcomponent.cpp
    skycomponents/constellationboundarylines.cpp
    skycomponents/constellationlines.cpp
    skycomponents/constellationnamescomponent.cpp
    skycomponents/supernovaecomponent.cpp
    skycomponents/coordinategrid.cpp
    skycomponents/equatorialcoordinategrid.cpp
    skycomponents/horizontalcoordinategrid.cpp
    skycomponents/localmeridiancomponent.cpp
    skycomponents/ecliptic.cpp
    skycomponents/equator.cpp
    skycomponents/artificialhorizoncomponent.cpp
    skycomponents/hipscomponent.cpp
    skycomponents/terraincomponent.cpp
    skycomponents/imageoverlaycomponent.cpp
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

IF (INDI_FOUND)
LIST(APPEND libkstarscomponents_SRCS
    skycomponents/mosaiccomponent.cpp
    )
ENDIF(INDI_FOUND)

#LIST(APPEND libkstarscomponents_SRCS
#    #skycomponents/notifyupdatesui.cpp
#    )

IF (BUILD_KSTARS_LITE)
    set(libkstarstools_ui_klite_SRCS
        tools/nameresolver.cpp
        )
ENDIF ()

set(kstars_skyobjects_SRCS
    skyobjects/constellationsart.cpp
    skyobjects/catalogobject.cpp
    skyobjects/jupitermoons.cpp
    skyobjects/planetmoons.cpp
    skyobjects/ksasteroid.cpp
    skyobjects/kscomet.cpp
    skyobjects/ksmoon.cpp
    skyobjects/ksearthshadow.cpp
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

IF (INDI_FOUND)
LIST(APPEND kstars_skyobjects_SRCS
    skyobjects/mosaictiles.cpp
    )
ENDIF(INDI_FOUND)

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
    auxiliary/nonlineardoublespinbox.cpp
    auxiliary/profileinfo.cpp
    auxiliary/filedownloader.cpp
    auxiliary/kspaths.cpp
    auxiliary/QRoundProgressBar.cpp
    auxiliary/skyobjectlistmodel.cpp
    auxiliary/ksnotification.cpp
    auxiliary/ksmessagebox.cpp
    auxiliary/QProgressIndicator.cpp
    auxiliary/ctkrangeslider.cpp
    auxiliary/ctk3slider.cpp
    auxiliary/rectangleoverlap.cpp
    auxiliary/gslhelpers.cpp
    auxiliary/robuststatistics.cpp
    time/simclock.cpp
    time/kstarsdatetime.cpp
    time/timezonerule.cpp
    ksnumbers.cpp
    kstarsdata.cpp
    texturemanager.cpp
    #to minimize number of indef KSTARS_LITE
    skypainter.cpp    
    auxiliary/screencapture.cpp    
    )

SET(kstars_extra_kstars_SRCS
    auxiliary/thememanager.cpp
    auxiliary/schememanager.cpp
    auxiliary/imagemask.cpp
    auxiliary/imageviewer.cpp
    auxiliary/xplanetimageviewer.cpp
    auxiliary/fov.cpp
    auxiliary/skymapview.cpp
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

SET(catalogsdb_SRCS
        catalogsdb/catalogsdb.cpp)

if(NOT APPLE) #KStarsLite files including the QML files are not needed on MacOS right now
# Temporary solution to allow use of qml files from source dir DELETE
SET(KSTARSLITE_CPP_OPTIONS -DSOURCE_DIR=\"${kstars_SOURCE_DIR}\" -DQML_IMPORT="${CMAKE_CURRENT_SOURCE_DIR}")

set(klite_SRCS
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
    #Other
    kstarslite/deviceorientation.cpp
    )

set(kstarslite_libtess_SRC
    #libtess
    libtess/gluos.h
    libtess/priorityq-sort.h
    libtess/sweep.c
    libtess/tessmono.c
    libtess/dict-list.h
    libtess/glu.h
    libtess/tessellate.c
    libtess/dict.c
    libtess/geom.c
    libtess/memalloc.c
    libtess/mesh.c
    libtess/normal.c
    libtess/priorityq.c
    libtess/priorityq-heap.c
    libtess/render.c
    libtess/tess.c
    )

IF (BUILD_KSTARS_LITE)
    ADD_CUSTOM_TARGET(convert_translations ${CMAKE_SOURCE_DIR}/tools/convert_translations.sh ${CMAKE_BINARY_DIR}
                      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    ADD_DEPENDENCIES(convert_translations fetch-translations)
    IF (ANDROID)
        ADD_CUSTOM_TARGET(convert_translations_to_android ${CMAKE_SOURCE_DIR}/tools/convert_translations.sh ${CMAKE_BINARY_DIR}/packaging/android/export/share/kstars
                          WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        ADD_DEPENDENCIES(convert_translations_to_android fetch-translations)
    ENDIF ()
ENDIF ()

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    SET_SOURCE_FILES_PROPERTIES(${kstarslite_libtess_SRC} PROPERTIES COMPILE_FLAGS "-Wno-error")
ENDIF ()

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
    kstarslite/qml/modules/KSTabBarArrow.qml
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
    kstarslite/qml/dialogs/AboutDialog.qml

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
endif(NOT APPLE) #This is the end of the Mac KStars disabling of the KStarsLite Code

if(ANDROID)
    add_subdirectory(kstarslite/res)
endif(ANDROID)

set(kstars_SRCS
    ${indi_SRCS}
    ${fits_SRCS}
    ${ekos_SRCS}
    ${libkstarswidgets_SRCS}
    ${libkstarscomponents_SRCS}
    ${libkstarstools_SRCS}
    ${kstars_extra_SRCS}
    ${kstars_extra_kstars_SRCS}
    ${kstars_projection_SRCS}
    ${xplanet_SRCS}
    ${kstars_options_SRCS}
    ${kstars_skyobjects_SRCS}
    ${kstars_dialogs_SRCS}
    ${hips_SRCS}
    ${terrain_SRCS}
    ${oal_SRCS}
    ${printing_SRCS}
    ${catalogsdb_SRCS}
    #KStars Lite
    ${kstarslite_SRCS}
    # Generated files
    ${libkstarstools_ui_SRCS}
    ${libkstarswidgets_ui_SRCS}
)

set(kstarslite_SRCS
    ${indi_klite_SRCS}
    ${libkstarscomponents_SRCS}
    ${kstars_extra_SRCS}
    ${kstars_projection_SRCS}
    ${kstars_skyobjects_SRCS}

    # KStars Lite sources
    ${klite_SRCS}
    # Generated files
    ${libkstarstools_ui_klite_SRCS}
)

# Generate all the necessary QLoggingCategory files
ecm_qt_declare_logging_category(kstars_SRCS HEADER kstars_debug.h IDENTIFIER KSTARS CATEGORY_NAME org.kde.kstars)
ecm_qt_declare_logging_category(kstars_SRCS HEADER indi_debug.h IDENTIFIER KSTARS_INDI CATEGORY_NAME org.kde.kstars.indi)
ecm_qt_declare_logging_category(kstars_SRCS HEADER fits_debug.h IDENTIFIER KSTARS_FITS CATEGORY_NAME org.kde.kstars.fits)
ecm_qt_declare_logging_category(kstars_SRCS HEADER ekos_debug.h IDENTIFIER KSTARS_EKOS CATEGORY_NAME org.kde.kstars.ekos)
ecm_qt_declare_logging_category(kstars_SRCS HEADER ekos_capture_debug.h IDENTIFIER KSTARS_EKOS_CAPTURE CATEGORY_NAME org.kde.kstars.ekos.capture)
ecm_qt_declare_logging_category(kstars_SRCS HEADER ekos_focus_debug.h IDENTIFIER KSTARS_EKOS_FOCUS CATEGORY_NAME org.kde.kstars.ekos.focus)
ecm_qt_declare_logging_category(kstars_SRCS HEADER ekos_align_debug.h IDENTIFIER KSTARS_EKOS_ALIGN CATEGORY_NAME org.kde.kstars.ekos.align)
ecm_qt_declare_logging_category(kstars_SRCS HEADER ekos_guide_debug.h IDENTIFIER KSTARS_EKOS_GUIDE CATEGORY_NAME org.kde.kstars.ekos.guide)
ecm_qt_declare_logging_category(kstars_SRCS HEADER ekos_mount_debug.h IDENTIFIER KSTARS_EKOS_MOUNT CATEGORY_NAME org.kde.kstars.ekos.mount)
ecm_qt_declare_logging_category(kstars_SRCS HEADER ekos_scheduler_debug.h IDENTIFIER KSTARS_EKOS_SCHEDULER CATEGORY_NAME org.kde.kstars.ekos.scheduler)
ecm_qt_declare_logging_category(kstars_SRCS HEADER ekos_analyze_debug.h IDENTIFIER KSTARS_EKOS_ANALYZE CATEGORY_NAME org.kde.kstars.ekos.analyze)
ecm_qt_declare_logging_category(kstars_SRCS HEADER ekos_observatory_debug.h IDENTIFIER KSTARS_EKOS_OBSERVATORY CATEGORY_NAME org.kde.kstars.ekos.observatory)

ecm_qt_declare_logging_category(kstars_SRCS HEADER catalogsdb_debug.h IDENTIFIER KSTARS_CATALOGS CATEGORY_NAME org.kde.kstars.catalogdb)


set( kstars_KCFG_SRCS Options.kcfgc )

# We cannot generate property up until KF5 Config v5.78.0 or later
if (${KFX_VERSION} VERSION_GREATER_EQUAL "5.78.0")
    set(KF5_CONFIG_OK true)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Options.kcfgc.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/Options.kcfgc
    )
    kconfig_add_kcfg_files(kstars_SRCS ${kstars_KCFG_SRCS} GENERATE_MOC)
    kconfig_add_kcfg_files(kstarslite_SRCS ${kstars_KCFG_SRCS} GENERATE_MOC)
else()
    set(KF5_CONFIG_OK false)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Options.kcfgc.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/Options.kcfgc
    )
    kconfig_add_kcfg_files(kstars_SRCS ${kstars_KCFG_SRCS})
    kconfig_add_kcfg_files(kstarslite_SRCS ${kstars_KCFG_SRCS})
endif()

ecm_qt_declare_logging_category(kstarslite_SRCS HEADER kstars_debug.h IDENTIFIER KSTARS CATEGORY_NAME org.kde.kstars)
ecm_qt_declare_logging_category(kstarslite_SRCS HEADER fits_debug.h IDENTIFIER KSTARS_FITS CATEGORY_NAME org.kde.kstars.fits)

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(kstars kstars_SRCS 10 cpp)
    ENABLE_UNITY_BUILD(kstarslite kstarslite_SRCS 10 cpp)
ENDIF ()

set(kstars_SRCS ${kstars_SRCS} ${fits2_SRCS} ${gpg_SRCS} ${hough_SRCS} ${hips_manager_SRCS})
set(kstarslite_SRCS ${kstarslite_SRCS} ${fits_klite_SRCS} ${hough_SRCS} ${fits2_klite_SRCS} ${kstarslite_libtess_SRC})

IF (NOT ANDROID)
    # GSL
    find_package(GSL REQUIRED)
    IF(BUILD_QT5)
       qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.xml kstars.h KStars)
       qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.SimClock.xml simclock.h SimClock)
       qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.FOV.xml fov.h FOV)
    ELSE()
       qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.xml kstars.h KStars)
       qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.SimClock.xml simclock.h SimClock)
       qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.FOV.xml fov.h FOV)
    ENDIF()
    IF (INDI_FOUND)
        IF(BUILD_QT5)
            # INDI
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.xml indi/indidbus.h INDIDBus)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.GenericDevice.xml indi/indistd.h ISD::GenericDevice)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.Dome.xml indi/indidome.h ISD::Dome)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.Weather.xml indi/indiweather.h ISD::Weather)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.DustCap.xml indi/indidustcap.h ISD::DustCap)
            # Ekos
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.xml ekos/manager.h Ekos::Manager)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Capture.xml ekos/capture/capture.h Ekos::Capture)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Focus.xml ekos/focus/focusmodule.h Ekos::FocusModule)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Guide.xml ekos/guide/guide.h Ekos::Guide)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Align.xml ekos/align/align.h Ekos::Align)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Mount.xml ekos/mount/mount.h Ekos::Mount)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Scheduler.xml ekos/scheduler/schedulerprocess.h Ekos::SchedulerProcess)
            qt5_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Observatory.xml ekos/observatory/observatory.h Ekos::Observatory)
        ELSE()
            # INDI
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.xml indi/indidbus.h INDIDBus)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.GenericDevice.xml indi/indistd.h ISD::GenericDevice)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.Dome.xml indi/indidome.h ISD::Dome)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.Weather.xml indi/indiweather.h ISD::Weather)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.INDI.DustCap.xml indi/indidustcap.h ISD::DustCap)
            # Ekos
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.xml ekos/manager.h Ekos::Manager)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Capture.xml ekos/capture/capture.h Ekos::Capture)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Focus.xml ekos/focus/focusmodule.h Ekos::FocusModule)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Guide.xml ekos/guide/guide.h Ekos::Guide)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Align.xml ekos/align/align.h Ekos::Align)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Mount.xml ekos/mount/mount.h Ekos::Mount)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Scheduler.xml ekos/scheduler/schedulerprocess.h Ekos::SchedulerProcess)
            qt6_add_dbus_adaptor(kstars_SRCS org.kde.kstars.Ekos.Observatory.xml ekos/observatory/observatory.h Ekos::Observatory)
	ENDIF()
    ENDIF ()

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
        hips/opships.ui
        hips/opshipsdisplay.ui
        hips/opshipscache.ui
        #skycomponents/notifyupdatesui.ui
    )

    add_library(KStarsLib STATIC ${kstars_SRCS}
        indi/collimationOptions.ui
    )

    if (BUILD_PYKSTARS)
      set_target_properties(KStarsLib PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif ()

    include(GenerateExportHeader)
    generate_export_header(KStarsLib)

    target_link_libraries(KStarsLib
    LibKSDataHandlers
    htmesh
    Qt::Gui
    Qt::PrintSupport
    Qt::Sql
    Qt::Svg
    Qt::Qml
    Qt::Quick
    Qt::Network
    #Qt::Positioning
    Qt::Concurrent
    Qt::WebSockets
    ${ZLIB_LIBRARIES}
    Eigen3::Eigen
    GSL::gsl
    )
    if (BUILD_QT5)
        target_link_libraries(KStarsLib
        KF5::Crash
        KF5::I18n
        KF5::NewStuff
        KF5::KIOFileWidgets
        KF5::WidgetsAddons
        KF5::Plotting
        KF5::Notifications
        )
    else()
        target_link_libraries(KStarsLib
        KF6::Crash
        KF6::I18n
	KF6::XmlGui
        KF6::NewStuffWidgets
        KF6::KIOFileWidgets
        KF6::WidgetsAddons
        KF6::Plotting
        KF6::Notifications
        Qt6::DBus
        Qt6::SvgWidgets
        )
    endif()

    if (Qt5Keychain_FOUND OR Qt6Keychain_FOUND)
        target_include_directories(KStarsLib PUBLIC ${QTKEYCHAIN_INCLUDE_DIRS})
        target_link_libraries(KStarsLib ${QTKEYCHAIN_LIBRARIES})
    endif()

    if (Qt5DataVisualization_FOUND OR Qt6DataVisualization_FOUND)
        target_link_libraries(KStarsLib Qt::DataVisualization)
    endif()

    if (KF5NotifyConfig_FOUND)
        target_link_libraries(KStarsLib KF5::NotifyConfig)
    elseif(KF6NotifyConfig_FOUND)
        target_link_libraries(KStarsLib KF6::NotifyConfig)
    endif()

    if (StellarSolver_FOUND)
	target_link_libraries(KStarsLib StellarSolver::stellarsolver)
    endif(StellarSolver_FOUND)

    if(NOT WIN32)
        target_link_libraries(KStarsLib m)
    endif(NOT WIN32)
ENDIF ()

if (BUILD_KSTARS_LITE)
    add_library(KStarsLiteLib STATIC ${kstarslite_SRCS})
    target_link_libraries(KStarsLiteLib
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
        Qt5::PositioningQuick
        Qt5::Concurrent
        ${ZLIB_LIBRARIES}
        )

    if (ANDROID)
        target_link_libraries(KStarsLiteLib Qt5::AndroidExtras)
    endif ()

endif ()


if (CFITSIO_FOUND)
    if (NOT ANDROID)
        target_include_directories(KStarsLib PUBLIC ${CFITSIO_INCLUDE_DIR})
        target_link_libraries(KStarsLib ${CFITSIO_LIBRARIES})
    endif()
    if (BUILD_KSTARS_LITE)
        target_include_directories(KStarsLiteLib PUBLIC ${CFITSIO_INCLUDE_DIR})
        target_link_libraries(KStarsLiteLib ${CFITSIO_LIBRARIES})
    endif()
endif(CFITSIO_FOUND)

if(INDI_FOUND)
    if (NOT ANDROID)
        find_package(Nova REQUIRED)
        include_directories(${NOVA_INCLUDE_DIR})
    endif ()

    ## Support Multiple Platforms. All Require INDI
    ## WIN32 Desktop: Requires INDI Qt5 Client + GSL
    ## WIN32 Lite: Requires INDI Qt5 Client
    ## Linux + MacOS Desktop: Requires INDI Client + GSL
    ## Linux + MacOS Lite: Requires INDI Qt5 Client
    ## Android: Requires INDI Qt5 Client built for Android
    if (NOT ANDROID)
        target_link_libraries(KStarsLib ${CMAKE_THREAD_LIBS_INIT})
    endif ()

    if(WIN32 OR ANDROID)
        if(ANDROID)
            target_link_libraries(KStarsLiteLib ${INDI_CLIENT_ANDROID_LIBRARIES} ${CFITSIO_LIBRARIES} ${LIBRAW_LIBRARIES})
            target_compile_options(KStarsLiteLib PRIVATE ${KSTARSLITE_CPP_OPTIONS} -DUSE_QT5_INDI -DKSTARS_LITE)
        else(ANDROID)
            target_link_libraries(KStarsLib ${INDI_CLIENT_LIBRARIES} ${NOVA_LIBRARIES})
        endif(ANDROID)
    else(WIN32 OR ANDROID)
        if (BUILD_KSTARS_LITE)
            target_link_libraries(KStarsLiteLib ${INDI_CLIENT_QT_LIBRARIES} ${NOVA_LIBRARIES} z)
            target_compile_options(KStarsLiteLib PRIVATE ${KSTARSLITE_CPP_OPTIONS} -DUSE_QT5_INDI -DKSTARS_LITE)
        endif(BUILD_KSTARS_LITE)
        target_link_libraries(KStarsLib ${INDI_CLIENT_LIBRARIES} ${NOVA_LIBRARIES} z)
    endif(WIN32 OR ANDROID)

endif(INDI_FOUND)

if(WCSLIB_FOUND)
    target_link_libraries(KStarsLib ${WCSLIB_LIBRARIES})
    if (BUILD_KSTARS_LITE)
        target_link_libraries(KStarsLiteLib ${WCSLIB_LIBRARIES})
    endif()
endif (WCSLIB_FOUND)

if(LibRaw_FOUND)
    if (NOT ANDROID)
        target_link_libraries(KStarsLib ${LibRaw_LIBRARIES})
    endif()
    if (BUILD_KSTARS_LITE)
        target_link_libraries(KStarsLiteLib ${LibRaw_LIBRARIES})
    endif()
endif (LibRaw_FOUND)

# Link libnova if found
if (NOVA_FOUND)
    target_link_libraries(KStarsLib ${NOVA_LIBRARIES})
endif()

if (LibXISF_FOUND)
    target_link_libraries(KStarsLib LibXISF::LibXISF)
endif()

#FIXME Enable OpenGL Later
#if( OPENGL_FOUND )
#    target_link_libraries(KStarsLib
#    ${OPENGL_LIBRARIES}
#    ${QT_QTOPENGL_LIBRARY}
#    )
#endif( OPENGL_FOUND )

if (OpenCV_FOUND)
    target_link_libraries(KStarsLib opencv_highgui opencv_imgcodecs)
endif()

set (KSTARS_APP_SRCS
    main.cpp
)

# add icon to application sources
ecm_add_app_icon(KSTARS_APP_SRCS ICONS
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/16-apps-kstars.png
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/32-apps-kstars.png
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/48-apps-kstars.png
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/64-apps-kstars.png
    ${CMAKE_CURRENT_SOURCE_DIR}/icons/128-apps-kstars.png
)

if (BUILD_QT5)
    qt5_add_resources(KSTARS_APP_SRCS data/kstars.qrc)
else()
    qt6_add_resources(KSTARS_APP_SRCS data/kstars.qrc)
endif()

if (ANDROID)
    add_library(kstars SHARED ${KSTARS_APP_SRCS})
    target_compile_options(kstars PRIVATE ${KSTARSLITE_CPP_OPTIONS} -DUSE_QT5_INDI -DKSTARS_LITE)
    add_dependencies(KStarsLiteLib cfitsio indi raw)
    target_link_libraries(kstars KStarsLiteLib)
else ()
    if (BUILD_KSTARS_LITE)
        add_executable(kstars_lite ${KSTARS_APP_SRCS})
        target_compile_options(kstars_lite PRIVATE ${KSTARSLITE_CPP_OPTIONS} -DUSE_QT5_INDI -DKSTARS_LITE)
        target_link_libraries(kstars_lite KStarsLiteLib)
    endif()
    add_executable(kstars ${KSTARS_APP_SRCS})
    target_link_libraries(kstars KStarsLib)
endif ()

if(APPLE)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files)
        message(STATUS "Updating KStars Mac Files")
        execute_process(COMMAND "git" "pull"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files")
    else(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files)
        message(STATUS "Downloading KStars Mac Files")
        execute_process(COMMAND "git" "clone" "https://github.com/rlancaste/kstars-mac-files.git"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files)

    set(KSTARS_APP "${CMAKE_BINARY_DIR}/bin/KStars.app")
    execute_process(COMMAND ${CMAKE_COMMAND}
                    -DBUNDLED_DATA_LOCATION=${CMAKE_BINARY_DIR}/bin/KStars.app/Contents/Resources
                    ${CMAKE_CURRENT_SOURCE_DIR}/data
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/kstars/data")
    execute_process(COMMAND ${CMAKE_COMMAND} --build . --target install
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/kstars/data")
    file(COPY
        "${CMAKE_CURRENT_SOURCE_DIR}/data/qml"
        DESTINATION "${KSTARS_APP}/Contents/Resources/kstars/")
    file(COPY
        "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files/icons"
        DESTINATION "${KSTARS_APP}/Contents/Resources/")
    file(COPY
        "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files/dbus"
        DESTINATION "${KSTARS_APP}/Contents/Plugins/")
    file(COPY
        "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files/gsc"
        DESTINATION "${KSTARS_APP}/Contents/MacOS/")
    file(COPY
        "${CMAKE_CURRENT_SOURCE_DIR}/kstars.notifyrc"
        DESTINATION "${KSTARS_APP}/Contents/Resources/knotifications5/")
    file(COPY
        "${CMAKE_CURRENT_SOURCE_DIR}/kstars.kcfg"
        DESTINATION "${KSTARS_APP}/Contents/Resources/")

    find_program(DBUS_SEND dbus-send PATHS ${PATHS})
    if(NOT DBUS_SEND)
        message(WARNING "DBUS Send program not found in Path, DBUS, KStars Scripting, and the Scheduler won't work without it.")
    else(NOT DBUS_SEND)
    	file(COPY
            "${DBUS_SEND}"
            DESTINATION "${KSTARS_APP}/Contents/MacOS/")
    endif(NOT DBUS_SEND)

    find_program(DBUS_DAEMON dbus-daemon PATHS ${PATHS})
    if(NOT DBUS_DAEMON)
        message(WARNING "DBUS Daemon program not found in Path, DBUS, KStars Scripting, and the Scheduler won't work without it.")
    else(NOT DBUS_DAEMON)
    	file(COPY
            "${DBUS_DAEMON}"
            DESTINATION "${KSTARS_APP}/Contents/MacOS/")    
    endif(NOT DBUS_DAEMON)

    find_program(XPLANET xplanet PATHS ${PATHS})
    if(NOT XPLANET)
        message(WARNING "X Planet program not found in Path, the XPlanet Solar System Simulator won't work without it.")
    else(NOT XPLANET)
        file(COPY
            "${XPLANET}"
            DESTINATION "${KSTARS_APP}/Contents/MacOS/")
    endif(NOT XPLANET)

    find_program(INDI_SERVER indiserver PATHS ${PATHS})
    if(NOT INDI_SERVER)
        message(WARNING "indiserver program and drivers not found, you can't run local indi drivers without it.")
    else(NOT INDI_SERVER)
        get_filename_component(INDI_DRIVERS_DIR "${INDI_SERVER}" DIRECTORY)
        # INDISERVER
        file(COPY
            "${INDI_SERVER}"
            DESTINATION "${KSTARS_APP}/Contents/MacOS/")
        # INDI Drivers
        file(GLOB INDI_DRIVERS
            "${INDI_DRIVERS_DIR}/indi*"
          )
        list(FILTER INDI_DRIVERS EXCLUDE REGEX "dSYM$")
        file(COPY ${INDI_DRIVERS} DESTINATION "${KSTARS_APP}/Contents/MacOS/")
        # Driver Support Files (Note: On some installations it is in the install prefix, in others in the /usr/local/lib directory)
        if(EXISTS "${CMAKE_INSTALL_PREFIX}/usr/local/lib/indi/DriverSupport")
            file(COPY "${CMAKE_INSTALL_PREFIX}/usr/local/lib/indi/DriverSupport" DESTINATION "${KSTARS_APP}/Contents/Resources/")
        elseif(EXISTS "/usr/local/lib/indi/DriverSupport")
            file(COPY "/usr/local/lib/indi/DriverSupport" DESTINATION "${KSTARS_APP}/Contents/Resources/")
        endif(EXISTS "${CMAKE_INSTALL_PREFIX}/usr/local/lib/indi/DriverSupport")
        file(GLOB XML_FILES
            "${CMAKE_INSTALL_PREFIX}/share/indi/*"
          )
        file(COPY ${XML_FILES} DESTINATION "${KSTARS_APP}/Contents/Resources/DriverSupport/")
        file(COPY "${CMAKE_INSTALL_PREFIX}/lib/indi/MathPlugins" DESTINATION "${KSTARS_APP}/Contents/Resources/")
        # GPHOTO Stuff
        #execute_process(COMMAND "pkg-config --modversion libgphoto2" OUTPUT_VARIABLE GPHOTO_VERSION)
        set(GPHOTO_VERSION "2.5.27")
        set(PORT_VERSION "0.12.0")
        file(GLOB IOLIBS
            "${CMAKE_INSTALL_PREFIX}/lib/libgphoto2_port/${PORT_VERSION}/*"
          )
        list(FILTER IOLIBS EXCLUDE REGEX "dSYM$")
        file(COPY ${IOLIBS} DESTINATION "${KSTARS_APP}/Contents/Resources/DriverSupport/gphoto/IOLIBS/")
        file(GLOB CAMLIBS
            "${CMAKE_INSTALL_PREFIX}/lib/libgphoto2/${GPHOTO_VERSION}/*"
          )
        list(FILTER CAMLIBS EXCLUDE REGEX "dSYM$")
        file(COPY ${CAMLIBS} DESTINATION "${KSTARS_APP}/Contents/Resources/DriverSupport/gphoto/CAMLIBS/")
    endif(NOT INDI_SERVER)

    execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xvz" "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files/phonon/FrameworksForVLC.zip"
        WORKING_DIRECTORY "${KSTARS_APP}/Contents/")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xvz" "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files/phonon/backend.zip"
        WORKING_DIRECTORY "${KSTARS_APP}/Contents/Plugins/")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xvz" "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files/phonon/vlc.zip"
        WORKING_DIRECTORY "${KSTARS_APP}/Contents/Plugins/")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xvz" "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files/xplanet-data.zip"
        WORKING_DIRECTORY "${KSTARS_APP}/Contents/Resources/kstars/")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xvz" "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files/sounds.zip"
        WORKING_DIRECTORY "${KSTARS_APP}/Contents/Resources/")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-E" "tar" "xvz" "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files/locale.zip"
        WORKING_DIRECTORY "${KSTARS_APP}/Contents/Resources/")
    set(KSTARS_VERSION "${KStars_VERSION_MAJOR}.${KStars_VERSION_MINOR}.${KStars_VERSION_REVISION}")
    execute_process(COMMAND date "+%Y" OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE THIS-YEAR)
    set_target_properties(kstars PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/kstars-mac-files/macosbundle.plist.in"
    )
endif(APPLE)

if (BUILD_PYKSTARS)
  add_subdirectory(python)
endif()

install(TARGETS kstars ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############
if(NOT APPLE) # The desktop file is not needed on MacOS and the other two are bundled above
install(PROGRAMS org.kde.kstars.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES kstars.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
IF(BUILD_QT5)
  install(FILES kstars.notifyrc DESTINATION ${KNOTIFYRC_INSTALL_DIR})
else()
  install(FILES kstars.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
endif(BUILD_QT5)
endif(NOT APPLE)

if(INDI_FOUND)
#install(FILES ekos/mount/mountbox.qml DESTINATION  ${KDE_INSTALL_DATADIR}/kstars/ekos/mount/qml)
#install(DIRECTORY ekos/mount/ DESTINATION ${KDE_INSTALL_DATADIR}/kstars/ekos/mount/qml
#        FILES_MATCHING PATTERN "*.png")
endif()

if (NOT ANDROID AND BUILD_KSTARS_LITE)
    install(TARGETS kstars_lite ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
endif()
