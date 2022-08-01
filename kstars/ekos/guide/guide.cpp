/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guide.h"

#include "guideadaptor.h"
#include "kstars.h"
#include "ksmessagebox.h"
#include "ksnotification.h"
#include "kstarsdata.h"
#include "opscalibration.h"
#include "opsguide.h"
#include "opsdither.h"
#include "opsgpg.h"
#include "Options.h"
#include "indi/indiguider.h"
#include "auxiliary/QProgressIndicator.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "externalguide/linguider.h"
#include "externalguide/phd2.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsviewer.h"
#include "internalguide/internalguider.h"
#include "guideview.h"
#include "guidegraph.h"

#include <KConfigDialog>

#include <basedevice.h>
#include <ekos_guide_debug.h>

#include "ui_manualdither.h"

#include <random>

#define CAPTURE_TIMEOUT_THRESHOLD 30000

namespace Ekos
{
Guide::Guide() : QWidget()
{
    // #0 Prelude
    internalGuider = new InternalGuider(); // Init Internal Guider always

    KConfigDialog *dialog = new KConfigDialog(this, "guidesettings", Options::self());

    opsGuide = new OpsGuide();  // Initialize sub dialog AFTER main dialog
    KPageWidgetItem *page = dialog->addPage(opsGuide, i18n("Guide"));
    page->setIcon(QIcon::fromTheme("kstars_guides"));
    connect(opsGuide, &OpsGuide::settingsUpdated, [this]()
    {
        onThresholdChanged(Options::guideAlgorithm());
        configurePHD2Camera();
        configSEPMultistarOptions(); // due to changes in 'Guide Setting: Algorithm'
    });

    opsCalibration = new OpsCalibration(internalGuider);
    page = dialog->addPage(opsCalibration, i18n("Calibration"));
    page->setIcon(QIcon::fromTheme("tool-measure"));

    opsDither = new OpsDither();
    page = dialog->addPage(opsDither, i18n("Dither"));
    page->setIcon(QIcon::fromTheme("transform-move"));

    opsGPG = new OpsGPG(internalGuider);
    page = dialog->addPage(opsGPG, i18n("GPG RA Guider"));
    page->setIcon(QIcon::fromTheme("pathshape"));

    // #1 Setup UI
    setupUi(this);

    // #2 Register DBus
    qRegisterMetaType<Ekos::GuideState>("Ekos::GuideState");
    qDBusRegisterMetaType<Ekos::GuideState>();
    new GuideAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Guide", this);

    // #3 Init Plots
    initPlots();

    // #4 Init View
    initView();
    internalGuider->setGuideView(m_GuideView);

    // #5 Load all settings
    loadSettings();

    // #6 Init Connections
    initConnections();

    // Progress Indicator
    pi = new QProgressIndicator(this);
    controlLayout->addWidget(pi, 1, 2, 1, 1);

    showFITSViewerB->setIcon(
        QIcon::fromTheme("kstars_fitsviewer"));
    connect(showFITSViewerB, &QPushButton::clicked, this, &Ekos::Guide::showFITSViewer);
    showFITSViewerB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    guideAutoScaleGraphB->setIcon(
        QIcon::fromTheme("zoom-fit-best"));
    connect(guideAutoScaleGraphB, &QPushButton::clicked, this, &Ekos::Guide::slotAutoScaleGraphs);
    guideAutoScaleGraphB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    guideSaveDataB->setIcon(
        QIcon::fromTheme("document-save"));
    connect(guideSaveDataB, &QPushButton::clicked, driftGraph, &GuideDriftGraph::exportGuideData);
    guideSaveDataB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    guideDataClearB->setIcon(
        QIcon::fromTheme("application-exit"));
    connect(guideDataClearB, &QPushButton::clicked, this, &Ekos::Guide::clearGuideGraphs);
    guideDataClearB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    // These icons seem very hard to read for this button. Just went with +.
    // guideZoomInXB->setIcon(QIcon::fromTheme("zoom-in"));
    guideZoomInXB->setText("+");
    connect(guideZoomInXB, &QPushButton::clicked, driftGraph, &GuideDriftGraph::zoomInX);
    guideZoomInXB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    // These icons seem very hard to read for this button. Just went with -.
    // guideZoomOutXB->setIcon(QIcon::fromTheme("zoom-out"));
    guideZoomOutXB->setText("-");
    connect(guideZoomOutXB, &QPushButton::clicked, driftGraph, &GuideDriftGraph::zoomOutX);
    guideZoomOutXB->setAttribute(Qt::WA_LayoutUsesWidgetRect);


    // Exposure
    //Should we set the range for the spin box here?
    QList<double> exposureValues;
    exposureValues << 0.02 << 0.05 << 0.1 << 0.2 << 0.5 << 1 << 1.5 << 2 << 2.5 << 3 << 3.5 << 4 << 4.5 << 5 << 6 << 7 << 8 << 9
                   << 10 << 15 << 30;
    exposureIN->setRecommendedValues(exposureValues);
    connect(exposureIN, &NonLinearDoubleSpinBox::editingFinished, this, &Ekos::Guide::saveDefaultGuideExposure);

    // Guide Delay
    connect(GuideDelay, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        Options::setGuideDelay(GuideDelay->value());
    });

    // Set current guide type
    setGuiderType(-1);

    //This allows the current guideSubframe option to be loaded.
    if(guiderType == GUIDE_PHD2)
    {
        setExternalGuiderBLOBEnabled(!Options::guideSubframeEnabled());
    }
    else
    {
        // These only apply to PHD2, so disabling them when using the internal guider.
        opsDither->kcfg_DitherTimeout->setEnabled(false);
        opsDither->kcfg_DitherThreshold->setEnabled(false);
        opsDither->kcfg_DitherMaxIterations->setEnabled(!Options::ditherWithOnePulse());
    }

    // Initialize non guided dithering random generator.
    resetNonGuidedDither();

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    connect(KStars::Instance(), &KStars::colorSchemeChanged, driftGraph, &GuideDriftGraph::refreshColorScheme);

    m_DarkProcessor = new DarkProcessor(this);
    connect(m_DarkProcessor, &DarkProcessor::newLog, this, &Ekos::Guide::appendLogText);
    connect(m_DarkProcessor, &DarkProcessor::darkFrameCompleted, this, [this](bool completed)
    {
        if (completed != darkFrameCheck->isChecked())
            setDarkFrameEnabled(completed);
        m_GuideView->setProperty("suspended", false);
        if (completed)
        {
            m_GuideView->rescale(ZOOM_KEEP_LEVEL);
            m_GuideView->updateFrame();
        }
        m_GuideView->updateFrame();
        setCaptureComplete();
    });
}

Guide::~Guide()
{
    delete m_GuiderInstance;
}

void Guide::handleHorizontalPlotSizeChange()
{
    targetPlot->handleHorizontalPlotSizeChange();
    calibrationPlot->xAxis->setScaleRatio(calibrationPlot->yAxis, 1.0);
    calibrationPlot->replot();
}

void Guide::handleVerticalPlotSizeChange()
{
    targetPlot->handleVerticalPlotSizeChange();
    calibrationPlot->yAxis->setScaleRatio(calibrationPlot->xAxis, 1.0);
    calibrationPlot->replot();
}

void Guide::guideAfterMeridianFlip()
{
    //This will clear the tracking box selection
    //The selected guide star is no longer valid due to the flip
    m_GuideView->setTrackingBoxEnabled(false);
    starCenter = QVector3D();

    if (Options::resetGuideCalibration())
        clearCalibration();

    // GPG guide algorithm should be reset on any slew.
    if (Options::gPGEnabled())
        m_GuiderInstance->resetGPG();

    guide();
}

void Guide::resizeEvent(QResizeEvent *event)
{
    if (event->oldSize().width() != -1)
    {
        if (event->oldSize().width() != size().width())
            handleHorizontalPlotSizeChange();
        else if (event->oldSize().height() != size().height())
            handleVerticalPlotSizeChange();
    }
    else
    {
        QTimer::singleShot(10, this, &Ekos::Guide::handleHorizontalPlotSizeChange);
    }
}

void Guide::buildTarget()
{
    double accuracyRadius = accuracyRadiusSpin->value();
    Options::setGuiderAccuracyThreshold(accuracyRadius);
    targetPlot->buildTarget(accuracyRadius);
}

void Guide::clearGuideGraphs()
{
    driftGraph->clear();
    targetPlot->clear();
}

void Guide::clearCalibrationGraphs()
{
    calibrationPlot->graph(GuideGraph::G_RA)->data()->clear(); //RA out
    calibrationPlot->graph(GuideGraph::G_DEC)->data()->clear(); //RA back
    calibrationPlot->graph(GuideGraph::G_RA_HIGHLIGHT)->data()->clear(); //Backlash
    calibrationPlot->graph(GuideGraph::G_DEC_HIGHLIGHT)->data()->clear(); //DEC out
    calibrationPlot->graph(GuideGraph::G_RA_PULSE)->data()->clear(); //DEC back
    calibrationPlot->replot();
}

void Guide::slotAutoScaleGraphs()
{
    driftGraph->zoomX(defaultXZoomLevel);

    driftGraph->autoScaleGraphs();
    targetPlot->autoScaleGraphs(accuracyRadiusSpin->value());

    calibrationPlot->rescaleAxes();
    calibrationPlot->yAxis->setScaleRatio(calibrationPlot->xAxis, 1.0);
    calibrationPlot->xAxis->setScaleRatio(calibrationPlot->yAxis, 1.0);
    calibrationPlot->replot();
}

void Guide::guideHistory()
{
    int sliderValue = guideSlider->value();
    latestCheck->setChecked(sliderValue == guideSlider->maximum() - 1 || sliderValue == guideSlider->maximum());
    double ra = driftGraph->graph(GuideGraph::G_RA)->dataMainValue(sliderValue); //Get RA from RA data
    double de = driftGraph->graph(GuideGraph::G_DEC)->dataMainValue(sliderValue); //Get DEC from DEC data
    driftGraph->guideHistory(sliderValue, graphOnLatestPt);

    targetPlot->showPoint(ra, de);
}

void Guide::setLatestGuidePoint(bool isChecked)
{
    graphOnLatestPt = isChecked;
    driftGraph->setLatestGuidePoint(isChecked);
    targetPlot->setLatestGuidePoint(isChecked);

    if(isChecked)
        guideSlider->setValue(guideSlider->maximum());
}

QString Guide::setRecommendedExposureValues(QList<double> values)
{
    exposureIN->setRecommendedValues(values);
    return exposureIN->getRecommendedValuesString();
}

void Guide::addCamera(ISD::Camera *device)
{
    // No duplicates
    for (auto &oneCamera : m_Cameras)
    {
        if (oneCamera->getDeviceName() == device->getDeviceName())
            return;
    }

    for (auto &oneCamera : m_Cameras)
        oneCamera->disconnect(this);

    m_Camera = device;
    m_Cameras.append(device);

    if(guiderType != GUIDE_INTERNAL)
    {
        //        connect(ccd, &ISD::Camera::newBLOBManager, [ccd, this](INDI::Property prop)
        //        {
        //            if (prop->isNameMatch("CCD1") ||  prop->isNameMatch("CCD2"))
        //            {
        //                ccd->setBLOBEnabled(false); //This will disable PHD2 external guide frames until it is properly connected.
        //                m_Camera = ccd;
        //            }
        //        });
        m_Camera->setBLOBEnabled(false);
        cameraCombo->clear();
        cameraCombo->setEnabled(false);
        if (guiderType == GUIDE_PHD2)
            cameraCombo->addItem("PHD2");
        else
            cameraCombo->addItem("LinGuider");
    }
    else
    {
        cameraCombo->setEnabled(true);
        cameraCombo->addItem(m_Camera->getDeviceName());
        if (device->hasGuideHead())
            addGuideHead(device);
        //        bool rc = false;
        //        if (Options::defaultGuideCCD().isEmpty() == false)
        //            rc = setCamera(Options::defaultGuideCCD());
        //        if (rc == false)
        //            setCamera(QString(device->getDeviceName()) + QString(" Guider"));
    }

    checkCamera();
    configurePHD2Camera();
}

void Guide::configurePHD2Camera()
{
    //Maybe something like this can be done for Linguider?
    //But for now, Linguider doesn't support INDI Cameras
    if(guiderType != GUIDE_PHD2)
        return;
    //This prevents a crash if phd2guider is null
    if(!phd2Guider)
        return;
    //This way it doesn't check if the equipment isn't connected yet.
    //It will check again when the equipment is connected.
    if(!phd2Guider->isConnected())
        return;
    //This way it doesn't check if the equipment List has not been received yet.
    //It will ask for the list.  When the list is received it will check again.
    if(phd2Guider->getCurrentCamera().isEmpty())
    {
        phd2Guider->requestCurrentEquipmentUpdate();
        return;
    }

    //this checks to see if a CCD in the list matches the name of PHD2's camera
    ISD::Camera *ccdMatch = nullptr;
    QString currentPHD2CameraName = "None";
    for(auto &oneCamera : m_Cameras)
    {
        if(phd2Guider->getCurrentCamera().contains(oneCamera->getDeviceName()))
        {
            ccdMatch = oneCamera;
            currentPHD2CameraName = (phd2Guider->getCurrentCamera());
            break;
        }
    }

    //If this method gives the same result as last time, no need to update the Camera info again.
    //That way the user doesn't see a ton of messages printing about the PHD2 external camera.
    //But lets make sure the blob is set correctly every time.
    if(lastPHD2CameraName == currentPHD2CameraName)
    {
        setExternalGuiderBLOBEnabled(!Options::guideSubframeEnabled());
        return;
    }

    //This means that a Guide Camera was connected before but it changed.
    if(m_Camera)
        setExternalGuiderBLOBEnabled(false);

    //Updating the currentCCD
    m_Camera = ccdMatch;

    //This updates the last camera name for the next time it is checked.
    lastPHD2CameraName = currentPHD2CameraName;

    //This sets a boolean that allows you to tell if the PHD2 camera is in Ekos
    phd2Guider->setCurrentCameraIsNotInEkos(m_Camera == nullptr);

    if(phd2Guider->isCurrentCameraNotInEkos())
    {
        appendLogText(
            i18n("PHD2's current camera: %1, is NOT connected to Ekos.  The PHD2 Guide Star Image will be received, but the full external guide frames cannot.",
                 phd2Guider->getCurrentCamera()));
        subFrameCheck->setEnabled(false);
        //We don't want to actually change the user's subFrame Setting for when a camera really is connected, just check the box to tell the user.
        disconnect(subFrameCheck, &QCheckBox::toggled, this, &Ekos::Guide::setSubFrameEnabled);
        subFrameCheck->setChecked(true);
        return;
    }

    appendLogText(
        i18n("PHD2's current camera: %1, IS connected to Ekos.  You can select whether to use the full external guide frames or just receive the PHD2 Guide Star Image using the SubFrame checkbox.",
             phd2Guider->getCurrentCamera()));
    subFrameCheck->setEnabled(true);
    connect(subFrameCheck, &QCheckBox::toggled, this, &Ekos::Guide::setSubFrameEnabled);
    subFrameCheck->setChecked(Options::guideSubframeEnabled());
}

void Guide::addGuideHead(ISD::Camera *device)
{
    if (guiderType != GUIDE_INTERNAL)
        return;

    m_Cameras.append(device);

    QString guiderName = device->getDeviceName() + QString(" Guider");

    if (cameraCombo->findText(guiderName) == -1)
    {
        cameraCombo->addItem(guiderName);
    }
}

void Guide::addMount(ISD::Mount *device)
{
    // No duplicates
    for (auto &oneMount : m_Mounts)
    {
        if (oneMount->getDeviceName() == device->getDeviceName())
            return;
    }

    for (auto &oneMount : m_Mounts)
        oneMount->disconnect(this);

    m_Mount = device;
    m_Mounts.append(device);
    syncTelescopeInfo();
}

bool Guide::setCamera(const QString &device)
{
    if (guiderType != GUIDE_INTERNAL)
        return true;

    for (int i = 0; i < cameraCombo->count(); i++)
        if (device == cameraCombo->itemText(i))
        {
            cameraCombo->setCurrentIndex(i);
            checkCamera(i);
            // Set requested binning in INDIDRiver of the camera selected for guiding
            updateCCDBin(guideBinIndex);
            return true;
        }
    // If we choose new profile with a new guider camera it will not be found because the default
    // camera in 'kstarscfg' is still the old one and will be updated only if we select the new one
    // in the 'Guider'pulldown menu. So we cannnot set binning in INDIDriver. As the default binning
    // of the new camera is mostly 1x1 binning is set to 1x1 to prevent false error report of
    // binning support in 'processCCDNumber'.
    //pmneo: Do net reset stored bin setting, this should be solved in the processCCDNumber
    //guideBinIndex = 0;

    return false;
}

QString Guide::camera()
{
    if (m_Camera)
        return m_Camera->getDeviceName();

    return QString();
}

void Guide::checkCamera(int ccdNum)
{
    // Do NOT perform checks when the camera is capturing as this may result
    // in signals/slots getting disconnected.
    if (guiderType != GUIDE_INTERNAL)
        return;

    switch (m_State)
    {
        // Not busy, camera change is OK
        case GUIDE_IDLE:
        case GUIDE_ABORTED:
        case GUIDE_CONNECTED:
        case GUIDE_DISCONNECTED:
        case GUIDE_CALIBRATION_ERROR:
            break;

        // Busy, camera change is not OK
        case GUIDE_CAPTURE:
        case GUIDE_LOOPING:
        case GUIDE_DARK:
        case GUIDE_SUBFRAME:
        case GUIDE_STAR_SELECT:
        case GUIDE_CALIBRATING:
        case GUIDE_CALIBRATION_SUCCESS:
        case GUIDE_GUIDING:
        case GUIDE_SUSPENDED:
        case GUIDE_REACQUIRE:
        case GUIDE_DITHERING:
        case GUIDE_MANUAL_DITHERING:
        case GUIDE_DITHERING_ERROR:
        case GUIDE_DITHERING_SUCCESS:
        case GUIDE_DITHERING_SETTLE:
            return;
    }

    if (ccdNum == -1)
    {
        ccdNum = cameraCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum <= m_Cameras.count())
    {
        m_Camera = m_Cameras.at(ccdNum);

        if (m_Camera->hasGuideHead() && cameraCombo->currentText().contains("Guider"))
            useGuideHead = true;
        else
            useGuideHead = false;

        ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        if (!targetChip)
        {
            qCCritical(KSTARS_EKOS_GUIDE) << "Failed to retrieve active guide chip in camera";
            return;
        }

        if (targetChip->isCapturing())
            return;

        if (guiderType != GUIDE_INTERNAL)
        {
            syncCameraInfo();
            return;
        }

        // Make sure to disconnect all m_Cameras first from slots of Ekos::Guide
        for (auto &oneCamera : m_Cameras)
            oneCamera->disconnect(this);

        connect(m_Camera, &ISD::Camera::numberUpdated, this, &Ekos::Guide::processCCDNumber, Qt::UniqueConnection);
        connect(m_Camera, &ISD::Camera::newExposureValue, this, &Ekos::Guide::checkExposureValue, Qt::UniqueConnection);

        syncCameraInfo();
    }
}

void Guide::syncCameraInfo()
{
    if (!m_Camera)
        return;

    auto nvp = m_Camera->getNumber(useGuideHead ? "GUIDER_INFO" : "CCD_INFO");

    if (nvp)
    {
        auto np = nvp->findWidgetByName("CCD_PIXEL_SIZE_X");
        if (np)
            ccdPixelSizeX = np->getValue();

        np = nvp->findWidgetByName( "CCD_PIXEL_SIZE_Y");
        if (np)
            ccdPixelSizeY = np->getValue();

        np = nvp->findWidgetByName("CCD_PIXEL_SIZE_Y");
        if (np)
            ccdPixelSizeY = np->getValue();
    }

    updateGuideParams();
}

void Guide::setTelescopeInfo(double primaryFocalLength, double primaryAperture, double guideFocalLength,
                             double guideAperture)
{
    if (primaryFocalLength > 0)
        focal_length = primaryFocalLength;
    if (primaryAperture > 0)
        aperture = primaryAperture;
    // If we have guide scope info, always prefer that over primary
    if (guideFocalLength > 0)
        focal_length = guideFocalLength;
    if (guideAperture > 0)
        aperture = guideAperture;

    updateGuideParams();
}

void Guide::syncTelescopeInfo()
{
    if (m_Mount == nullptr || m_Mount->isConnected() == false)
        return;

    auto nvp = m_Mount->getNumber("TELESCOPE_INFO");

    if (nvp)
    {
        auto np = nvp->findWidgetByName("TELESCOPE_APERTURE");

        if (np && np->getValue() > 0)
            primaryAperture = np->getValue();

        np = nvp->findWidgetByName("GUIDER_APERTURE");
        if (np && np->getValue() > 0)
            guideAperture = np->getValue();

        aperture = primaryAperture;

        //if (currentCCD && currentCCD->getTelescopeType() == ISD::Camera::TELESCOPE_GUIDE)
        if (FOVScopeCombo->currentIndex() == ISD::Camera::TELESCOPE_GUIDE)
            aperture = guideAperture;

        np = nvp->findWidgetByName("TELESCOPE_FOCAL_LENGTH");
        if (np && np->getValue() > 0)
            primaryFL = np->getValue();

        np = nvp->findWidgetByName("GUIDER_FOCAL_LENGTH");
        if (np && np->getValue() > 0)
            guideFL = np->getValue();

        focal_length = primaryFL;

        //if (currentCCD && currentCCD->getTelescopeType() == ISD::Camera::TELESCOPE_GUIDE)
        if (FOVScopeCombo->currentIndex() == ISD::Camera::TELESCOPE_GUIDE)
            focal_length = guideFL;
    }

    updateGuideParams();
}

void Guide::updateGuideParams()
{
    if (m_Camera == nullptr)
        return;

    if (m_Camera->hasGuideHead() == false)
        useGuideHead = false;

    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

    if (targetChip == nullptr)
    {
        appendLogText(i18n("Connection to the guide CCD is lost."));
        return;
    }

    if (targetChip->getFrameType() != FRAME_LIGHT)
        return;

    if(guiderType == GUIDE_INTERNAL)
        binningCombo->setEnabled(targetChip->canBin());

    int subBinX = 1, subBinY = 1;
    if (targetChip->canBin())
    {
        int maxBinX, maxBinY;

        targetChip->getBinning(&subBinX, &subBinY);
        targetChip->getMaxBin(&maxBinX, &maxBinY);

        //override with stored guide bin index, if within the range of possible bin modes
        if( guideBinIndex >= 0 && guideBinIndex < maxBinX && guideBinIndex < maxBinY )
        {
            subBinX = guideBinIndex + 1;
            subBinY = guideBinIndex + 1;
        }

        guideBinIndex = subBinX - 1;

        binningCombo->blockSignals(true);

        binningCombo->clear();
        for (int i = 1; i <= maxBinX; i++)
            binningCombo->addItem(QString("%1x%2").arg(i).arg(i));

        binningCombo->setCurrentIndex( guideBinIndex );

        binningCombo->blockSignals(false);
    }

    // If frame setting does not exist, create a new one.
    if (frameSettings.contains(targetChip) == false)
    {
        int x, y, w, h;
        if (targetChip->getFrame(&x, &y, &w, &h))
        {
            if (w > 0 && h > 0)
            {
                int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
                targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

                QVariantMap settings;

                settings["x"]    = Options::guideSubframeEnabled() ? x : minX;
                settings["y"]    = Options::guideSubframeEnabled() ? y : minY;
                settings["w"]    = Options::guideSubframeEnabled() ? w : maxW;
                settings["h"]    = Options::guideSubframeEnabled() ? h : maxH;
                settings["binx"] = subBinX;
                settings["biny"] = subBinY;

                frameSettings[targetChip] = settings;
            }
        }
    }
    // Otherwise update existing map
    else
    {
        QVariantMap settings = frameSettings[targetChip];
        settings["binx"] = subBinX;
        settings["biny"] = subBinY;
        frameSettings[targetChip] = settings;
    }

    if (ccdPixelSizeX != -1 && ccdPixelSizeY != -1 && aperture != -1 && focal_length != -1)
    {
        FOVScopeCombo->setItemData(
            ISD::Camera::TELESCOPE_PRIMARY,
            i18nc("F-Number, Focal length, Aperture",
                  "<nobr>F<b>%1</b> Focal length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                  QString::number(primaryFL / primaryAperture, 'f', 1), QString::number(primaryFL, 'f', 2),
                  QString::number(primaryAperture, 'f', 2)),
            Qt::ToolTipRole);
        FOVScopeCombo->setItemData(
            ISD::Camera::TELESCOPE_GUIDE,
            i18nc("F-Number, Focal length, Aperture",
                  "<nobr>F<b>%1</b> Focal length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                  QString::number(guideFL / guideAperture, 'f', 1), QString::number(guideFL, 'f', 2),
                  QString::number(guideAperture, 'f', 2)),
            Qt::ToolTipRole);

        m_GuiderInstance->setGuiderParams(ccdPixelSizeX, ccdPixelSizeY, aperture, focal_length);
        emit guideChipUpdated(targetChip);

        int x, y, w, h;
        if (targetChip->getFrame(&x, &y, &w, &h))
        {
            m_GuiderInstance->setFrameParams(x, y, w, h, subBinX, subBinY);
        }

        l_Focal->setText(QString::number(focal_length, 'f', 1));
        l_Aperture->setText(QString::number(aperture, 'f', 1));
        if (aperture == 0)
        {
            l_FbyD->setText("0");
            // Pixel scale in arcsec/pixel
            pixScaleX = 0;
            pixScaleY = 0;
        }
        else
        {
            l_FbyD->setText(QString::number(focal_length / aperture, 'f', 1));
            // Pixel scale in arcsec/pixel
            pixScaleX = 206264.8062470963552 * ccdPixelSizeX / 1000.0 / focal_length;
            pixScaleY = 206264.8062470963552 * ccdPixelSizeY / 1000.0 / focal_length;
        }

        // FOV in arcmin
        double fov_w = (w * pixScaleX) / 60.0;
        double fov_h = (h * pixScaleY) / 60.0;

        l_FOV->setText(QString("%1x%2").arg(QString::number(fov_w, 'f', 1), QString::number(fov_h, 'f', 1)));
    }
}

void Guide::addGuider(ISD::Guider *device)
{
    if (guiderType != GUIDE_INTERNAL)
        return;

    // No duplicates
    for (auto &oneGuider : m_Guiders)
    {
        if (oneGuider->getDeviceName() == device->getDeviceName())
            return;
    }

    for (auto &oneGuider : m_Guiders)
        oneGuider->disconnect(this);

    m_Guider = device;
    m_Guiders.append(device);
    guiderCombo->addItem(device->getDeviceName());

    setGuider(0);
}

bool Guide::setGuider(const QString &device)
{
    if (guiderType != GUIDE_INTERNAL)
        return true;

    for (int i = 0; i < m_Guiders.count(); i++)
        if (m_Guiders.at(i)->getDeviceName() == device)
        {
            guiderCombo->setCurrentIndex(i);
            setGuider(i);
            return true;
        }

    return false;
}

QString Guide::guider()
{
    if (guiderType != GUIDE_INTERNAL || guiderCombo->currentIndex() == -1)
        return QString();

    return guiderCombo->currentText();
}

void Guide::setGuider(int index)
{
    if (m_Guiders.empty() || index >= m_Guiders.count() || guiderType != GUIDE_INTERNAL)
        return;

    m_Guider = m_Guiders.at(index);
}

bool Guide::capture()
{
    buildOperationStack(GUIDE_CAPTURE);

    return executeOperationStack();
}

bool Guide::captureOneFrame()
{
    captureTimeout.stop();

    if (m_Camera == nullptr)
        return false;

    if (m_Camera->isConnected() == false)
    {
        appendLogText(i18n("Error: lost connection to CCD."));
        return false;
    }

    // If CCD Telescope Type does not match desired scope type, change it
    if (m_Camera->getTelescopeType() != FOVScopeCombo->currentIndex())
        m_Camera->setTelescopeType(static_cast<ISD::Camera::TelescopeType>(FOVScopeCombo->currentIndex()));

    double seqExpose = exposureIN->value();

    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

    prepareCapture(targetChip);

    m_GuideView->setBaseSize(guideWidget->size());
    setBusy(true);

    // Check if we have a valid frame setting
    if (frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];
        targetChip->setFrame(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(),
                             settings["h"].toInt());
        targetChip->setBinning(settings["binx"].toInt(), settings["biny"].toInt());
    }

    connect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Guide::processData, Qt::UniqueConnection);
    qCDebug(KSTARS_EKOS_GUIDE) << "Capturing frame...";

    double finalExposure = seqExpose;

    // Increase exposure for calibration frame if we need auto-select a star
    // To increase chances we detect one.
    if (operationStack.contains(GUIDE_STAR_SELECT) && Options::guideAutoStarEnabled() &&
            !((guiderType == GUIDE_INTERNAL) && internalGuider->SEPMultiStarEnabled()))
        finalExposure *= 3;

    // Prevent flicker when processing dark frame by suspending updates
    m_GuideView->setProperty("suspended", operationStack.contains(GUIDE_DARK));

    // Timeout is exposure duration + timeout threshold in seconds
    captureTimeout.start(finalExposure * 1000 + CAPTURE_TIMEOUT_THRESHOLD);

    targetChip->capture(finalExposure);

    return true;
}

void Guide::prepareCapture(ISD::CameraChip *targetChip)
{
    targetChip->setBatchMode(false);
    targetChip->setCaptureMode(FITS_GUIDE);
    targetChip->setFrameType(FRAME_LIGHT);
    targetChip->setCaptureFilter(FITS_NONE);
    m_Camera->setEncodingFormat("FITS");
}

bool Guide::abort()
{
    if (m_Camera && guiderType == GUIDE_INTERNAL)
    {
        captureTimeout.stop();
        m_PulseTimer.stop();
        ISD::CameraChip *targetChip =
            m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        if (targetChip->isCapturing())
            targetChip->abortExposure();
    }

    manualDitherB->setEnabled(false);

    setBusy(false);

    switch (m_State)
    {
        case GUIDE_IDLE:
        case GUIDE_CONNECTED:
        case GUIDE_DISCONNECTED:
            break;

        case GUIDE_CALIBRATING:
        case GUIDE_DITHERING:
        case GUIDE_STAR_SELECT:
        case GUIDE_CAPTURE:
        case GUIDE_GUIDING:
        case GUIDE_LOOPING:
            m_GuiderInstance->abort();
            break;

        default:
            break;
    }

    return true;
}

void Guide::setBusy(bool enable)
{
    if (enable && pi->isAnimated())
        return;
    else if (enable == false && pi->isAnimated() == false)
        return;

    if (enable)
    {
        clearCalibrationB->setEnabled(false);
        guideB->setEnabled(false);
        captureB->setEnabled(false);
        loopB->setEnabled(false);

        darkFrameCheck->setEnabled(false);
        subFrameCheck->setEnabled(false);
        autoStarCheck->setEnabled(false);

        stopB->setEnabled(true);

        pi->startAnimation();

        //disconnect(guideView, SIGNAL(trackingStarSelected(int,int)), this, &Ekos::Guide::setTrackingStar(int,int)));
    }
    else
    {
        if(guiderType != GUIDE_LINGUIDER)
        {
            captureB->setEnabled(true);
            loopB->setEnabled(true);
            autoStarCheck->setEnabled(!internalGuider->SEPMultiStarEnabled()); // cf. configSEPMultistarOptions()
            if(m_Camera)
                subFrameCheck->setEnabled(!internalGuider->SEPMultiStarEnabled()); // cf. configSEPMultistarOptions()
        }
        if (guiderType == GUIDE_INTERNAL)
            darkFrameCheck->setEnabled(true);

        if (calibrationComplete ||
                ((guiderType == GUIDE_INTERNAL) &&
                 Options::reuseGuideCalibration() &&
                 !Options::serializedCalibration().isEmpty()))
            clearCalibrationB->setEnabled(true);
        guideB->setEnabled(true);
        stopB->setEnabled(false);
        pi->stopAnimation();

        connect(m_GuideView.get(), &FITSView::trackingStarSelected, this, &Ekos::Guide::setTrackingStar, Qt::UniqueConnection);
    }
}

void Guide::processCaptureTimeout()
{
    auto restartExposure = [&]()
    {
        appendLogText(i18n("Exposure timeout. Restarting exposure..."));
        m_Camera->setEncodingFormat("FITS");
        ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        targetChip->abortExposure();
        prepareCapture(targetChip);
        targetChip->capture(exposureIN->value());
        captureTimeout.start(exposureIN->value() * 1000 + CAPTURE_TIMEOUT_THRESHOLD);
    };

    m_CaptureTimeoutCounter++;

    if (m_Camera == nullptr)
        return;

    if (m_DeviceRestartCounter >= 3)
    {
        m_CaptureTimeoutCounter = 0;
        m_DeviceRestartCounter = 0;
        if (m_State == GUIDE_GUIDING)
            appendLogText(i18n("Exposure timeout. Aborting Autoguide."));
        else if (m_State == GUIDE_DITHERING)
            appendLogText(i18n("Exposure timeout. Aborting Dithering."));
        else if (m_State == GUIDE_CALIBRATING)
            appendLogText(i18n("Exposure timeout. Aborting Calibration."));

        captureTimeout.stop();
        abort();
        return;
    }

    if (m_CaptureTimeoutCounter > 1)
    {
        QString camera = m_Camera->getDeviceName();
        QString via = m_Guider ? m_Guider->getDeviceName() : "";
        ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        QVariantMap settings = frameSettings[targetChip];
        emit driverTimedout(camera);
        QTimer::singleShot(5000, [ &, camera, via, settings]()
        {
            m_DeviceRestartCounter++;
            reconnectDriver(camera, via, settings);
        });
        return;
    }

    else
        restartExposure();
}

void Guide::reconnectDriver(const QString &camera, const QString &via, const QVariantMap &settings)
{
    for (auto &oneCamera : m_Cameras)
    {
        if (oneCamera->getDeviceName() == camera)
        {
            // Set camera again to the one we restarted
            cameraCombo->setCurrentIndex(cameraCombo->findText(camera));
            guiderCombo->setCurrentIndex(guiderCombo->findText(via));

            // Set state to IDLE so that checkCamera is processed since it will not process GUIDE_GUIDING state.
            Ekos::GuideState currentState = m_State;
            m_State = GUIDE_IDLE;
            checkCamera();
            // Restore state to last state.
            m_State = currentState;

            if (guiderType == GUIDE_INTERNAL)
            {
                // Reset the frame settings to the restarted camera once again before capture.
                ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
                frameSettings[targetChip] = settings;
                // restart capture
                m_CaptureTimeoutCounter = 0;
                captureOneFrame();
            }

            return;
        }
    }

    QTimer::singleShot(5000, this, [ &, camera, via, settings]()
    {
        reconnectDriver(camera, via, settings);
    });
}

void Guide::processData(const QSharedPointer<FITSData> &data)
{
    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
    if (targetChip->getCaptureMode() != FITS_GUIDE)
    {
        if (data)
        {
            QString blobInfo = QString("{Device: %1 Property: %2 Element: %3 Chip: %4}").arg(data->property("device").toString())
                               .arg(data->property("blobVector").toString())
                               .arg(data->property("blobElement").toString())
                               .arg(data->property("chip").toInt());

            qCWarning(KSTARS_EKOS_GUIDE) << blobInfo << "Ignoring Received FITS as it has the wrong capture mode" <<
                                         targetChip->getCaptureMode();
        }

        return;
    }

    if (data)
    {
        m_GuideView->loadData(data);
        m_ImageData = data;
    }
    else
        m_ImageData.reset();

    if (guiderType == GUIDE_INTERNAL)
        internalGuider->setImageData(m_ImageData);

    captureTimeout.stop();
    m_CaptureTimeoutCounter = 0;

    disconnect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Guide::processData);

    qCDebug(KSTARS_EKOS_GUIDE) << "Received guide frame.";

    int subBinX = 1, subBinY = 1;
    targetChip->getBinning(&subBinX, &subBinY);

    if (starCenter.x() == 0 && starCenter.y() == 0)
    {
        int x = 0, y = 0, w = 0, h = 0;

        if (frameSettings.contains(targetChip))
        {
            QVariantMap settings = frameSettings[targetChip];
            x                    = settings["x"].toInt();
            y                    = settings["y"].toInt();
            w                    = settings["w"].toInt();
            h                    = settings["h"].toInt();
        }
        else
            targetChip->getFrame(&x, &y, &w, &h);

        starCenter.setX(w / (2 * subBinX));
        starCenter.setY(h / (2 * subBinY));
        starCenter.setZ(subBinX);
    }

    syncTrackingBoxPosition();

    setCaptureComplete();
}

void Guide::setCaptureComplete()
{
    if (!m_GuideView.isNull())
        m_GuideView->clearNeighbors();

    DarkLibrary::Instance()->disconnect(this);

    if (operationStack.isEmpty() == false)
    {
        executeOperationStack();
        return;
    }

    switch (m_State)
    {
        case GUIDE_IDLE:
        case GUIDE_ABORTED:
        case GUIDE_CONNECTED:
        case GUIDE_DISCONNECTED:
        case GUIDE_CALIBRATION_SUCCESS:
        case GUIDE_CALIBRATION_ERROR:
        case GUIDE_DITHERING_ERROR:
            setBusy(false);
            break;

        case GUIDE_CAPTURE:
            m_State = GUIDE_IDLE;
            emit newStatus(m_State);
            setBusy(false);
            break;

        case GUIDE_LOOPING:
            capture();
            break;

        case GUIDE_CALIBRATING:
            m_GuiderInstance->calibrate();
            break;

        case GUIDE_GUIDING:
            m_GuiderInstance->guide();
            break;

        case GUIDE_DITHERING:
            m_GuiderInstance->dither(Options::ditherPixels());
            break;

        // Feature only of internal guider
        case GUIDE_MANUAL_DITHERING:
            dynamic_cast<InternalGuider*>(m_GuiderInstance)->processManualDithering();
            break;

        case GUIDE_REACQUIRE:
            m_GuiderInstance->reacquire();
            break;

        case GUIDE_DITHERING_SETTLE:
            if (Options::ditherNoGuiding())
                return;
            capture();
            break;

        case GUIDE_SUSPENDED:
            if (Options::gPGEnabled())
                m_GuiderInstance->guide();
            break;

        default:
            break;
    }

    emit newImage(m_GuideView);
    emit newStarPixmap(m_GuideView->getTrackingBoxPixmap(10));
}

void Guide::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_GUIDE) << text;

    emit newLog(text);
}

void Guide::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

void Guide::setDECSwap(bool enable)
{
    if (m_Guider == nullptr || m_GuiderInstance == nullptr)
        return;

    if (guiderType == GUIDE_INTERNAL)
    {
        dynamic_cast<InternalGuider *>(m_GuiderInstance)->setDECSwap(enable);
        m_Guider->setDECSwap(enable);
    }
}

bool Guide::sendMultiPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs)
{
    if (m_Guider == nullptr || (ra_dir == NO_DIR && dec_dir == NO_DIR))
        return false;

    // Delay next capture by user-configurable delay.
    // If user delay is zero, delay by the pulse length plus 100 milliseconds before next capture.
    auto ms = std::max(ra_msecs, dec_msecs) + 100;
    auto delay = std::max(static_cast<int>(Options::guideDelay() * 1000), ms);

    m_PulseTimer.start(delay);

    return m_Guider->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
}

bool Guide::sendSinglePulse(GuideDirection dir, int msecs)
{
    if (m_Guider == nullptr || dir == NO_DIR)
        return false;

    // Delay next capture by user-configurable delay.
    // If user delay is zero, delay by the pulse length plus 100 milliseconds before next capture.
    auto ms = msecs + 100;
    auto delay = std::max(static_cast<int>(Options::guideDelay() * 1000), ms);

    m_PulseTimer.start(delay);

    return m_Guider->doPulse(dir, msecs);
}

QStringList Guide::getGuiders()
{
    QStringList devices;

    for (auto &oneGuider : m_Guiders)
        devices << oneGuider->getDeviceName();

    return devices;
}


bool Guide::calibrate()
{
    // Set status to idle and let the operations change it as they get executed
    m_State = GUIDE_IDLE;
    emit newStatus(m_State);

    if (guiderType == GUIDE_INTERNAL)
    {
        ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

        if (frameSettings.contains(targetChip))
        {
            targetChip->resetFrame();
            int x, y, w, h;
            targetChip->getFrame(&x, &y, &w, &h);
            QVariantMap settings      = frameSettings[targetChip];
            settings["x"]             = x;
            settings["y"]             = y;
            settings["w"]             = w;
            settings["h"]             = h;
            frameSettings[targetChip] = settings;

            subFramed = false;
        }
    }

    saveSettings();

    buildOperationStack(GUIDE_CALIBRATING);

    executeOperationStack();

    qCDebug(KSTARS_EKOS_GUIDE) << "Starting calibration using CCD:" << m_Camera->getDeviceName() << "via" <<
                               guiderCombo->currentText();

    return true;
}

bool Guide::guide()
{
    auto executeGuide = [this]()
    {
        if(guiderType != GUIDE_PHD2)
        {
            if (calibrationComplete == false)
            {
                calibrate();
                return;
            }
        }

        saveSettings();
        m_GuiderInstance->guide();

        //If PHD2 gets a Guide command and it is looping, it will accept a lock position
        //but if it was not looping it will ignore the lock position and do an auto star automatically
        //This is not the default behavior in Ekos if auto star is not selected.
        //This gets around that by noting the position of the tracking box, and enforcing it after the state switches to guide.
        if(!Options::guideAutoStarEnabled())
        {
            if(guiderType == GUIDE_PHD2 && m_GuideView->isTrackingBoxEnabled())
            {
                double x = starCenter.x();
                double y = starCenter.y();

                if(!m_ImageData.isNull())
                {
                    if(m_ImageData->width() > 50)
                    {
                        guideConnect = connect(this, &Guide::newStatus, this, [this, x, y](Ekos::GuideState newState)
                        {
                            if(newState == GUIDE_GUIDING)
                            {
                                phd2Guider->setLockPosition(x, y);
                                disconnect(guideConnect);
                            }
                        });
                    }
                }
            }
        }
    };

    //    if (Options::defaultCaptureCCD() == cameraCombo->currentText())
    //    {
    //        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, executeGuide]()
    //        {
    //            //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
    //            KSMessageBox::Instance()->disconnect(this);
    //            executeGuide();
    //        });

    //        KSMessageBox::Instance()->questionYesNo(
    //            i18n("The guide camera is identical to the primary imaging camera. Are you sure you want to continue?"));

    //        return false;
    //    }

    if (m_MountStatus == ISD::Mount::MOUNT_PARKED)
    {
        KSMessageBox::Instance()->sorry(i18n("The mount is parked. Unpark to start guiding."));
        return false;
    }

    executeGuide();
    return true;
}

bool Guide::dither()
{
    if (Options::ditherNoGuiding() && m_State == GUIDE_IDLE)
    {
        nonGuidedDither();
        return true;
    }

    if (m_State == GUIDE_DITHERING || m_State == GUIDE_DITHERING_SETTLE)
        return true;

    //This adds a dither text item to the graph where dithering occurred.
    double time = guideTimer.elapsed() / 1000.0;
    QCPItemText *ditherLabel = new QCPItemText(driftGraph);
    ditherLabel->setPositionAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    ditherLabel->position->setType(QCPItemPosition::ptPlotCoords);
    ditherLabel->position->setCoords(time, 1.5);
    ditherLabel->setColor(Qt::white);
    ditherLabel->setBrush(Qt::NoBrush);
    ditherLabel->setPen(Qt::NoPen);
    ditherLabel->setText("Dither");
    ditherLabel->setFont(QFont(font().family(), 10));

    if (guiderType == GUIDE_INTERNAL)
    {
        if (m_State != GUIDE_GUIDING)
            capture();

        setStatus(GUIDE_DITHERING);

        return true;
    }
    else
        return m_GuiderInstance->dither(Options::ditherPixels());
}

bool Guide::suspend()
{
    if (m_State == GUIDE_SUSPENDED)
        return true;
    else if (m_State >= GUIDE_CAPTURE)
        return m_GuiderInstance->suspend();
    else
        return false;
}

bool Guide::resume()
{
    if (m_State == GUIDE_GUIDING)
        return true;
    else if (m_State == GUIDE_SUSPENDED)
        return m_GuiderInstance->resume();
    else
        return false;
}

void Guide::setCaptureStatus(CaptureState newState)
{
    switch (newState)
    {
        case CAPTURE_DITHERING:
            dither();
            break;
        case CAPTURE_IDLE:
        case CAPTURE_ABORTED:
            // We need to reset the non guided dithering status every time a new capture task is started (and not for every single capture).
            // The non dithering logic is a bit convoluted and controlled by the Capture module,
            // which calls Guide::setCaptureStatus(CAPTURE_DITHERING) when it wants guide to dither.
            // It actually calls newStatus(CAPTURE_DITHERING) in Capture::checkDithering(), but manager.cpp in Manager::connectModules() connects that to Guide::setCaptureStatus()).
            // So the only way to reset the non guided dithering prior to a new capture task is to put it here, when the Capture status moves to IDLE or ABORTED state.
            resetNonGuidedDither();
            break;
        default:
            break;
    }
}

void Guide::setPierSide(ISD::Mount::PierSide newSide)
{
    m_GuiderInstance->setPierSide(newSide);

    // If pier side changes in internal guider
    // and calibration was already done
    // then let's swap
    if (guiderType == GUIDE_INTERNAL &&
            m_State != GUIDE_GUIDING &&
            m_State != GUIDE_CALIBRATING &&
            calibrationComplete)
    {
        // Couldn't restore an old calibration if we call clearCalibration().
        if (Options::reuseGuideCalibration())
            calibrationComplete = false;
        else
        {
            clearCalibration();
            appendLogText(i18n("Pier side change detected. Clearing calibration."));
        }
    }
}

void Guide::setMountStatus(ISD::Mount::Status newState)
{
    m_MountStatus = newState;

    if (newState == ISD::Mount::MOUNT_PARKING || newState == ISD::Mount::MOUNT_SLEWING)
    {
        // reset the calibration if "Always reset calibration" is selected and the mount moves
        if (Options::resetGuideCalibration())
        {
            appendLogText(i18n("Mount is moving. Resetting calibration..."));
            clearCalibration();
        }
        else if (Options::reuseGuideCalibration() && (guiderType == GUIDE_INTERNAL))
        {
            // It will restore it with the reused one, and this way it reselects a guide star.
            calibrationComplete = false;
        }
        // GPG guide algorithm should be reset on any slew.
        if (Options::gPGEnabled())
            m_GuiderInstance->resetGPG();

        // If we're guiding, and the mount either slews or parks, then we abort.
        if (m_State == GUIDE_GUIDING || m_State == GUIDE_DITHERING)
        {
            if (newState == ISD::Mount::MOUNT_PARKING)
                appendLogText(i18n("Mount is parking. Aborting guide..."));
            else
                appendLogText(i18n("Mount is slewing. Aborting guide..."));

            abort();
        }
    }

    if (guiderType != GUIDE_INTERNAL)
        return;

    switch (newState)
    {
        case ISD::Mount::MOUNT_SLEWING:
        case ISD::Mount::MOUNT_PARKING:
        case ISD::Mount::MOUNT_MOVING:
            captureB->setEnabled(false);
            loopB->setEnabled(false);
            clearCalibrationB->setEnabled(false);
            break;

        default:
            if (pi->isAnimated() == false)
            {
                captureB->setEnabled(true);
                loopB->setEnabled(true);
                clearCalibrationB->setEnabled(true);
            }
    }
}

void Guide::setMountCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha)
{
    Q_UNUSED(ha);
    m_GuiderInstance->setMountCoords(position, pierSide);
}

void Guide::setExposure(double value)
{
    exposureIN->setValue(value);
}

void Guide::setCalibrationTwoAxis(bool enable)
{
    Options::setTwoAxisEnabled(enable);
}

void Guide::setCalibrationAutoStar(bool enable)
{
    autoStarCheck->setChecked(enable);
}

void Guide::setCalibrationAutoSquareSize(bool enable)
{
    Options::setGuideAutoSquareSizeEnabled(enable);
}

void Guide::setCalibrationPulseDuration(int pulseDuration)
{
    Options::setCalibrationPulseDuration(pulseDuration);
}

void Guide::setGuideBoxSizeIndex(int index)
{
    Options::setGuideSquareSizeIndex(index);
}

void Guide::setGuideAlgorithmIndex(int index)
{
    Options::setGuideAlgorithm(index);
}

void Guide::setSubFrameEnabled(bool enable)
{
    Options::setGuideSubframeEnabled(enable);
    if (subFrameCheck->isChecked() != enable)
        subFrameCheck->setChecked(enable);
    if(guiderType == GUIDE_PHD2)
        setExternalGuiderBLOBEnabled(!enable);
}


void Guide::setDitherSettings(bool enable, double value)
{
    Options::setDitherEnabled(enable);
    Options::setDitherPixels(value);
}


void Guide::clearCalibration()
{
    calibrationComplete = false;

    m_GuiderInstance->clearCalibration();

    appendLogText(i18n("Calibration is cleared."));
}

void Guide::setStatus(Ekos::GuideState newState)
{
    if (newState == m_State)
    {
        // pass through the aborted state
        if (newState == GUIDE_ABORTED)
            emit newStatus(m_State);
        return;
    }

    GuideState previousState = m_State;

    m_State = newState;
    emit newStatus(m_State);

    switch (m_State)
    {
        case GUIDE_CONNECTED:
            appendLogText(i18n("External guider connected."));
            externalConnectB->setEnabled(false);
            externalDisconnectB->setEnabled(true);
            clearCalibrationB->setEnabled(true);
            guideB->setEnabled(true);

            if(guiderType == GUIDE_PHD2)
            {
                captureB->setEnabled(true);
                loopB->setEnabled(true);
                autoStarCheck->setEnabled(true);
                configurePHD2Camera();
                setExternalGuiderBLOBEnabled(!Options::guideSubframeEnabled());
                boxSizeCombo->setEnabled(true);
            }
            break;

        case GUIDE_DISCONNECTED:
            appendLogText(i18n("External guider disconnected."));
            setBusy(false); //This needs to come before caputureB since it will set it to enabled again.
            externalConnectB->setEnabled(true);
            externalDisconnectB->setEnabled(false);
            clearCalibrationB->setEnabled(false);
            guideB->setEnabled(false);
            captureB->setEnabled(false);
            loopB->setEnabled(false);
            autoStarCheck->setEnabled(false);
            boxSizeCombo->setEnabled(false);
            //setExternalGuiderBLOBEnabled(true);
#ifdef Q_OS_OSX
            repaint(); //This is a band-aid for a bug in QT 5.10.0
#endif
            break;

        case GUIDE_CALIBRATION_SUCCESS:
            appendLogText(i18n("Calibration completed."));
            calibrationComplete = true;

            if(guiderType !=
                    GUIDE_PHD2) //PHD2 will take care of this.  If this command is executed for PHD2, it might start guiding when it is first connected, if the calibration was completed already.
                guide();
            break;

        case GUIDE_IDLE:
        case GUIDE_CALIBRATION_ERROR:
            setBusy(false);
            manualDitherB->setEnabled(false);
            break;

        case GUIDE_CALIBRATING:
            clearCalibrationGraphs();
            appendLogText(i18n("Calibration started."));
            setBusy(true);
            break;

        case GUIDE_GUIDING:
            if (previousState == GUIDE_SUSPENDED || previousState == GUIDE_DITHERING_SUCCESS)
                appendLogText(i18n("Guiding resumed."));
            else
            {
                appendLogText(i18n("Autoguiding started."));
                setBusy(true);

                clearGuideGraphs();
                guideTimer.start();
                driftGraph->resetTimer();
                driftGraph->refreshColorScheme();
            }
            manualDitherB->setEnabled(true);

            break;

        case GUIDE_ABORTED:
            appendLogText(i18n("Autoguiding aborted."));
            setBusy(false);
            break;

        case GUIDE_SUSPENDED:
            appendLogText(i18n("Guiding suspended."));
            break;

        case GUIDE_REACQUIRE:
            if (guiderType == GUIDE_INTERNAL)
                capture();
            break;

        case GUIDE_MANUAL_DITHERING:
            appendLogText(i18n("Manual dithering in progress."));
            break;

        case GUIDE_DITHERING:
            appendLogText(i18n("Dithering in progress."));
            break;

        case GUIDE_DITHERING_SETTLE:
            appendLogText(i18np("Post-dither settling for %1 second...", "Post-dither settling for %1 seconds...",
                                Options::ditherSettle()));
            break;

        case GUIDE_DITHERING_ERROR:
            appendLogText(i18n("Dithering failed."));
            // LinGuider guide continue after dithering failure
            if (guiderType != GUIDE_LINGUIDER)
            {
                //state = GUIDE_IDLE;
                m_State = GUIDE_ABORTED;
                setBusy(false);
            }
            break;

        case GUIDE_DITHERING_SUCCESS:
            appendLogText(i18n("Dithering completed successfully."));
            // Go back to guiding state immediately if using regular guider
            if (Options::ditherNoGuiding() == false)
            {
                setStatus(GUIDE_GUIDING);
                // Only capture again if we are using internal guider
                if (guiderType == GUIDE_INTERNAL)
                    capture();
            }
            break;
        default:
            break;
    }
}

void Guide::updateCCDBin(int index)
{
    if (m_Camera == nullptr || guiderType != GUIDE_INTERNAL)
        return;

    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

    targetChip->setBinning(index + 1, index + 1);
    guideBinIndex = index;

    QVariantMap settings      = frameSettings[targetChip];
    settings["binx"]          = index + 1;
    settings["biny"]          = index + 1;
    frameSettings[targetChip] = settings;

    m_GuiderInstance->setFrameParams(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(), settings["h"].toInt(),
                                     settings["binx"].toInt(), settings["biny"].toInt());

    // saveSettings(); too early! Check first supported binning (see "processCCDNumber")
}

void Guide::processCCDNumber(INumberVectorProperty *nvp)
{
    if (m_Camera == nullptr || (nvp->device != m_Camera->getDeviceName()) || guiderType != GUIDE_INTERNAL)
        return;

    if ((!strcmp(nvp->name, "CCD_BINNING") && useGuideHead == false) ||
            (!strcmp(nvp->name, "GUIDER_BINNING") && useGuideHead))
    {
        binningCombo->disconnect();
        if (guideBinIndex > (nvp->np[0].value - 1)) // INDI driver reports not supported binning
        {
            appendLogText(i18n("%1x%1 guide binning is not supported.", guideBinIndex + 1));
            binningCombo->setCurrentIndex( nvp->np[0].value - 1 );
        }
        else
        {
            binningCombo->setCurrentIndex(guideBinIndex);
        }
        connect(binningCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Guide::updateCCDBin);
        saveSettings(); // Save binning (and more) immediately
    }
}

void Guide::checkExposureValue(ISD::CameraChip *targetChip, double exposure, IPState expState)
{
    // Ignore if not using internal guider, or chip belongs to a different camera.
    if (guiderType != GUIDE_INTERNAL || targetChip->getCCD() != m_Camera)
        return;

    INDI_UNUSED(exposure);

    if (expState == IPS_ALERT &&
            ((m_State == GUIDE_GUIDING) || (m_State == GUIDE_DITHERING) || (m_State == GUIDE_CALIBRATING)))
    {
        appendLogText(i18n("Exposure failed. Restarting exposure..."));
        m_Camera->setEncodingFormat("FITS");
        targetChip->capture(exposureIN->value());
    }
}

void Guide::configSEPMultistarOptions()
{
    // SEP MultiStar always uses an automated guide star & doesn't subframe.
    if (internalGuider->SEPMultiStarEnabled())
    {
        subFrameCheck->setChecked(false);
        subFrameCheck->setEnabled(false);
        autoStarCheck->setChecked(true);
        autoStarCheck->setEnabled(false);
    }
    else
    {
        subFrameCheck->setChecked(Options::guideSubframeEnabled());
        subFrameCheck->setEnabled(true);
        autoStarCheck->setChecked(Options::guideAutoStarEnabled());
        autoStarCheck->setEnabled(true);
    }
}

void Guide::setDarkFrameEnabled(bool enable)
{
    Options::setGuideDarkFrameEnabled(enable);
    if (darkFrameCheck->isChecked() != enable)
        darkFrameCheck->setChecked(enable);
}

void Guide::saveDefaultGuideExposure()
{
    Options::setGuideExposure(exposureIN->value());
    if(guiderType == GUIDE_PHD2)
        phd2Guider->requestSetExposureTime(exposureIN->value() * 1000);
}

void Guide::setStarPosition(const QVector3D &newCenter, bool updateNow)
{
    starCenter.setX(newCenter.x());
    starCenter.setY(newCenter.y());
    if (newCenter.z() > 0)
        starCenter.setZ(newCenter.z());

    if (updateNow)
        syncTrackingBoxPosition();
}

void Guide::syncTrackingBoxPosition()
{
    if(!m_Camera || guiderType == GUIDE_LINGUIDER)
        return;

    if(guiderType == GUIDE_PHD2)
    {
        //This way it won't set the tracking box on the Guide Star Image.
        if(!m_ImageData.isNull())
        {
            if(m_ImageData->width() < 50)
            {
                m_GuideView->setTrackingBoxEnabled(false);
                return;
            }
        }
    }

    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
    Q_ASSERT(targetChip);

    int subBinX = 1, subBinY = 1;
    targetChip->getBinning(&subBinX, &subBinY);

    if (starCenter.isNull() == false)
    {
        double boxSize = boxSizeCombo->currentText().toInt();
        int x, y, w, h;
        targetChip->getFrame(&x, &y, &w, &h);
        // If box size is larger than image size, set it to lower index
        if (boxSize / subBinX >= w || boxSize / subBinY >= h)
        {
            int newIndex = boxSizeCombo->currentIndex() - 1;
            if (newIndex >= 0)
                boxSizeCombo->setCurrentIndex(newIndex);
            return;
        }

        // If binning changed, update coords accordingly
        if (subBinX != starCenter.z())
        {
            if (starCenter.z() > 0)
            {
                starCenter.setX(starCenter.x() * (starCenter.z() / subBinX));
                starCenter.setY(starCenter.y() * (starCenter.z() / subBinY));
            }

            starCenter.setZ(subBinX);
        }

        QRect starRect = QRect(starCenter.x() - boxSize / (2 * subBinX), starCenter.y() - boxSize / (2 * subBinY),
                               boxSize / subBinX, boxSize / subBinY);
        m_GuideView->setTrackingBoxEnabled(true);
        m_GuideView->setTrackingBox(starRect);
    }
}

bool Guide::setGuiderType(int type)
{
    // Use default guider option
    if (type == -1)
        type = Options::guiderType();
    else if (type == guiderType)
        return true;

    if (m_State == GUIDE_CALIBRATING || m_State == GUIDE_GUIDING || m_State == GUIDE_DITHERING)
    {
        appendLogText(i18n("Cannot change guider type while active."));
        return false;
    }

    if (m_GuiderInstance != nullptr)
    {
        // Disconnect from host
        if (m_GuiderInstance->isConnected())
            m_GuiderInstance->Disconnect();

        // Disconnect signals
        m_GuiderInstance->disconnect();
    }

    guiderType = static_cast<GuiderType>(type);

    switch (type)
    {
        case GUIDE_INTERNAL:
        {
            connect(internalGuider, &InternalGuider::newMultiPulse, this, &Guide::sendMultiPulse);
            connect(internalGuider, &InternalGuider::newSinglePulse, this, &Guide::sendSinglePulse);
            connect(internalGuider, &InternalGuider::DESwapChanged, swapCheck, &QCheckBox::setChecked);
            connect(internalGuider, &InternalGuider::newStarPixmap, this, &Guide::newStarPixmap);

            m_GuiderInstance = internalGuider;

            internalGuider->setSquareAlgorithm(opsGuide->kcfg_GuideAlgorithm->currentIndex());

            clearCalibrationB->setEnabled(true);
            guideB->setEnabled(true);
            captureB->setEnabled(true);
            loopB->setEnabled(true);

            configSEPMultistarOptions();
            darkFrameCheck->setEnabled(true);

            cameraCombo->setEnabled(true);
            guiderCombo->setEnabled(true);
            exposureIN->setEnabled(true);
            binningCombo->setEnabled(true);
            boxSizeCombo->setEnabled(true);

            externalConnectB->setEnabled(false);
            externalDisconnectB->setEnabled(false);

            opsGuide->controlGroup->setEnabled(true);
            infoGroup->setEnabled(true);
            label_6->setEnabled(true);
            FOVScopeCombo->setEnabled(true);
            l_5->setEnabled(true);
            l_6->setEnabled(true);
            l_7->setEnabled(true);
            l_8->setEnabled(true);
            l_Aperture->setEnabled(true);
            l_FOV->setEnabled(true);
            l_FbyD->setEnabled(true);
            l_Focal->setEnabled(true);
            driftGraphicsGroup->setEnabled(true);

            cameraCombo->setToolTip(i18n("Select guide camera."));

            updateGuideParams();
        }
        break;

        case GUIDE_PHD2:
            if (phd2Guider.isNull())
                phd2Guider = new PHD2();

            m_GuiderInstance = phd2Guider;
            phd2Guider->setGuideView(m_GuideView);

            connect(phd2Guider, SIGNAL(newStarPixmap(QPixmap &)), this, SIGNAL(newStarPixmap(QPixmap &)));

            clearCalibrationB->setEnabled(true);
            captureB->setEnabled(false);
            loopB->setEnabled(false);
            darkFrameCheck->setEnabled(false);
            subFrameCheck->setEnabled(false);
            autoStarCheck->setEnabled(false);
            guideB->setEnabled(false); //This will be enabled later when equipment connects (or not)
            externalConnectB->setEnabled(false);

            checkBox_DirRA->setEnabled(false);
            eastControlCheck->setEnabled(false);
            westControlCheck->setEnabled(false);
            swapCheck->setEnabled(false);


            opsGuide->controlGroup->setEnabled(false);
            infoGroup->setEnabled(true);
            label_6->setEnabled(false);
            FOVScopeCombo->setEnabled(false);
            l_5->setEnabled(false);
            l_6->setEnabled(false);
            l_7->setEnabled(false);
            l_8->setEnabled(false);
            l_Aperture->setEnabled(false);
            l_FOV->setEnabled(false);
            l_FbyD->setEnabled(false);
            l_Focal->setEnabled(false);
            driftGraphicsGroup->setEnabled(true);

            guiderCombo->setEnabled(false);
            exposureIN->setEnabled(true);
            binningCombo->setEnabled(false);
            boxSizeCombo->setEnabled(false);
            cameraCombo->setEnabled(false);

            if (Options::resetGuideCalibration())
                appendLogText(i18n("Warning: Reset Guiding Calibration is enabled. It is recommended to turn this option off for PHD2."));

            updateGuideParams();
            break;

        case GUIDE_LINGUIDER:
            if (linGuider.isNull())
                linGuider = new LinGuider();

            m_GuiderInstance = linGuider;

            clearCalibrationB->setEnabled(true);
            captureB->setEnabled(false);
            loopB->setEnabled(false);
            darkFrameCheck->setEnabled(false);
            subFrameCheck->setEnabled(false);
            autoStarCheck->setEnabled(false);
            guideB->setEnabled(true);
            externalConnectB->setEnabled(true);

            opsGuide->controlGroup->setEnabled(false);
            infoGroup->setEnabled(false);
            driftGraphicsGroup->setEnabled(false);

            guiderCombo->setEnabled(false);
            exposureIN->setEnabled(false);
            binningCombo->setEnabled(false);
            boxSizeCombo->setEnabled(false);

            cameraCombo->setEnabled(false);

            updateGuideParams();

            break;
    }

    if (m_GuiderInstance != nullptr)
    {
        connect(m_GuiderInstance, &Ekos::GuideInterface::frameCaptureRequested, this, &Ekos::Guide::capture);
        connect(m_GuiderInstance, &Ekos::GuideInterface::newLog, this, &Ekos::Guide::appendLogText);
        connect(m_GuiderInstance, &Ekos::GuideInterface::newStatus, this, &Ekos::Guide::setStatus);
        connect(m_GuiderInstance, &Ekos::GuideInterface::newStarPosition, this, &Ekos::Guide::setStarPosition);
        connect(m_GuiderInstance, &Ekos::GuideInterface::guideStats, this, &Ekos::Guide::guideStats);

        connect(m_GuiderInstance, &Ekos::GuideInterface::newAxisDelta, this, &Ekos::Guide::setAxisDelta);
        connect(m_GuiderInstance, &Ekos::GuideInterface::newAxisPulse, this, &Ekos::Guide::setAxisPulse);
        connect(m_GuiderInstance, &Ekos::GuideInterface::newAxisSigma, this, &Ekos::Guide::setAxisSigma);
        connect(m_GuiderInstance, &Ekos::GuideInterface::newSNR, this, &Ekos::Guide::setSNR);

        driftGraph->connectGuider(m_GuiderInstance);
        targetPlot->connectGuider(m_GuiderInstance);

        connect(m_GuiderInstance, &Ekos::GuideInterface::calibrationUpdate, this, &Ekos::Guide::calibrationUpdate);

        connect(m_GuiderInstance, &Ekos::GuideInterface::guideEquipmentUpdated, this, &Ekos::Guide::configurePHD2Camera);
    }

    externalConnectB->setEnabled(false);
    externalDisconnectB->setEnabled(false);

    if (m_GuiderInstance != nullptr && guiderType != GUIDE_INTERNAL)
    {
        externalConnectB->setEnabled(!m_GuiderInstance->isConnected());
        externalDisconnectB->setEnabled(m_GuiderInstance->isConnected());
    }

    if (m_GuiderInstance != nullptr)
        m_GuiderInstance->Connect();

    return true;
}

void Guide::updateTrackingBoxSize(int currentIndex)
{
    if (currentIndex >= 0)
    {
        Options::setGuideSquareSizeIndex(currentIndex);

        if (guiderType == GUIDE_INTERNAL)
            dynamic_cast<InternalGuider *>(m_GuiderInstance)->setGuideBoxSize(boxSizeCombo->currentText().toInt());

        syncTrackingBoxPosition();
    }
}

void Guide::onThresholdChanged(int index)
{
    switch (guiderType)
    {
        case GUIDE_INTERNAL:
            dynamic_cast<InternalGuider *>(m_GuiderInstance)->setSquareAlgorithm(index);
            break;

        default:
            break;
    }
}

void Guide::onEnableDirRA(bool enable)
{
    // If RA guiding is enable or disabled, the GPG should be reset.
    if (Options::gPGEnabled())
        m_GuiderInstance->resetGPG();
    Options::setRAGuideEnabled(enable);
}

void Guide::onEnableDirDEC(bool enable)
{
    Options::setDECGuideEnabled(enable);
    updatePHD2Directions();
}

void Guide::syncSettings()
{
    QCheckBox *pCB = nullptr;

    QObject *obj = sender();

    if ((pCB = qobject_cast<QCheckBox*>(obj)))
    {
        if (pCB == autoStarCheck)
            Options::setGuideAutoStarEnabled(pCB->isChecked());
    }

    emit settingsUpdated(getSettings());
}

void Guide::onControlDirectionChanged(bool enable)
{
    QObject *obj = sender();

    if (northControlCheck == dynamic_cast<QCheckBox *>(obj))
    {
        Options::setNorthDECGuideEnabled(enable);
        updatePHD2Directions();
    }
    else if (southControlCheck == dynamic_cast<QCheckBox *>(obj))
    {
        Options::setSouthDECGuideEnabled(enable);
        updatePHD2Directions();
    }
    else if (westControlCheck == dynamic_cast<QCheckBox *>(obj))
    {
        Options::setWestRAGuideEnabled(enable);
    }
    else if (eastControlCheck == dynamic_cast<QCheckBox *>(obj))
    {
        Options::setEastRAGuideEnabled(enable);
    }
}
void Guide::updatePHD2Directions()
{
    if(guiderType == GUIDE_PHD2)
        phd2Guider -> requestSetDEGuideMode(checkBox_DirDEC->isChecked(), northControlCheck->isChecked(),
                                            southControlCheck->isChecked());
}
void Guide::updateDirectionsFromPHD2(const QString &mode)
{
    //disable connections
    disconnect(checkBox_DirDEC, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirDEC);
    disconnect(northControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    disconnect(southControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);

    if(mode == "Auto")
    {
        checkBox_DirDEC->setChecked(true);
        northControlCheck->setChecked(true);
        southControlCheck->setChecked(true);
    }
    else if(mode == "North")
    {
        checkBox_DirDEC->setChecked(true);
        northControlCheck->setChecked(true);
        southControlCheck->setChecked(false);
    }
    else if(mode == "South")
    {
        checkBox_DirDEC->setChecked(true);
        northControlCheck->setChecked(false);
        southControlCheck->setChecked(true);
    }
    else //Off
    {
        checkBox_DirDEC->setChecked(false);
        northControlCheck->setChecked(true);
        southControlCheck->setChecked(true);
    }

    //Re-enable connections
    connect(checkBox_DirDEC, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirDEC);
    connect(northControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(southControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
}


void Guide::loadSettings()
{
    // Settings in main dialog
    // Exposure
    exposureIN->setValue(Options::guideExposure());
    // Delay
    GuideDelay->setValue(Options::guideDelay());
    // Bin Size
    guideBinIndex = Options::guideBinSizeIndex();
    // Box Size
    boxSizeCombo->setCurrentIndex(Options::guideSquareSizeIndex());
    // Dark frame?
    darkFrameCheck->setChecked(Options::guideDarkFrameEnabled());
    // Subframed?
    subFrameCheck->setChecked(Options::guideSubframeEnabled());
    // RA/DEC enabled?
    checkBox_DirRA->setChecked(Options::rAGuideEnabled());
    checkBox_DirDEC->setChecked(Options::dECGuideEnabled());
    // N/S enabled?
    northControlCheck->setChecked(Options::northDECGuideEnabled());
    southControlCheck->setChecked(Options::southDECGuideEnabled());
    // W/E enabled?
    westControlCheck->setChecked(Options::westRAGuideEnabled());
    eastControlCheck->setChecked(Options::eastRAGuideEnabled());
    // Autostar
    autoStarCheck->setChecked(Options::guideAutoStarEnabled());

    /* Settings in sub dialog are controlled by KConfigDialog ("kcfg"-variables)
     * PID Control - Proportional Gain
     * PID Control - Integral Gain
     * Max Pulse Duration (arcsec)
     * Min Pulse Duration (arcsec)
     */
    // Transition code: if old values are stored in the proportional gains,
    // change them to a default value.
    if (Options::rAProportionalGain() > 1.0)
        Options::setRAProportionalGain(0.75);
    if (Options::dECProportionalGain() > 1.0)
        Options::setDECProportionalGain(0.75);
    if (Options::rAIntegralGain() > 1.0)
        Options::setRAIntegralGain(0.75);
    if (Options::dECIntegralGain() > 1.0)
        Options::setDECIntegralGain(0.75);


}

void Guide::saveSettings()
{
    // Settings in main dialog
    // Exposure
    Options::setGuideExposure(exposureIN->value());
    // Delay
    Options::setGuideDelay(GuideDelay->value());
    // Bin Size
    Options::setGuideBinSizeIndex(binningCombo->currentIndex());
    // Box Size
    Options::setGuideSquareSizeIndex(boxSizeCombo->currentIndex());
    // Dark frame?
    Options::setGuideDarkFrameEnabled(darkFrameCheck->isChecked());
    // Subframed?
    Options::setGuideSubframeEnabled(subFrameCheck->isChecked());
    // RA/DEC enabled?
    Options::setRAGuideEnabled(checkBox_DirRA->isChecked());
    Options::setDECGuideEnabled(checkBox_DirDEC->isChecked());
    // N/S enabled?
    Options::setNorthDECGuideEnabled(northControlCheck->isChecked());
    Options::setSouthDECGuideEnabled(southControlCheck->isChecked());
    // W/E enabled?
    Options::setWestRAGuideEnabled(westControlCheck->isChecked());
    Options::setEastRAGuideEnabled(eastControlCheck->isChecked());
    /* Settings in sub dialog are controlled by KConfigDialog ("kcfg"-variables)
     * PID Control - Proportional Gain
     * PID Control - Integral Gain
     * Max Pulse Duration (arcsec)
     * Min Pulse Duration (arcsec)
     */
}

void Guide::setTrackingStar(int x, int y)
{
    QVector3D newStarPosition(x, y, -1);
    setStarPosition(newStarPosition, true);

    if(guiderType == GUIDE_PHD2)
    {
        //The Guide Star Image is 32 pixels across or less, so this guarantees it isn't that.
        if(!m_ImageData.isNull())
        {
            if(m_ImageData->width() > 50)
                phd2Guider->setLockPosition(starCenter.x(), starCenter.y());
        }
    }

    if (operationStack.isEmpty() == false)
        executeOperationStack();
}

void Guide::setAxisDelta(double ra, double de)
{
    //If PHD2 starts guiding because somebody pusted the button remotely, we want to set the state to guiding.
    //If guide pulses start coming in, it must be guiding.
    // 2020-04-10 sterne-jaeger: Will be resolved inside EKOS phd guiding.
    // if(guiderType == GUIDE_PHD2 && state != GUIDE_GUIDING)
    //     setStatus(GUIDE_GUIDING);

    ra = -ra;  //The ra is backwards in sign from how it should be displayed on the graph.

    int currentNumPoints = driftGraph->graph(GuideGraph::G_RA)->dataCount();
    guideSlider->setMaximum(currentNumPoints);
    if(graphOnLatestPt)
    {
        guideSlider->setValue(currentNumPoints);
    }
    l_DeltaRA->setText(QString::number(ra, 'f', 2));
    l_DeltaDEC->setText(QString::number(de, 'f', 2));

    emit newAxisDelta(ra, de);
}

void Guide::calibrationUpdate(GuideInterface::CalibrationUpdateType type, const QString &message,
                              double dx, double dy)
{
    switch (type)
    {
        case GuideInterface::RA_OUT:
            calibrationPlot->graph(GuideGraph::G_RA)->addData(dx, dy);
            break;
        case GuideInterface::RA_IN:
            calibrationPlot->graph(GuideGraph::G_DEC)->addData(dx, dy);
            break;
        case GuideInterface::BACKLASH:
            calibrationPlot->graph(GuideGraph::G_RA_HIGHLIGHT)->addData(dx, dy);
            break;
        case GuideInterface::DEC_OUT:
            calibrationPlot->graph(GuideGraph::G_DEC_HIGHLIGHT)->addData(dx, dy);
            break;
        case GuideInterface::DEC_IN:
            calibrationPlot->graph(GuideGraph::G_RA_PULSE)->addData(dx, dy);
            break;
        case GuideInterface::CALIBRATION_MESSAGE_ONLY:
            ;
    }
    calLabel->setText(message);
    calibrationPlot->replot();
}

void Guide::setAxisSigma(double ra, double de)
{
    l_ErrRA->setText(QString::number(ra, 'f', 2));
    l_ErrDEC->setText(QString::number(de, 'f', 2));
    const double total = std::hypot(ra, de);
    l_TotalRMS->setText(QString::number(total, 'f', 2));

    emit newAxisSigma(ra, de);
}

QList<double> Guide::axisDelta()
{
    QList<double> delta;

    delta << l_DeltaRA->text().toDouble() << l_DeltaDEC->text().toDouble();

    return delta;
}

QList<double> Guide::axisSigma()
{
    QList<double> sigma;

    sigma << l_ErrRA->text().toDouble() << l_ErrDEC->text().toDouble();

    return sigma;
}

void Guide::setAxisPulse(double ra, double de)
{
    l_PulseRA->setText(QString::number(static_cast<int>(ra)));
    l_PulseDEC->setText(QString::number(static_cast<int>(de)));
}

void Guide::setSNR(double snr)
{
    l_SNR->setText(QString::number(snr, 'f', 1));
}

void Guide::buildOperationStack(GuideState operation)
{
    operationStack.clear();

    switch (operation)
    {
        case GUIDE_CAPTURE:
            if (Options::guideDarkFrameEnabled())
                operationStack.push(GUIDE_DARK);

            operationStack.push(GUIDE_CAPTURE);
            operationStack.push(GUIDE_SUBFRAME);
            break;

        case GUIDE_CALIBRATING:
            operationStack.push(GUIDE_CALIBRATING);
            if (guiderType == GUIDE_INTERNAL)
            {
                if (Options::guideDarkFrameEnabled())
                    operationStack.push(GUIDE_DARK);

                // Auto Star Selected Path
                if (Options::guideAutoStarEnabled() ||
                        // SEP MultiStar always uses an automated guide star.
                        internalGuider->SEPMultiStarEnabled())
                {
                    // If subframe is enabled and we need to auto select a star, then we need to make the final capture
                    // of the subframed image. This is only done if we aren't already subframed.
                    if (subFramed == false && Options::guideSubframeEnabled())
                        operationStack.push(GUIDE_CAPTURE);

                    operationStack.push(GUIDE_SUBFRAME);
                    operationStack.push(GUIDE_STAR_SELECT);


                    operationStack.push(GUIDE_CAPTURE);

                    // If we are being ask to go full frame, let's do that first
                    if (subFramed == true && Options::guideSubframeEnabled() == false)
                        operationStack.push(GUIDE_SUBFRAME);
                }
                // Manual Star Selection Path
                else
                {
                    // Final capture before we start calibrating
                    if (subFramed == false && Options::guideSubframeEnabled())
                        operationStack.push(GUIDE_CAPTURE);

                    // Subframe if required
                    operationStack.push(GUIDE_SUBFRAME);

                    // First capture an image
                    operationStack.push(GUIDE_CAPTURE);
                }

            }
            break;

        default:
            break;
    }
}

bool Guide::executeOperationStack()
{
    if (operationStack.isEmpty())
        return false;

    GuideState nextOperation = operationStack.pop();

    bool actionRequired = false;

    switch (nextOperation)
    {
        case GUIDE_SUBFRAME:
            actionRequired = executeOneOperation(nextOperation);
            break;

        case GUIDE_DARK:
            actionRequired = executeOneOperation(nextOperation);
            break;

        case GUIDE_CAPTURE:
            actionRequired = captureOneFrame();
            break;

        case GUIDE_STAR_SELECT:
            actionRequired = executeOneOperation(nextOperation);
            break;

        case GUIDE_CALIBRATING:
            if (guiderType == GUIDE_INTERNAL)
            {
                m_GuiderInstance->setStarPosition(starCenter);

                // Tracking must be engaged
                if (m_Mount && m_Mount->canControlTrack() && m_Mount->isTracking() == false)
                    m_Mount->setTrackEnabled(true);
            }

            if (m_GuiderInstance->calibrate())
            {
                if (guiderType == GUIDE_INTERNAL)
                    disconnect(m_GuideView.get(), &FITSView::trackingStarSelected, this, &Guide::setTrackingStar);
                setBusy(true);
            }
            else
            {
                emit newStatus(GUIDE_CALIBRATION_ERROR);
                m_State = GUIDE_IDLE;
                appendLogText(i18n("Calibration failed to start."));
                setBusy(false);
            }
            break;

        default:
            break;
    }

    // If an additional action is required, return return and continue later
    if (actionRequired)
        return true;
    // Otherwise, continue processing the stack
    else
        return executeOperationStack();
}

bool Guide::executeOneOperation(GuideState operation)
{
    bool actionRequired = false;

    if (m_Camera == nullptr)
        return actionRequired;

    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
    if (targetChip == nullptr)
        return false;

    int subBinX, subBinY;
    targetChip->getBinning(&subBinX, &subBinY);

    switch (operation)
    {
        case GUIDE_SUBFRAME:
        {
            // SEP MultiStar doesn't subframe.
            if ((guiderType == GUIDE_INTERNAL) && internalGuider->SEPMultiStarEnabled())
                break;
            // Check if we need and can subframe
            if (subFramed == false && Options::guideSubframeEnabled() == true && targetChip->canSubframe())
            {
                int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
                targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

                int offset = boxSizeCombo->currentText().toInt() / subBinX;

                int x = starCenter.x();
                int y = starCenter.y();

                x     = (x - offset * 2) * subBinX;
                y     = (y - offset * 2) * subBinY;
                int w = offset * 4 * subBinX;
                int h = offset * 4 * subBinY;

                if (x < minX)
                    x = minX;
                if (y < minY)
                    y = minY;
                if ((x + w) > maxW)
                    w = maxW - x;
                if ((y + h) > maxH)
                    h = maxH - y;

                targetChip->setFrame(x, y, w, h);

                subFramed            = true;
                QVariantMap settings = frameSettings[targetChip];
                settings["x"]        = x;
                settings["y"]        = y;
                settings["w"]        = w;
                settings["h"]        = h;
                settings["binx"]     = subBinX;
                settings["biny"]     = subBinY;

                frameSettings[targetChip] = settings;

                starCenter.setX(w / (2 * subBinX));
                starCenter.setY(h / (2 * subBinX));
            }
            // Otherwise check if we are already subframed
            // and we need to go back to full frame
            // or if we need to go back to full frame since we need
            // to reaquire a star
            else if (subFramed &&
                     (Options::guideSubframeEnabled() == false ||
                      m_State == GUIDE_REACQUIRE))
            {
                targetChip->resetFrame();

                int x, y, w, h;
                targetChip->getFrame(&x, &y, &w, &h);

                QVariantMap settings;
                settings["x"]             = x;
                settings["y"]             = y;
                settings["w"]             = w;
                settings["h"]             = h;
                settings["binx"]          = subBinX;
                settings["biny"]          = subBinY;
                frameSettings[targetChip] = settings;

                subFramed = false;

                starCenter.setX(w / (2 * subBinX));
                starCenter.setY(h / (2 * subBinX));

                //starCenter.setX(0);
                //starCenter.setY(0);
            }
        }
        break;

        case GUIDE_DARK:
        {
            // Do we need to take a dark frame?
            if (m_ImageData && Options::guideDarkFrameEnabled())
            {
                QVariantMap settings = frameSettings[targetChip];
                uint16_t offsetX = 0;
                uint16_t offsetY = 0;

                if (settings["x"].isValid() &&
                        settings["y"].isValid() &&
                        settings["binx"].isValid() &&
                        settings["biny"].isValid())
                {
                    offsetX = settings["x"].toInt() / settings["binx"].toInt();
                    offsetY = settings["y"].toInt() / settings["biny"].toInt();
                }

                actionRequired = true;
                targetChip->setCaptureFilter(FITS_NONE);
                m_DarkProcessor->denoise(targetChip, m_ImageData, exposureIN->value(), offsetX, offsetY);
            }
        }
        break;

        case GUIDE_STAR_SELECT:
        {
            m_State = GUIDE_STAR_SELECT;
            emit newStatus(m_State);

            if (Options::guideAutoStarEnabled() ||
                    // SEP MultiStar always uses an automated guide star.
                    ((guiderType == GUIDE_INTERNAL) &&
                     internalGuider->SEPMultiStarEnabled()))
            {
                bool autoStarCaptured = internalGuider->selectAutoStar();
                if (autoStarCaptured)
                {
                    appendLogText(i18n("Auto star selected."));
                }
                else
                {
                    appendLogText(i18n("Failed to select an auto star."));
                    actionRequired = true;
                    m_State = GUIDE_CALIBRATION_ERROR;
                    emit newStatus(m_State);
                    setBusy(false);
                }
            }
            else
            {
                appendLogText(i18n("Select a guide star to calibrate."));
                actionRequired = true;
            }
        }
        break;

        default:
            break;
    }

    return actionRequired;
}

void Guide::processGuideOptions()
{
    if (Options::guiderType() != guiderType)
    {
        guiderType = static_cast<GuiderType>(Options::guiderType());
        setGuiderType(Options::guiderType());
    }
}

void Guide::showFITSViewer()
{
    static int lastFVTabID = -1;
    if (m_ImageData)
    {
        QUrl url = QUrl::fromLocalFile("guide.fits");
        if (fv.isNull())
        {
            fv = KStars::Instance()->createFITSViewer();
            fv->loadData(m_ImageData, url, &lastFVTabID);
        }
        else if (fv->updateData(m_ImageData, url, lastFVTabID, &lastFVTabID) == false)
            fv->loadData(m_ImageData, url, &lastFVTabID);

        fv->show();
    }
}

void Guide::setExternalGuiderBLOBEnabled(bool enable)
{
    // Nothing to do if guider is internal
    if (guiderType == GUIDE_INTERNAL)
        return;

    if(!m_Camera)
        return;

    m_Camera->setBLOBEnabled(enable);

    if(m_Camera->isBLOBEnabled())
    {
        if (m_Camera->hasGuideHead() && cameraCombo->currentText().contains("Guider"))
            useGuideHead = true;
        else
            useGuideHead = false;


        auto targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        if (targetChip)
            targetChip->setCaptureMode(FITS_GUIDE);
        syncCameraInfo();
    }

}

void Guide::resetNonGuidedDither()
{
    // reset non guided dither total drift
    nonGuidedDitherRaOffsetMsec = 0;
    nonGuidedDitherDecOffsetMsec = 0;
    qCDebug(KSTARS_EKOS_GUIDE) << "Reset non guiding dithering position";

    // initialize random generator if not done before
    if (!isNonGuidedDitherInitialized)
    {
        auto seed = std::chrono::system_clock::now().time_since_epoch().count();
        nonGuidedPulseGenerator.seed(seed);
        isNonGuidedDitherInitialized = true;
        qCDebug(KSTARS_EKOS_GUIDE) << "Initialize non guiding dithering random generator";
    }
}

void Guide::nonGuidedDither()
{
    double ditherPulse = Options::ditherNoGuidingPulse();

    // Randomize dithering position up to +/-dithePulse distance from original
    std::uniform_int_distribution<int> newPos(-ditherPulse, +ditherPulse);

    // Calculate the pulse needed to move to the new position, then save the new position and apply the pulse

    // for ra
    const int newRaOffsetMsec = newPos(nonGuidedPulseGenerator);
    const int raPulse = nonGuidedDitherRaOffsetMsec - newRaOffsetMsec;
    nonGuidedDitherRaOffsetMsec = newRaOffsetMsec;
    const int raMsec  = std::abs(raPulse);
    const int raPolarity = (raPulse >= 0 ? 1 : -1);

    // and for dec
    const int newDecOffsetMsec = newPos(nonGuidedPulseGenerator);
    const int decPulse = nonGuidedDitherDecOffsetMsec - newDecOffsetMsec;
    nonGuidedDitherDecOffsetMsec = newDecOffsetMsec;
    const int decMsec  = std::abs(decPulse);
    const int decPolarity = (decPulse >= 0 ? 1 : -1);

    qCInfo(KSTARS_EKOS_GUIDE) << "Starting non-guiding dither...";
    qCDebug(KSTARS_EKOS_GUIDE) << "dither ra_msec:" << raMsec << "ra_polarity:" << raPolarity << "de_msec:" << decMsec <<
                               "de_polarity:" << decPolarity;

    bool rc = sendMultiPulse(raPolarity > 0 ? RA_INC_DIR : RA_DEC_DIR, raMsec, decPolarity > 0 ? DEC_INC_DIR : DEC_DEC_DIR,
                             decMsec);

    if (rc)
    {
        qCInfo(KSTARS_EKOS_GUIDE) << "Non-guiding dither successful.";
        QTimer::singleShot( (raMsec > decMsec ? raMsec : decMsec) + Options::ditherSettle() * 1000 + 100, [this]()
        {
            emit newStatus(GUIDE_DITHERING_SUCCESS);
            m_State = GUIDE_IDLE;
        });
    }
    else
    {
        qCWarning(KSTARS_EKOS_GUIDE) << "Non-guiding dither failed.";
        emit newStatus(GUIDE_DITHERING_ERROR);
        m_State = GUIDE_IDLE;
    }
}

void Guide::updateTelescopeType(int index)
{
    if (m_Camera == nullptr)
        return;

    focal_length = (index == ISD::Camera::TELESCOPE_PRIMARY) ? primaryFL : guideFL;
    aperture = (index == ISD::Camera::TELESCOPE_PRIMARY) ? primaryAperture : guideAperture;

    Options::setGuideScopeType(index);

    syncTelescopeInfo();
}

void Guide::setDefaultGuider(const QString &driver)
{
    Options::setDefaultGuideGuider(driver);
}

void Guide::setDefaultCCD(const QString &ccd)
{
    if (guiderType == GUIDE_INTERNAL)
        Options::setDefaultGuideCCD(ccd);
}

void Guide::handleManualDither()
{
    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
    if (targetChip == nullptr)
        return;

    Ui::ManualDither ditherDialog;
    QDialog container(this);
    ditherDialog.setupUi(&container);

    if (guiderType != GUIDE_INTERNAL)
    {
        ditherDialog.coordinatesR->setEnabled(false);
        ditherDialog.x->setEnabled(false);
        ditherDialog.y->setEnabled(false);
    }

    int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
    targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

    ditherDialog.x->setMinimum(minX);
    ditherDialog.x->setMaximum(maxX);
    ditherDialog.y->setMinimum(minY);
    ditherDialog.y->setMaximum(maxY);

    ditherDialog.x->setValue(starCenter.x());
    ditherDialog.y->setValue(starCenter.y());

    if (container.exec() == QDialog::Accepted)
    {
        if (ditherDialog.magnitudeR->isChecked())
            m_GuiderInstance->dither(ditherDialog.magnitude->value());
        else
        {
            InternalGuider * const ig = dynamic_cast<InternalGuider *>(m_GuiderInstance);
            if (ig)
                ig->ditherXY(ditherDialog.x->value(), ditherDialog.y->value());
        }
    }
}

bool Guide::connectGuider()
{
    setStatus(GUIDE_IDLE);
    return m_GuiderInstance->Connect();
}

bool Guide::disconnectGuider()
{
    return m_GuiderInstance->Disconnect();
}

void Guide::initPlots()
{
    initDriftGraph();
    initCalibrationPlot();

    connect(rightLayout, &QSplitter::splitterMoved, this, &Ekos::Guide::handleVerticalPlotSizeChange);
    connect(driftSplitter, &QSplitter::splitterMoved, this, &Ekos::Guide::handleHorizontalPlotSizeChange);

    //This sets the values of all the Graph Options that are stored.
    accuracyRadiusSpin->setValue(Options::guiderAccuracyThreshold());
    showRAPlotCheck->setChecked(Options::rADisplayedOnGuideGraph());
    showDECPlotCheck->setChecked(Options::dEDisplayedOnGuideGraph());
    showRACorrectionsCheck->setChecked(Options::rACorrDisplayedOnGuideGraph());
    showDECorrectionsCheck->setChecked(Options::dECorrDisplayedOnGuideGraph());
    showSNRPlotCheck->setChecked(Options::sNRDisplayedOnGuideGraph());
    showRMSPlotCheck->setChecked(Options::rMSDisplayedOnGuideGraph());

    buildTarget();
}

void Guide::initDriftGraph()
{
    //Dragging and zooming settings
    // make bottom axis transfer its range to the top axis if the graph gets zoomed:
    connect(driftGraph->xAxis,  static_cast<void(QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            driftGraph->xAxis2, static_cast<void(QCPAxis::*)(const QCPRange &)>(&QCPAxis::setRange));
    // update the second vertical axis properly if the graph gets zoomed.
    connect(driftGraph->yAxis, static_cast<void(QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            [this]()
    {
        driftGraph->setCorrectionGraphScale(correctionSlider->value());
    });

    connect(driftGraph, &QCustomPlot::mouseMove, driftGraph, &GuideDriftGraph::mouseOverLine);
    connect(driftGraph, &QCustomPlot::mousePress, driftGraph, &GuideDriftGraph::mouseClicked);

    int scale =
        50;  //This is a scaling value between the left and the right axes of the driftGraph, it could be stored in kstars kcfg
    correctionSlider->setValue(scale);
}

void Guide::initCalibrationPlot()
{
    calibrationPlot->setBackground(QBrush(Qt::black));
    calibrationPlot->setSelectionTolerance(10);

    calibrationPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    calibrationPlot->yAxis->setBasePen(QPen(Qt::white, 1));

    calibrationPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    calibrationPlot->yAxis->setTickPen(QPen(Qt::white, 1));

    calibrationPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    calibrationPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));

    calibrationPlot->xAxis->setTickLabelColor(Qt::white);
    calibrationPlot->yAxis->setTickLabelColor(Qt::white);

    calibrationPlot->xAxis->setLabelColor(Qt::white);
    calibrationPlot->yAxis->setLabelColor(Qt::white);

    calibrationPlot->xAxis->setLabelFont(QFont(font().family(), 10));
    calibrationPlot->yAxis->setLabelFont(QFont(font().family(), 10));
    calibrationPlot->xAxis->setTickLabelFont(QFont(font().family(), 9));
    calibrationPlot->yAxis->setTickLabelFont(QFont(font().family(), 9));

    calibrationPlot->xAxis->setLabelPadding(2);
    calibrationPlot->yAxis->setLabelPadding(2);

    calibrationPlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    calibrationPlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    calibrationPlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    calibrationPlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    calibrationPlot->xAxis->grid()->setZeroLinePen(QPen(Qt::gray));
    calibrationPlot->yAxis->grid()->setZeroLinePen(QPen(Qt::gray));

    calibrationPlot->xAxis->setLabel(i18n("x (pixels)"));
    calibrationPlot->yAxis->setLabel(i18n("y (pixels)"));

    calibrationPlot->xAxis->setRange(-20, 20);
    calibrationPlot->yAxis->setRange(-20, 20);

    calibrationPlot->setInteractions(QCP::iRangeZoom);
    calibrationPlot->setInteraction(QCP::iRangeDrag, true);

    calibrationPlot->addGraph();
    calibrationPlot->graph(GuideGraph::G_RA)->setLineStyle(QCPGraph::lsNone);
    calibrationPlot->graph(GuideGraph::G_RA)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,
            QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"), 2), QBrush(), 6));
    calibrationPlot->graph(GuideGraph::G_RA)->setName("RA out");

    calibrationPlot->addGraph();
    calibrationPlot->graph(GuideGraph::G_DEC)->setLineStyle(QCPGraph::lsNone);
    calibrationPlot->graph(GuideGraph::G_DEC)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(Qt::white, 2),
            QBrush(), 4));
    calibrationPlot->graph(GuideGraph::G_DEC)->setName("RA in");

    calibrationPlot->addGraph();
    calibrationPlot->graph(GuideGraph::G_RA_HIGHLIGHT)->setLineStyle(QCPGraph::lsNone);
    calibrationPlot->graph(GuideGraph::G_RA_HIGHLIGHT)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus, QPen(Qt::white,
            2),
            QBrush(), 6));
    calibrationPlot->graph(GuideGraph::G_RA_HIGHLIGHT)->setName("Backlash");

    calibrationPlot->addGraph();
    calibrationPlot->graph(GuideGraph::G_DEC_HIGHLIGHT)->setLineStyle(QCPGraph::lsNone);
    calibrationPlot->graph(GuideGraph::G_DEC_HIGHLIGHT)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,
            QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"), 2), QBrush(), 6));
    calibrationPlot->graph(GuideGraph::G_DEC_HIGHLIGHT)->setName("DEC out");

    calibrationPlot->addGraph();
    calibrationPlot->graph(GuideGraph::G_RA_PULSE)->setLineStyle(QCPGraph::lsNone);
    calibrationPlot->graph(GuideGraph::G_RA_PULSE)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(Qt::yellow,
            2),
            QBrush(), 4));
    calibrationPlot->graph(GuideGraph::G_RA_PULSE)->setName("DEC in");

    calLabel = new QCPItemText(calibrationPlot);
    calLabel->setColor(QColor(255, 255, 255));
    calLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    calLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
    calLabel->position->setCoords(0.5, 0);
    calLabel->setText("");
    calLabel->setFont(QFont(font().family(), 10));
    calLabel->setVisible(true);

    calibrationPlot->resize(190, 190);
    calibrationPlot->replot();
}

void Guide::initView()
{
    guideStateWidget = new GuideStateWidget();
    guideInfoLayout->insertWidget(0, guideStateWidget);

    m_GuideView.reset(new GuideView(guideWidget, FITS_GUIDE));
    m_GuideView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_GuideView->setBaseSize(guideWidget->size());
    m_GuideView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(m_GuideView.get());
    guideWidget->setLayout(vlayout);
    connect(m_GuideView.get(), &FITSView::trackingStarSelected, this, &Ekos::Guide::setTrackingStar);
}

void Guide::initConnections()
{
    // Exposure Timeout
    captureTimeout.setSingleShot(true);
    connect(&captureTimeout, &QTimer::timeout, this, &Ekos::Guide::processCaptureTimeout);

    // Guiding Box Size
    connect(boxSizeCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Guide::updateTrackingBoxSize);

    // Guider CCD Selection
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    connect(cameraCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Guide::setDefaultCCD);
#else
    connect(cameraCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::textActivated), this,
            &Ekos::Guide::setDefaultCCD);
#endif
    connect(cameraCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this,
            [&](int index)
    {
        if (guiderType == GUIDE_INTERNAL)
        {
            starCenter = QVector3D();
            checkCamera(index);
        }
    }
           );

    FOVScopeCombo->setCurrentIndex(Options::guideScopeType());
    connect(FOVScopeCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Guide::updateTelescopeType);

    // Dark Frame Check
    connect(darkFrameCheck, &QCheckBox::toggled, this, &Ekos::Guide::setDarkFrameEnabled);
    // Subframe check
    if(guiderType != GUIDE_PHD2) //For PHD2, this is handled in the configurePHD2Camera method
        connect(subFrameCheck, &QCheckBox::toggled, this, &Ekos::Guide::setSubFrameEnabled);
    // ST4 Selection
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    connect(guiderCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated), [&](const QString & text)
#else
    connect(guiderCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::textActivated), [&](const QString & text)
#endif
    {
        setDefaultGuider(text);
        setGuider(text);
    });

    // Binning Combo Selection
    connect(binningCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Guide::updateCCDBin);

    // RA/DEC Enable directions
    connect(checkBox_DirRA, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirRA);
    connect(checkBox_DirDEC, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirDEC);

    // N/W and W/E direction enable
    connect(northControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(southControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(westControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(eastControlCheck, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);

    // Auto star check
    connect(autoStarCheck, &QCheckBox::toggled, this, &Ekos::Guide::syncSettings);

    // Declination Swap
    connect(swapCheck, &QCheckBox::toggled, this, &Ekos::Guide::setDECSwap);

    // Capture
    connect(captureB, &QPushButton::clicked, this, [this]()
    {
        m_State = GUIDE_CAPTURE;
        emit newStatus(m_State);

        if(guiderType == GUIDE_PHD2)
        {
            configurePHD2Camera();
            if(phd2Guider->isCurrentCameraNotInEkos())
                appendLogText(
                    i18n("The PHD2 camera is not available to Ekos, so you cannot see the captured images.  But you will still see the Guide Star Image when you guide."));
            else if(Options::guideSubframeEnabled())
            {
                appendLogText(
                    i18n("To receive PHD2 images other than the Guide Star Image, SubFrame must be unchecked.  Unchecking it now to enable your image captures.  You can re-enable it before Guiding"));
                subFrameCheck->setChecked(false);
            }
            phd2Guider->captureSingleFrame();
        }
        else if (guiderType == GUIDE_INTERNAL)
            capture();
    });

    // Framing
    connect(loopB, &QPushButton::clicked, this, &Guide::loop);

    // Stop
    connect(stopB, &QPushButton::clicked, this, &Ekos::Guide::abort);

    // Clear Calibrate
    //connect(calibrateB, &QPushButton::clicked, this, &Ekos::Guide::calibrate()));
    connect(clearCalibrationB, &QPushButton::clicked, this, &Ekos::Guide::clearCalibration);

    // Guide
    connect(guideB, &QPushButton::clicked, this, &Ekos::Guide::guide);

    // Connect External Guide
    connect(externalConnectB, &QPushButton::clicked, this, [&]()
    {
        //setExternalGuiderBLOBEnabled(false);
        m_GuiderInstance->Connect();
    });
    connect(externalDisconnectB, &QPushButton::clicked, this, [&]()
    {
        //setExternalGuiderBLOBEnabled(true);
        m_GuiderInstance->Disconnect();
    });

    // Pulse Timer
    m_PulseTimer.setSingleShot(true);
    connect(&m_PulseTimer, &QTimer::timeout, this, &Ekos::Guide::capture);

    //This connects all the buttons and slider below the guide plots.
    connect(accuracyRadiusSpin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            &Ekos::Guide::buildTarget);
    connect(guideSlider, &QSlider::sliderMoved, this, &Ekos::Guide::guideHistory);
    connect(latestCheck, &QCheckBox::toggled, this, &Ekos::Guide::setLatestGuidePoint);
    connect(showRAPlotCheck, &QCheckBox::toggled, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_RA, isChecked);
    });
    connect(showDECPlotCheck, &QCheckBox::toggled, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_DEC, isChecked);
    });
    connect(showRACorrectionsCheck, &QCheckBox::toggled, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_RA_PULSE, isChecked);
    });
    connect(showDECorrectionsCheck, &QCheckBox::toggled, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_DEC_PULSE, isChecked);
    });
    connect(showSNRPlotCheck, &QCheckBox::toggled, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_SNR, isChecked);
    });
    connect(showRMSPlotCheck, &QCheckBox::toggled, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_RMS, isChecked);
    });
    connect(correctionSlider, &QSlider::sliderMoved, driftGraph, &GuideDriftGraph::setCorrectionGraphScale);

    connect(manualDitherB, &QPushButton::clicked, this, &Guide::handleManualDither);

    connect(this, &Ekos::Guide::newStatus, guideStateWidget, &Ekos::GuideStateWidget::updateGuideStatus);
}

void Guide::removeDevice(ISD::GenericDevice *device)
{
    auto name = device->getDeviceName();

    device->disconnect(this);

    // Mounts
    for (auto &oneMount : m_Mounts)
    {
        if (oneMount->getDeviceName() == name)
        {
            m_Mounts.removeOne(oneMount);
            if (m_Mount && (m_Mount->getDeviceName() == name))
                m_Mount = nullptr;
            break;
        }
    }

    // Cameras
    for (auto &oneCamera : m_Cameras)
    {
        if (oneCamera->getDeviceName() == name)
        {
            m_Cameras.removeAll(oneCamera);
            cameraCombo->removeItem(cameraCombo->findText(name));
            cameraCombo->removeItem(cameraCombo->findText(name + " Guider"));
            if (m_Cameras.empty())
            {
                m_Camera = nullptr;
                cameraCombo->setCurrentIndex(-1);
            }
            else
            {
                m_Camera = m_Cameras[0];
                cameraCombo->setCurrentIndex(0);
            }

            QTimer::singleShot(1000, this, [this]()
            {
                checkCamera();
            });

            break;
        }
    }

    // Guiders
    for (auto &oneGuider : m_Guiders)
    {
        if (oneGuider->getDeviceName() == name)
        {
            m_Guiders.removeAll(oneGuider);
            guiderCombo->removeItem(guiderCombo->findText(name));
            if (m_Guiders.empty())
                m_Guider = nullptr;
            else
                setGuider(guiderCombo->currentText());
            break;
        }
    }
}

QJsonObject Guide::getSettings() const
{
    QJsonObject settings;

    settings.insert("camera", cameraCombo->currentText());
    settings.insert("via", guiderCombo->currentText());
    settings.insert("exp", exposureIN->value());
    settings.insert("bin", qMax(1, binningCombo->currentIndex() + 1));
    settings.insert("dark", darkFrameCheck->isChecked());
    settings.insert("box", boxSizeCombo->currentText());
    settings.insert("ra_control", checkBox_DirRA->isChecked());
    settings.insert("de_control", checkBox_DirDEC->isChecked());
    settings.insert("east", eastControlCheck->isChecked());
    settings.insert("west", westControlCheck->isChecked());
    settings.insert("north", northControlCheck->isChecked());
    settings.insert("south", southControlCheck->isChecked());
    settings.insert("scope", qMax(0, FOVScopeCombo->currentIndex()));
    settings.insert("swap", swapCheck->isChecked());
    settings.insert("ra_gain", Options::rAProportionalGain());
    settings.insert("de_gain", Options::dECProportionalGain());
    settings.insert("dither_enabled", Options::ditherEnabled());
    settings.insert("dither_pixels", Options::ditherPixels());
    settings.insert("dither_frequency", static_cast<int>(Options::ditherFrames()));
    settings.insert("gpg_enabled", Options::gPGEnabled());

    return settings;
}

void Guide::setSettings(const QJsonObject &settings)
{
    static bool init = false;

    auto syncControl = [settings](const QString & key, QWidget * widget)
    {
        if (settings.contains(key) == false)
            return false;

        QSpinBox *pSB = nullptr;
        QDoubleSpinBox *pDSB = nullptr;
        QCheckBox *pCB = nullptr;
        QComboBox *pComboBox = nullptr;

        if ((pSB = qobject_cast<QSpinBox *>(widget)))
        {
            const int value = settings[key].toInt(pSB->value());
            if (value != pSB->value())
            {
                pSB->setValue(value);
                return true;
            }
        }
        else if ((pDSB = qobject_cast<QDoubleSpinBox *>(widget)))
        {
            const double value = settings[key].toDouble(pDSB->value());
            if (value != pDSB->value())
            {
                pDSB->setValue(value);
                return true;
            }
        }
        else if ((pCB = qobject_cast<QCheckBox *>(widget)))
        {
            const bool value = settings[key].toBool(pCB->isChecked());
            if (value != pCB->isChecked())
            {
                pCB->setChecked(value);
                return true;
            }
        }
        // ONLY FOR STRINGS, not INDEX
        else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
        {
            const QString value = settings[key].toString(pComboBox->currentText());
            if (value != pComboBox->currentText())
            {
                pComboBox->setCurrentText(value);
                return true;
            }
        }

        return false;
    };

    // Camera
    if (syncControl("camera", cameraCombo) || init == false)
        checkCamera();
    // Via
    syncControl("via", guiderCombo);
    // Exposure
    syncControl("exp", exposureIN);
    // Binning
    const int bin = settings["bin"].toInt(binningCombo->currentIndex() + 1) - 1;
    if (bin != binningCombo->currentIndex())
        binningCombo->setCurrentIndex(bin);
    // Dark
    syncControl("dark", darkFrameCheck);
    // Box
    syncControl("box", boxSizeCombo);
    // Swap
    syncControl("swap", swapCheck);
    // RA Control
    syncControl("ra_control", checkBox_DirRA);
    // DE Control
    syncControl("de_control", checkBox_DirDEC);
    // NSWE controls
    syncControl("east", eastControlCheck);
    syncControl("west", westControlCheck);
    syncControl("north", northControlCheck);
    syncControl("south", southControlCheck);
    // Scope
    const int scope = settings["scope"].toInt(FOVScopeCombo->currentIndex());
    if (scope >= 0 && scope != FOVScopeCombo->currentIndex())
        FOVScopeCombo->setCurrentIndex(scope);
    // RA Gain
    syncControl("ra_gain", opsGuide->kcfg_RAProportionalGain);
    // DE Gain
    syncControl("de_gain", opsGuide->kcfg_DECProportionalGain);
    // Options
    const bool ditherEnabled = settings["dither_enabled"].toBool(Options::ditherEnabled());
    Options::setDitherEnabled(ditherEnabled);
    const double ditherPixels = settings["dither_pixels"].toDouble(Options::ditherPixels());
    Options::setDitherPixels(ditherPixels);
    const int ditherFrequency = settings["dither_frequency"].toInt(Options::ditherFrames());
    Options::setDitherFrames(ditherFrequency);
    const bool gpg = settings["gpg_enabled"].toBool(Options::gPGEnabled());
    Options::setGPGEnabled(gpg);

    init = true;
}

void Guide::loop()
{
    m_State = GUIDE_LOOPING;
    emit newStatus(m_State);

    if(guiderType == GUIDE_PHD2)
    {
        configurePHD2Camera();
        if(phd2Guider->isCurrentCameraNotInEkos())
            appendLogText(
                i18n("The PHD2 camera is not available to Ekos, so you cannot see the captured images.  But you will still see the Guide Star Image when you guide."));
        else if(Options::guideSubframeEnabled())
        {
            appendLogText(
                i18n("To receive PHD2 images other than the Guide Star Image, SubFrame must be unchecked.  Unchecking it now to enable your image captures.  You can re-enable it before Guiding"));
            subFrameCheck->setChecked(false);
        }
        phd2Guider->loop();
        stopB->setEnabled(true);
    }
    else if (guiderType == GUIDE_INTERNAL)
        capture();
}
}
