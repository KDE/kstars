/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guide.h"

#include "guideadaptor.h"
#include "kstars.h"
#include "ksmessagebox.h"
#include "kstarsdata.h"
#include "ksnotification.h"
#include "opscalibration.h"
#include "opsguide.h"
#include "opsdither.h"
#include "opsgpg.h"
#include "Options.h"
#include "indi/indiguider.h"
#include "indi/indiadaptiveoptics.h"
#include "auxiliary/QProgressIndicator.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/auxiliary/darklibrary.h"
#include "externalguide/linguider.h"
#include "externalguide/phd2.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsviewer.h"
#include "internalguide/internalguider.h"
#include "guideview.h"
#include "guidegraph.h"
#include "guidestatewidget.h"
#include "manualpulse.h"
#include "ekos/auxiliary/darkprocessor.h"

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
    connect(opsGuide, &OpsGuide::settingsUpdated, this, [this]()
    {
        onThresholdChanged(Options::guideAlgorithm());
        configurePHD2Camera();
        configSEPMultistarOptions(); // due to changes in 'Guide Setting: Algorithm'
        checkUseGuideHead();
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

    // #5 Init Connections
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

    // Icons
    focalLengthIcon->setPixmap(QIcon::fromTheme("gnumeric-formulaguru").pixmap(32, 32));
    apertureIcon->setPixmap(QIcon::fromTheme("lensautofix").pixmap(32, 32));
    reducerIcon->setPixmap(QIcon::fromTheme("format-align-vertical-bottom").pixmap(32, 32));
    FOVIcon->setPixmap(QIcon::fromTheme("timeline-use-zone-on").pixmap(32, 32));
    focalRatioIcon->setPixmap(QIcon::fromTheme("node-type-symmetric").pixmap(32, 32));

    // Exposure
    //Should we set the range for the spin box here?
    QList<double> exposureValues;
    exposureValues << 0.02 << 0.05 << 0.1 << 0.2 << 0.5 << 1 << 1.5 << 2 << 2.5 << 3 << 3.5 << 4 << 4.5 << 5 << 6 << 7 << 8 << 9
                   << 10 << 15 << 30;
    guideExposure->setRecommendedValues(exposureValues);
    connect(guideExposure, &NonLinearDoubleSpinBox::editingFinished, this, &Ekos::Guide::saveDefaultGuideExposure);

    // Set current guide type
    setGuiderType(-1);

    //This allows the current guideSubframe option to be loaded.
    if(guiderType == GUIDE_PHD2)
    {
        setExternalGuiderBLOBEnabled(!Options::guideSubframe());
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
        if (completed != guideDarkFrame->isChecked())
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

    m_ManaulPulse = new ManualPulse(this);
    connect(m_ManaulPulse, &ManualPulse::newSinglePulse, this, &Guide::sendSinglePulse);
    connect(manualPulseB, &QPushButton::clicked, this, [this]()
    {
        m_ManaulPulse->reset();
        m_ManaulPulse->show();
    });

    loadGlobalSettings();
    connectSettings();

    setupOpticalTrainManager();
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

void Guide::resizeEvent(QResizeEvent * event)
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
    targetPlot->buildTarget(guiderAccuracyThreshold->value());
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
    calRALabel->setVisible(false);
    calRAArrow->setVisible(false);
    calDECLabel->setVisible(false);
    calDECArrow->setVisible(false);
    calibrationPlot->replot();
}

void Guide::slotAutoScaleGraphs()
{
    driftGraph->zoomX(defaultXZoomLevel);

    driftGraph->autoScaleGraphs();
    targetPlot->autoScaleGraphs(guiderAccuracyThreshold->value());

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
    guideExposure->setRecommendedValues(values);
    return guideExposure->getRecommendedValuesString();
}

bool Guide::setCamera(ISD::Camera * device)
{
    if (m_Camera && device == m_Camera)
    {
        checkCamera();
        return false;
    }

    if (m_Camera)
        m_Camera->disconnect(this);

    m_Camera = device;

    if (m_Camera)
    {
        connect(m_Camera, &ISD::Camera::Connected, this, [this]()
        {
            controlGroupBox->setEnabled(true);
        });
        connect(m_Camera, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            controlGroupBox->setEnabled(false);
        });
    }

    controlGroupBox->setEnabled(m_Camera && m_Camera->isConnected());

    // If camera was reset, return now.
    if (!m_Camera)
        return false;

    if(guiderType != GUIDE_INTERNAL)
        m_Camera->setBLOBEnabled(false);

    checkCamera();
    configurePHD2Camera();

    // In case we are recovering from a crash and capture is pending, process it immediately.
    if (captureTimeout.isActive() && m_State >= Ekos::GUIDE_CAPTURE)
        QTimer::singleShot(100, this, &Guide::processCaptureTimeout);

    return true;
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
    if(m_Camera && phd2Guider->getCurrentCamera().contains(m_Camera->getDeviceName()))
    {
        ccdMatch = m_Camera;
        currentPHD2CameraName = (phd2Guider->getCurrentCamera());
    }

    //If this method gives the same result as last time, no need to update the Camera info again.
    //That way the user doesn't see a ton of messages printing about the PHD2 external camera.
    //But lets make sure the blob is set correctly every time.
    if(m_LastPHD2CameraName == currentPHD2CameraName)
    {
        setExternalGuiderBLOBEnabled(!guideSubframe->isChecked());
        return;
    }

    //This means that a Guide Camera was connected before but it changed.
    if(m_Camera)
        setExternalGuiderBLOBEnabled(false);

    //Updating the currentCCD
    m_Camera = ccdMatch;

    //This updates the last camera name for the next time it is checked.
    m_LastPHD2CameraName = currentPHD2CameraName;

    m_LastPHD2MountName = phd2Guider->getCurrentMount();

    //This sets a boolean that allows you to tell if the PHD2 camera is in Ekos
    phd2Guider->setCurrentCameraIsNotInEkos(m_Camera == nullptr);

    if(phd2Guider->isCurrentCameraNotInEkos())
    {
        appendLogText(
            i18n("PHD2's current camera: %1, is not connected to Ekos.  The PHD2 Guide Star Image will be received, but the full external guide frames cannot.",
                 phd2Guider->getCurrentCamera()));
        guideSubframe->setEnabled(false);
        //We don't want to actually change the user's subFrame Setting for when a camera really is connected, just check the box to tell the user.
        disconnect(guideSubframe, &QCheckBox::toggled, this, &Ekos::Guide::setSubFrameEnabled);
        guideSubframe->setChecked(true);
        return;
    }

    appendLogText(
        i18n("PHD2's current camera: %1, is connected to Ekos.  You can select whether to use the full external guide frames or just receive the PHD2 Guide Star Image using the SubFrame checkbox.",
             phd2Guider->getCurrentCamera()));
    guideSubframe->setEnabled(true);
    connect(guideSubframe, &QCheckBox::toggled, this, &Ekos::Guide::setSubFrameEnabled);
    guideSubframe->setChecked(guideSubframe->isChecked());
}

bool Guide::setMount(ISD::Mount * device)
{
    if (m_Mount && m_Mount == device)
    {
        syncTelescopeInfo();
        return false;
    }

    if (m_Mount)
        m_Mount->disconnect(this);

    m_Mount = device;
    syncTelescopeInfo();
    return true;
}

QString Guide::camera()
{
    if (m_Camera)
        return m_Camera->getDeviceName();

    return QString();
}

void Guide::checkCamera()
{
    // Do NOT perform checks when the camera is capturing as this may result
    // in signals/slots getting disconnected.
    if (!m_Camera || guiderType != GUIDE_INTERNAL)
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

    checkUseGuideHead();

    auto targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
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

    connect(m_Camera, &ISD::Camera::propertyUpdated, this, &Ekos::Guide::updateProperty, Qt::UniqueConnection);
    connect(m_Camera, &ISD::Camera::newExposureValue, this, &Ekos::Guide::checkExposureValue, Qt::UniqueConnection);

    syncCameraInfo();
}

void Ekos::Guide::checkUseGuideHead()
{
    if (m_Camera == nullptr)
        return;

    if (m_Camera->hasGuideHead() && Options::useGuideHead())
        useGuideHead = true;
    else
        useGuideHead = false;
    // guiding option only enabled if camera has a dedicated guiding chip
    opsGuide->kcfg_UseGuideHead->setEnabled(m_Camera->hasGuideHead());
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

void Guide::syncTelescopeInfo()
{
    if (m_Mount == nullptr || m_Mount->isConnected() == false)
        return;

    updateGuideParams();
}

void Guide::updateGuideParams()
{
    if (m_Camera == nullptr)
        return;

    checkUseGuideHead();

    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

    if (targetChip == nullptr)
    {
        appendLogText(i18n("Connection to the guide CCD is lost."));
        return;
    }

    if (targetChip->getFrameType() != FRAME_LIGHT)
        return;

    if(guiderType == GUIDE_INTERNAL)
        guideBinning->setEnabled(targetChip->canBin());

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

        guideBinning->blockSignals(true);

        guideBinning->clear();
        for (int i = 1; i <= maxBinX; i++)
            guideBinning->addItem(QString("%1x%2").arg(i).arg(i));

        guideBinning->setCurrentIndex( guideBinIndex );

        guideBinning->blockSignals(false);
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
                auto subframed = guideSubframe->isChecked();

                QVariantMap settings;

                settings["x"]    = subframed ? x : minX;
                settings["y"]    = subframed ? y : minY;
                settings["w"]    = subframed ? w : maxW;
                settings["h"]    = subframed ? h : maxH;
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

    if (ccdPixelSizeX != -1 && ccdPixelSizeY != -1 && m_FocalLength > 0)
    {
        auto effectiveFocaLength = m_Reducer * m_FocalLength;
        m_GuiderInstance->setGuiderParams(ccdPixelSizeX, ccdPixelSizeY, m_Aperture, effectiveFocaLength);
        emit guideChipUpdated(targetChip);

        int x, y, w, h;
        if (targetChip->getFrame(&x, &y, &w, &h))
        {
            m_GuiderInstance->setFrameParams(x, y, w, h, subBinX, subBinY);
        }

        l_Focal->setText(QString("%1mm").arg(m_FocalLength, 0, 'f', 0));
        if (m_Aperture > 0)
            l_Aperture->setText(QString("%1mm").arg(m_Aperture, 0, 'f', 0));
        // FIXME is there a way to know aperture?
        else
            l_Aperture->setText("DSLR");
        l_Reducer->setText(QString("%1x").arg(QString::number(m_Reducer, 'f', 2)));

        if (m_FocalRatio > 0)
            l_FbyD->setText(QString("F/%1").arg(m_FocalRatio, 0, 'f', 1));
        else if (m_Aperture > 0)
            l_FbyD->setText(QString("F/%1").arg(m_FocalLength / m_Aperture, 0, 'f', 1));

        // Pixel scale in arcsec/pixel
        pixScaleX = 206264.8062470963552 * ccdPixelSizeX / 1000.0 / effectiveFocaLength;
        pixScaleY = 206264.8062470963552 * ccdPixelSizeY / 1000.0 / effectiveFocaLength;

        // FOV in arcmin
        double fov_w = (w * pixScaleX) / 60.0;
        double fov_h = (h * pixScaleY) / 60.0;

        l_FOV->setText(QString("%1' x %2'").arg(QString::number(fov_w, 'f', 1), QString::number(fov_h, 'f', 1)));
    }

    // Gain Check
    if (m_Camera->hasGain())
    {
        double min, max, step, value;
        m_Camera->getGainMinMaxStep(&min, &max, &step);

        // Allow the possibility of no gain value at all.
        guideGainSpecialValue = min - step;
        guideGain->setRange(guideGainSpecialValue, max);
        guideGain->setSpecialValueText(i18n("--"));
        guideGain->setEnabled(true);
        guideGain->setSingleStep(step);
        m_Camera->getGain(&value);

        auto gain = m_Settings["guideGain"];
        // Set the custom gain if we have one
        // otherwise it will not have an effect.
        if (gain.isValid())
            TargetCustomGainValue = gain.toDouble();
        if (TargetCustomGainValue > 0)
            guideGain->setValue(TargetCustomGainValue);
        else
            guideGain->setValue(guideGainSpecialValue);

        guideGain->setReadOnly(m_Camera->getGainPermission() == IP_RO);

        connect(guideGain, &QDoubleSpinBox::editingFinished, this, [this]()
        {
            if (guideGain->value() > guideGainSpecialValue)
                TargetCustomGainValue = guideGain->value();
        });
    }
    else
        guideGain->setEnabled(false);
}

bool Guide::setGuider(ISD::Guider * device)
{
    if (guiderType != GUIDE_INTERNAL || (m_Guider && device == m_Guider))
        return false;

    if (m_Guider)
        m_Guider->disconnect(this);

    m_Guider = device;

    if (m_Guider)
    {
        connect(m_Guider, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            guideB->setEnabled(true);
        });
        connect(m_Guider, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            guideB->setEnabled(false);
        });
    }

    guideB->setEnabled(m_Guider && m_Guider->isConnected());
    return true;
}

bool Guide::setAdaptiveOptics(ISD::AdaptiveOptics * device)
{
    if (guiderType != GUIDE_INTERNAL || (m_AO && device == m_AO))
        return false;

    if (m_AO)
        m_AO->disconnect(this);

    // FIXME AO are not yet utilized property in Guide module
    m_AO = device;
    return true;
}

QString Guide::guider()
{
    if (guiderType != GUIDE_INTERNAL || m_Guider == nullptr)
        return QString();

    return m_Guider->getDeviceName();
}

bool Guide::capture()
{
    // Only capture if we're not already capturing.
    //
    // This is necessary due to the possibility of multiple asncronous actions in the guider.
    // It would be better to have a syncronous guider, but that is not the current guider.
    //
    // One situation I'm seeting this fire is with the internal guider, using GPG,
    // set to suspend guiding during focus, when focus has been completed and guiding moves
    // back from suspended to guiding. Here's why:
    // During focus, even though guiding is suspended and not sending guide pulses, it still
    // captures images and compute guide offsets and send those offset values to gpg
    // (see setCaptureComple() case GUIDE_SUSPENDED). When focus completes, guiding moves from
    // suspended back to guiding--resume() gets called (from the focuser connected through the
    // manager) which calls InternalGuider::resume() which emits frameCaptureRequested() which
    // calls Guide::capture().  So, there would likely be a call to capture() while the previous
    // gpg-induced capture is still running (above setCaptureComplete case GUIDE_SUSPENDED).
    // I'm leaving this behavior in the code, as this seems like an ok solution.
    //
    // Similarly, this duplicate capturing can happen during ordinary guiding and dithering. Guiding
    // captures, then recieves an image. Around the same time capture recieves an image and decides it's
    // time to dither (guiding hasn't yet processed the image it received). For whatever reason, guiding
    // does the dither processing first (say it's one-pulse-dither and it's done quickly, and emits the
    // signal that generates the dither pulses). Then the previous guide processing happens, and it
    // completes and sends out its guide pulses followed by a signal to recapture. Then the dither settle
    // happens and it tries to recapture after its settle time completes.
    if (guiderType == GUIDE_INTERNAL && captureTimeout.isActive() && captureTimeout.remainingTime() > 0)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Internal guider skipping capture, already running with remaining seconds =" <<
                                   captureTimeout.remainingTime() / 1000.0;
        return false;
    }

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

    double seqExpose = guideExposure->value();

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
    if (operationStack.contains(GUIDE_STAR_SELECT) && guideAutoStar->isChecked() &&
            !((guiderType == GUIDE_INTERNAL) && internalGuider->SEPMultiStarEnabled()))
        finalExposure *= 3;

    // Prevent flicker when processing dark frame by suspending updates
    m_GuideView->setProperty("suspended", operationStack.contains(GUIDE_DARK));

    // Timeout is exposure duration + timeout threshold in seconds
    captureTimeout.start(finalExposure * 1000 + CAPTURE_TIMEOUT_THRESHOLD);

    targetChip->capture(finalExposure);

    return true;
}

void Guide::prepareCapture(ISD::CameraChip * targetChip)
{
    targetChip->setBatchMode(false);
    targetChip->setCaptureMode(FITS_GUIDE);
    targetChip->setFrameType(FRAME_LIGHT);
    targetChip->setCaptureFilter(FITS_NONE);
    m_Camera->setEncodingFormat("FITS");

    // Set gain if applicable
    if (m_Camera->hasGain() && guideGain->isEnabled() && guideGain->value() > guideGainSpecialValue)
        m_Camera->setGain(guideGain->value());
}

void Guide::abortExposure()
{
    if (m_Camera && guiderType == GUIDE_INTERNAL)
    {
        captureTimeout.stop();
        m_PulseTimer.stop();
        ISD::CameraChip *targetChip =
            m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        if (targetChip->isCapturing())
        {
            qCDebug(KSTARS_EKOS_GUIDE) << "Aborting guide capture";
            targetChip->abortExposure();
        }
    }
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
        guideDarkFrame->setEnabled(false);
        guideSubframe->setEnabled(false);
        guideAutoStar->setEnabled(false);
        stopB->setEnabled(true);
        // Optical Train
        opticalTrainCombo->setEnabled(false);
        trainB->setEnabled(false);

        pi->startAnimation();
    }
    else
    {
        if(guiderType != GUIDE_LINGUIDER)
        {
            captureB->setEnabled(true);
            loopB->setEnabled(true);
            guideAutoStar->setEnabled(!internalGuider->SEPMultiStarEnabled()); // cf. configSEPMultistarOptions()
            if(m_Camera)
                guideSubframe->setEnabled(!internalGuider->SEPMultiStarEnabled()); // cf. configSEPMultistarOptions()
        }
        if (guiderType == GUIDE_INTERNAL)
            guideDarkFrame->setEnabled(true);

        if (calibrationComplete ||
                ((guiderType == GUIDE_INTERNAL) &&
                 Options::reuseGuideCalibration() &&
                 !Options::serializedCalibration().isEmpty()))
            clearCalibrationB->setEnabled(true);
        guideB->setEnabled(true);
        stopB->setEnabled(false);
        pi->stopAnimation();

        // Optical Train
        opticalTrainCombo->setEnabled(true);
        trainB->setEnabled(true);

        connect(m_GuideView.get(), &FITSView::trackingStarSelected, this, &Ekos::Guide::setTrackingStar, Qt::UniqueConnection);
    }
}

void Guide::processCaptureTimeout()
{
    // Don't restart if we've since been suspended.
    if (m_State == GUIDE_SUSPENDED)
    {
        appendLogText(i18n("Exposure timeout, but suspended. Ignoring..."));
        return;
    }

    auto restartExposure = [&]()
    {
        appendLogText(i18n("Exposure timeout. Restarting exposure..."));
        m_Camera->setEncodingFormat("FITS");
        ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        targetChip->abortExposure();
        prepareCapture(targetChip);
        connect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Guide::processData, Qt::UniqueConnection);
        connect(m_Camera, &ISD::Camera::propertyUpdated, this, &Ekos::Guide::updateProperty, Qt::UniqueConnection);
        connect(m_Camera, &ISD::Camera::newExposureValue, this, &Ekos::Guide::checkExposureValue, Qt::UniqueConnection);
        targetChip->capture(guideExposure->value());
        captureTimeout.start(guideExposure->value() * 1000 + CAPTURE_TIMEOUT_THRESHOLD);
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

    if (m_CaptureTimeoutCounter > 3)
    {
        appendLogText(i18n("Exposure timeout. Too many. Restarting driver."));
        QString camera = m_Camera->getDeviceName();
        QString via = m_Guider ? m_Guider->getDeviceName() : "";
        ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
        QVariantMap settings = frameSettings[targetChip];
        emit driverTimedout(camera);
        QTimer::singleShot(5000, [ &, camera, settings]()
        {
            m_DeviceRestartCounter++;
            reconnectDriver(camera, settings);
        });
        return;
    }
    else
        restartExposure();
}

void Guide::reconnectDriver(const QString &camera, QVariantMap settings)
{
    if (m_Camera && m_Camera->getDeviceName() == camera)
    {
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

    QTimer::singleShot(5000, this, [ &, camera, settings]()
    {
        reconnectDriver(camera, settings);
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

    captureTimeout.stop();
    m_CaptureTimeoutCounter = 0;

    if (data)
    {
        m_GuideView->loadData(data);
        m_ImageData = data;
    }
    else
        m_ImageData.reset();

    if (guiderType == GUIDE_INTERNAL)
        internalGuider->setImageData(m_ImageData);

    disconnect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Guide::processData);

    // qCDebug(KSTARS_EKOS_GUIDE) << "Received guide frame.";

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
    // qCDebug(KSTARS_EKOS_GUIDE) << "Tracking box position synched.";

    setCaptureComplete();
    // qCDebug(KSTARS_EKOS_GUIDE) << "Capture complete.";

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

    qCDebug(KSTARS_EKOS_GUIDE) << "Capture complete, state=" << getGuideStatusString(m_State);
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
            qCDebug(KSTARS_EKOS_GUIDE) << "Guiding capture complete.";
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
            // Do nothing
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

bool Guide::sendMultiPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs,
                           CaptureAfterPulses followWithCapture)
{
    if (m_Guider == nullptr || (ra_dir == NO_DIR && dec_dir == NO_DIR))
        return false;

    if (followWithCapture == StartCaptureAfterPulses)
    {
        // Delay next capture by user-configurable delay.
        // If user delay is zero, delay by the pulse length plus 100 milliseconds before next capture.
        auto ms = std::max(ra_msecs, dec_msecs) + 100;
        auto delay = std::max(static_cast<int>(guideDelay->value() * 1000), ms);

        m_PulseTimer.start(delay);
    }
    return m_Guider->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
}

bool Guide::sendSinglePulse(GuideDirection dir, int msecs, CaptureAfterPulses followWithCapture)
{
    if (m_Guider == nullptr || dir == NO_DIR)
        return false;

    if (followWithCapture == StartCaptureAfterPulses)
    {
        // Delay next capture by user-configurable delay.
        // If user delay is zero, delay by the pulse length plus 100 milliseconds before next capture.
        auto ms = msecs + 100;
        auto delay = std::max(static_cast<int>(guideDelay->value() * 1000), ms);

        m_PulseTimer.start(delay);
    }

    return m_Guider->doPulse(dir, msecs);
}

bool Guide::calibrate()
{
    // Set status to idle and let the operations change it as they get executed
    m_State = GUIDE_IDLE;
    qCDebug(KSTARS_EKOS_GUIDE) << "Calibrating...";
    emit newStatus(m_State);

    if (guiderType == GUIDE_INTERNAL)
    {
        if (!m_Camera)
        {
            KSNotification::error(i18n("No camera detected. Check camera in optical trains."));
            return false;
        }

        if (!m_Guider)
        {
            KSNotification::error(i18n("No guider detected. Check guide-via in optical trains."));
            return false;
        }

        auto targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

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

    buildOperationStack(GUIDE_CALIBRATING);

    executeOperationStack();

    if (m_Camera && m_Guider)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Starting calibration using camera:" << m_Camera->getDeviceName() << "via" <<
                                   m_Guider->getDeviceName();
    }

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

        m_GuiderInstance->guide();

        //If PHD2 gets a Guide command and it is looping, it will accept a lock position
        //but if it was not looping it will ignore the lock position and do an auto star automatically
        //This is not the default behavior in Ekos if auto star is not selected.
        //This gets around that by noting the position of the tracking box, and enforcing it after the state switches to guide.
        if(!guideAutoStar->isChecked())
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

    if (guiderType == GUIDE_INTERNAL && !Options::ditherWithOnePulse())
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
            manualPulseB->setEnabled(false);
            break;

        default:
            if (pi->isAnimated() == false)
            {
                captureB->setEnabled(true);
                loopB->setEnabled(true);
                clearCalibrationB->setEnabled(true);
                manualPulseB->setEnabled(true);
            }
    }
}

void Guide::setMountCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha)
{
    Q_UNUSED(ha);
    m_GuiderInstance->setMountCoords(position, pierSide);
    m_ManaulPulse->setMountCoords(position);
}

void Guide::setExposure(double value)
{
    guideExposure->setValue(value);
}

void Guide::setSubFrameEnabled(bool enable)
{
    if (guideSubframe->isChecked() != enable)
        guideSubframe->setChecked(enable);
    if(guiderType == GUIDE_PHD2)
        setExternalGuiderBLOBEnabled(!enable);
}

void Guide::setAutoStarEnabled(bool enable)
{
    if(guiderType == GUIDE_INTERNAL)
        guideAutoStar->setChecked(enable);
}

void Guide::clearCalibration()
{
    calibrationComplete = false;
    if (m_GuiderInstance->clearCalibration())
    {
        clearCalibrationB->setEnabled(false);
        appendLogText(i18n("Calibration is cleared."));
    }


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
                guideAutoStar->setEnabled(true);
                configurePHD2Camera();
                setExternalGuiderBLOBEnabled(!guideSubframe->isChecked());
                guideSquareSize->setEnabled(true);
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
            guideAutoStar->setEnabled(false);
            guideSquareSize->setEnabled(false);
            //setExternalGuiderBLOBEnabled(true);
#ifdef Q_OS_MACOS
            repaint(); //This is a band-aid for a bug in QT 5.10.0
#endif
            break;

        case GUIDE_CALIBRATION_SUCCESS:
            appendLogText(i18n("Calibration completed."));
            manualPulseB->setEnabled(true);
            calibrationComplete = true;

            if(guiderType !=
                    GUIDE_PHD2) //PHD2 will take care of this.  If this command is executed for PHD2, it might start guiding when it is first connected, if the calibration was completed already.
                guide();
            break;

        case GUIDE_IDLE:
        case GUIDE_CALIBRATION_ERROR:
            setBusy(false);
            manualDitherB->setEnabled(false);
            manualPulseB->setEnabled(true);
            break;

        case GUIDE_CALIBRATING:
            clearCalibrationGraphs();
            appendLogText(i18n("Calibration started."));
            setBusy(true);
            manualPulseB->setEnabled(false);
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
}

void Guide::updateProperty(INDI::Property prop)
{
    if (m_Camera == nullptr || (prop.getDeviceName() != m_Camera->getDeviceName()) || guiderType != GUIDE_INTERNAL)
        return;

    if ((prop.isNameMatch("CCD_BINNING") && useGuideHead == false) ||
            (prop.isNameMatch("GUIDER_BINNING") && useGuideHead))
    {
        auto nvp = prop.getNumber();
        auto value = nvp->at(0)->getValue();
        if (guideBinIndex > (value - 1)) // INDI driver reports not supported binning
        {
            appendLogText(i18n("%1x%1 guide binning is not supported.", guideBinIndex + 1));
            guideBinning->setCurrentIndex( value - 1 );
            updateSetting("guideBinning", guideBinning->currentText());
        }
        else
        {
            guideBinning->setCurrentIndex(guideBinIndex);
        }
    }
}

void Guide::checkExposureValue(ISD::CameraChip * targetChip, double exposure, IPState expState)
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
        targetChip->capture(guideExposure->value());
    }
}

void Guide::configSEPMultistarOptions()
{
    // SEP MultiStar always uses an automated guide star & doesn't subframe.
    if (internalGuider->SEPMultiStarEnabled())
    {
        guideSubframe->setChecked(false);
        guideSubframe->setEnabled(false);
        guideAutoStar->setChecked(true);
        guideAutoStar->setEnabled(false);
    }
    else
    {
        guideAutoStar->setEnabled(true);
        guideSubframe->setEnabled(true);

        auto subframed = m_Settings["guideSubframe"];
        if (subframed.isValid())
            guideSubframe->setChecked(subframed.toBool());

        auto autostar = m_Settings["guideAutoStar"];
        if (autostar.isValid())
            guideAutoStar->setChecked(autostar.toBool());
    }
}

void Guide::setDarkFrameEnabled(bool enable)
{
    if (guideDarkFrame->isChecked() != enable)
        guideDarkFrame->setChecked(enable);
}

void Guide::saveDefaultGuideExposure()
{
    if(guiderType == GUIDE_PHD2)

        phd2Guider->requestSetExposureTime(guideExposure->value() * 1000);
    else if (guiderType == GUIDE_INTERNAL)
    {
        internalGuider->setExposureTime();
    }
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
        double boxSize = guideSquareSize->currentText().toInt();
        int x, y, w, h;
        targetChip->getFrame(&x, &y, &w, &h);
        // If box size is larger than image size, set it to lower index
        if (boxSize / subBinX >= w || boxSize / subBinY >= h)
        {
            int newIndex = guideSquareSize->currentIndex() - 1;
            if (newIndex >= 0)
                guideSquareSize->setCurrentIndex(newIndex);
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
            connect(internalGuider, &InternalGuider::DESwapChanged, this, &Guide::setDECSwap);
            connect(internalGuider, &InternalGuider::newStarPixmap, this, &Guide::newStarPixmap);

            m_GuiderInstance = internalGuider;

            internalGuider->setSquareAlgorithm(opsGuide->kcfg_GuideAlgorithm->currentIndex());

            clearCalibrationB->setEnabled(true);
            guideB->setEnabled(true);
            captureB->setEnabled(true);
            loopB->setEnabled(true);

            configSEPMultistarOptions();
            guideDarkFrame->setEnabled(true);

            guideExposure->setEnabled(true);
            guideBinning->setEnabled(true);
            guideSquareSize->setEnabled(true);

            externalConnectB->setEnabled(false);
            externalDisconnectB->setEnabled(false);

            opsGuide->controlGroup->setEnabled(true);
            infoGroup->setEnabled(true);
            l_Aperture->setEnabled(true);
            l_FOV->setEnabled(true);
            l_FbyD->setEnabled(true);
            l_Focal->setEnabled(true);
            driftGraphicsGroup->setEnabled(true);

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
            guideDarkFrame->setEnabled(false);
            guideSubframe->setEnabled(false);
            guideAutoStar->setEnabled(false);
            guideB->setEnabled(false); //This will be enabled later when equipment connects (or not)
            externalConnectB->setEnabled(false);

            rAGuideEnabled->setEnabled(false);
            eastRAGuideEnabled->setEnabled(false);
            westRAGuideEnabled->setEnabled(false);

            opsGuide->controlGroup->setEnabled(false);
            infoGroup->setEnabled(true);
            l_Aperture->setEnabled(false);
            l_FOV->setEnabled(false);
            l_FbyD->setEnabled(false);
            l_Focal->setEnabled(false);
            driftGraphicsGroup->setEnabled(true);

            guideExposure->setEnabled(true);
            guideBinning->setEnabled(false);
            guideSquareSize->setEnabled(false);

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
            guideDarkFrame->setEnabled(false);
            guideSubframe->setEnabled(false);
            guideAutoStar->setEnabled(false);
            guideB->setEnabled(true);
            externalConnectB->setEnabled(true);

            opsGuide->controlGroup->setEnabled(false);
            infoGroup->setEnabled(false);
            driftGraphicsGroup->setEnabled(false);

            guideExposure->setEnabled(false);
            guideBinning->setEnabled(false);
            guideSquareSize->setEnabled(false);

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
        connect(m_GuiderInstance, &Ekos::GuideInterface::guideInfo, this, &Ekos::Guide::guideInfo);
        connect(m_GuiderInstance, &Ekos::GuideInterface::abortExposure, this, &Ekos::Guide::abortExposure);

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

void Guide::guideInfo(const QString &info)
{
    if (info.size() == 0)
    {
        guideInfoLabel->setVisible(false);
        guideInfoText->setVisible(false);
        return;
    }
    guideInfoLabel->setVisible(true);
    guideInfoLabel->setText("Detections");
    guideInfoText->setVisible(true);
    guideInfoText->setText(info);
}

void Guide::updateTrackingBoxSize(int currentIndex)
{
    if (currentIndex >= 0)
    {
        if (guiderType == GUIDE_INTERNAL)
            dynamic_cast<InternalGuider *>(m_GuiderInstance)->setGuideBoxSize(guideSquareSize->currentText().toInt());

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

void Guide::onEnableDirRA()
{
    // If RA guiding is enable or disabled, the GPG should be reset.
    if (Options::gPGEnabled())
        m_GuiderInstance->resetGPG();
}

void Guide::onEnableDirDEC()
{
    onControlDirectionChanged();
}

void Guide::onControlDirectionChanged()
{
    if(guiderType == GUIDE_PHD2)
        phd2Guider -> requestSetDEGuideMode(dECGuideEnabled->isChecked(), northDECGuideEnabled->isChecked(),
                                            southDECGuideEnabled->isChecked());
}

void Guide::updateDirectionsFromPHD2(const QString &mode)
{
    //disable connections
    disconnect(dECGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirDEC);
    disconnect(northDECGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    disconnect(southDECGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);

    if(mode == "Auto")
    {
        dECGuideEnabled->setChecked(true);
        northDECGuideEnabled->setChecked(true);
        southDECGuideEnabled->setChecked(true);
    }
    else if(mode == "North")
    {
        dECGuideEnabled->setChecked(true);
        northDECGuideEnabled->setChecked(true);
        southDECGuideEnabled->setChecked(false);
    }
    else if(mode == "South")
    {
        dECGuideEnabled->setChecked(true);
        northDECGuideEnabled->setChecked(false);
        southDECGuideEnabled->setChecked(true);
    }
    else //Off
    {
        dECGuideEnabled->setChecked(false);
        northDECGuideEnabled->setChecked(true);
        southDECGuideEnabled->setChecked(true);
    }

    //Re-enable connections
    connect(dECGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirDEC);
    connect(northDECGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(southDECGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
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
        case GuideInterface::RA_OUT_OK:
            drawRADECAxis(calRALabel, calRAArrow, dx, dy);
            break;
        case GuideInterface::RA_IN:
            calibrationPlot->graph(GuideGraph::G_DEC)->addData(dx, dy);
            calDecArrowStartX = dx;
            calDecArrowStartY = dy;
            break;
        case GuideInterface::BACKLASH:
            calibrationPlot->graph(GuideGraph::G_RA_HIGHLIGHT)->addData(dx, dy);
            calDecArrowStartX = dx;
            calDecArrowStartY = dy;
            break;
        case GuideInterface::DEC_OUT:
            calibrationPlot->graph(GuideGraph::G_DEC_HIGHLIGHT)->addData(dx, dy);
            break;
        case GuideInterface::DEC_OUT_OK:
            drawRADECAxis(calDECLabel, calDECArrow, dx, dy);
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

void Guide::drawRADECAxis(QCPItemText *Label, QCPItemLine *Arrow, const double xEnd, const double yEnd)
{

    Arrow->start->setCoords(calDecArrowStartX, calDecArrowStartY);
    Arrow->end->setCoords(xEnd, yEnd);
    Arrow->setHead(QCPLineEnding::esSpikeArrow);
    Label->position->setCoords(xEnd, yEnd);
    Label->setColor(Qt::white);
    yEnd > 0 ? Label->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom) :
    Label->setPositionAlignment(Qt::AlignHCenter | Qt::AlignTop);
    Arrow->setVisible(true);
    Label->setVisible(true);
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
            if (guideDarkFrame->isChecked())
                operationStack.push(GUIDE_DARK);

            operationStack.push(GUIDE_CAPTURE);
            operationStack.push(GUIDE_SUBFRAME);
            break;

        case GUIDE_CALIBRATING:
            operationStack.push(GUIDE_CALIBRATING);
            if (guiderType == GUIDE_INTERNAL)
            {
                if (guideDarkFrame->isChecked())
                    operationStack.push(GUIDE_DARK);

                // Auto Star Selected Path
                if (guideAutoStar->isChecked() ||
                        // SEP MultiStar always uses an automated guide star.
                        internalGuider->SEPMultiStarEnabled())
                {
                    // If subframe is enabled and we need to auto select a star, then we need to make the final capture
                    // of the subframed image. This is only done if we aren't already subframed.
                    if (subFramed == false && guideSubframe->isChecked())
                        operationStack.push(GUIDE_CAPTURE);

                    operationStack.push(GUIDE_SUBFRAME);
                    operationStack.push(GUIDE_STAR_SELECT);


                    operationStack.push(GUIDE_CAPTURE);

                    // If we are being ask to go full frame, let's do that first
                    if (subFramed == true && guideSubframe->isChecked() == false)
                        operationStack.push(GUIDE_SUBFRAME);
                }
                // Manual Star Selection Path
                else
                {
                    // Final capture before we start calibrating
                    if (subFramed == false && guideSubframe->isChecked())
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
    // qCDebug(KSTARS_EKOS_GUIDE) << "Executing operation " << getGuideStatusString(nextOperation);

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
            if (subFramed == false && guideSubframe->isChecked() == true && targetChip->canSubframe())
            {
                int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
                targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

                int offset = guideSquareSize->currentText().toInt() / subBinX;

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
                     (guideSubframe->isChecked() == false ||
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
            if (m_ImageData && guideDarkFrame->isChecked())
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
                m_DarkProcessor->denoise(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()),
                                         targetChip, m_ImageData, guideExposure->value(), offsetX, offsetY);
            }
        }
        break;

        case GUIDE_STAR_SELECT:
        {
            m_State = GUIDE_STAR_SELECT;
            emit newStatus(m_State);

            if (guideAutoStar->isChecked() ||
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
            connect(fv.get(), &FITSViewer::terminated, this, [this]()
            {
                fv.clear();
            });
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
        checkUseGuideHead();

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
                             decMsec, DontCaptureAfterPulses);

    if (rc)
    {
        qCInfo(KSTARS_EKOS_GUIDE) << "Non-guiding dither successful.";
        QTimer::singleShot( (raMsec > decMsec ? raMsec : decMsec) + Options::ditherSettle() * 1000 + 100, this, [this]()
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

    calibrationPlot->xAxis->setLabel(i18n("dx (pixels)"));
    calibrationPlot->yAxis->setLabel(i18n("dy (pixels)"));

    calibrationPlot->xAxis->setRange(-20, 20);
    calibrationPlot->yAxis->setRange(-20, 20);

    calibrationPlot->setInteractions(QCP::iRangeZoom);
    calibrationPlot->setInteraction(QCP::iRangeDrag, true);

    calibrationPlot->addGraph();
    calibrationPlot->graph(GuideGraph::G_RA)->setLineStyle(QCPGraph::lsNone);
    calibrationPlot->graph(GuideGraph::G_RA)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,
            QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError"), 2),
            QBrush(), 6));
    calibrationPlot->graph(GuideGraph::G_RA)->setName("RA+");

    calibrationPlot->addGraph();
    calibrationPlot->graph(GuideGraph::G_DEC)->setLineStyle(QCPGraph::lsNone);
    calibrationPlot->graph(GuideGraph::G_DEC)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,
            QPen(Qt::white, 2),
            QBrush(), 4));
    calibrationPlot->graph(GuideGraph::G_DEC)->setName("RA-");

    calibrationPlot->addGraph();
    calibrationPlot->graph(GuideGraph::G_RA_HIGHLIGHT)->setLineStyle(QCPGraph::lsNone);
    calibrationPlot->graph(GuideGraph::G_RA_HIGHLIGHT)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus,
            QPen(Qt::white, 2),
            QBrush(), 6));
    calibrationPlot->graph(GuideGraph::G_RA_HIGHLIGHT)->setName("Backlash");

    calibrationPlot->addGraph();
    calibrationPlot->graph(GuideGraph::G_DEC_HIGHLIGHT)->setLineStyle(QCPGraph::lsNone);
    calibrationPlot->graph(GuideGraph::G_DEC_HIGHLIGHT)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,
            QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError"), 2),
            QBrush(), 6));
    calibrationPlot->graph(GuideGraph::G_DEC_HIGHLIGHT)->setName("DEC+");

    calibrationPlot->addGraph();
    calibrationPlot->graph(GuideGraph::G_RA_PULSE)->setLineStyle(QCPGraph::lsNone);
    calibrationPlot->graph(GuideGraph::G_RA_PULSE)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,
            QPen(Qt::yellow, 2),
            QBrush(), 4));
    calibrationPlot->graph(GuideGraph::G_RA_PULSE)->setName("DEC-");

    calLabel = new QCPItemText(calibrationPlot);
    calLabel->setColor(QColor(255, 255, 255));
    calLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    calLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
    calLabel->position->setCoords(0.5, 0);
    calLabel->setText("");
    calLabel->setFont(QFont(font().family(), 10));
    calLabel->setVisible(true);

    calRALabel = new QCPItemText(calibrationPlot);
    calRALabel->setText("RA");
    calRALabel->setColor(Qt::white);
    calRALabel->setPen(QPen(Qt::white, 1)); // Draw frame
    calRALabel->setVisible(false);
    calRAArrow = new QCPItemLine(calibrationPlot);
    calRAArrow->setPen(QPen(Qt::white, 1));
    calRAArrow->setHead(QCPLineEnding::esSpikeArrow);
    calRAArrow->setVisible(false);

    calDECLabel = new QCPItemText(calibrationPlot);
    calDECLabel->setText("DEC");
    calDECLabel->setColor(Qt::white); // Draw frame
    calDECLabel->setPen(QPen(Qt::white, 1));
    calDECLabel->setVisible(false);
    calDECArrow = new QCPItemLine(calibrationPlot);
    calDECArrow->setPen(QPen(Qt::white, 1));
    calDECArrow->setHead(QCPLineEnding::esSpikeArrow);
    calDECArrow->setVisible(false);

    calDecArrowStartX = 0;
    calDecArrowStartY = 0;

    calibrationPlot->resize(190, 190);
    calibrationPlot->replot();
}

void Guide::initView()
{
    guideStateWidget = new GuideStateWidget();
    guideInfoLayout->insertWidget(-1, guideStateWidget);

    m_GuideView.reset(new GuideView(guideWidget, FITS_GUIDE));
    m_GuideView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_GuideView->setBaseSize(guideWidget->size());
    m_GuideView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(m_GuideView.get());
    guideWidget->setLayout(vlayout);
    connect(m_GuideView.get(), &FITSView::trackingStarSelected, this, &Ekos::Guide::setTrackingStar);
    guideInfoLabel->setVisible(false);
    guideInfoText->setVisible(false);
}

void Guide::initConnections()
{
    // Exposure Timeout
    captureTimeout.setSingleShot(true);
    connect(&captureTimeout, &QTimer::timeout, this, &Ekos::Guide::processCaptureTimeout);

    // Setup Debounce timer to limit over-activation of settings changes
    m_DebounceTimer.setInterval(500);
    m_DebounceTimer.setSingleShot(true);
    connect(&m_DebounceTimer, &QTimer::timeout, this, &Guide::settleSettings);

    // Guiding Box Size
    connect(guideSquareSize, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Guide::updateTrackingBoxSize);

    // Dark Frame Check
    connect(guideDarkFrame, &QCheckBox::toggled, this, &Ekos::Guide::setDarkFrameEnabled);
    // Subframe check
    if(guiderType != GUIDE_PHD2) //For PHD2, this is handled in the configurePHD2Camera method
        connect(guideSubframe, &QCheckBox::toggled, this, &Ekos::Guide::setSubFrameEnabled);

    // Binning Combo Selection
    connect(guideBinning, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &Ekos::Guide::updateCCDBin);

    // RA/DEC Enable directions
    connect(rAGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirRA);
    connect(dECGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onEnableDirDEC);

    // N/W and W/E direction enable
    connect(northDECGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(southDECGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(westRAGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);
    connect(eastRAGuideEnabled, &QCheckBox::toggled, this, &Ekos::Guide::onControlDirectionChanged);

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
            else if(guideSubframe->isChecked())
            {
                appendLogText(
                    i18n("To receive PHD2 images other than the Guide Star Image, SubFrame must be unchecked.  Unchecking it now to enable your image captures.  You can re-enable it before Guiding"));
                guideSubframe->setChecked(false);
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
    connect(guiderAccuracyThreshold, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            &Ekos::Guide::buildTarget);
    connect(guideSlider, &QSlider::sliderMoved, this, &Ekos::Guide::guideHistory);
    connect(latestCheck, &QCheckBox::toggled, this, &Ekos::Guide::setLatestGuidePoint);
    connect(rADisplayedOnGuideGraph, &QCheckBox::toggled, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_RA, isChecked);
    });
    connect(dEDisplayedOnGuideGraph, &QCheckBox::toggled,  this, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_DEC, isChecked);
    });
    connect(rACorrDisplayedOnGuideGraph, &QCheckBox::toggled,  this, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_RA_PULSE, isChecked);
    });
    connect(dECorrDisplayedOnGuideGraph, &QCheckBox::toggled,  this, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_DEC_PULSE, isChecked);
    });
    connect(sNRDisplayedOnGuideGraph, &QCheckBox::toggled,  this, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_SNR, isChecked);
    });
    connect(rMSDisplayedOnGuideGraph, &QCheckBox::toggled,  this, [this](bool isChecked)
    {
        driftGraph->toggleShowPlot(GuideGraph::G_RMS, isChecked);
    });
    connect(correctionSlider, &QSlider::sliderMoved, driftGraph, &GuideDriftGraph::setCorrectionGraphScale);

    connect(manualDitherB, &QPushButton::clicked, this, &Guide::handleManualDither);

    connect(this, &Ekos::Guide::newStatus, guideStateWidget, &Ekos::GuideStateWidget::updateGuideStatus);
}

void Guide::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    auto name = device->getDeviceName();

    device->disconnect(this);

    // Mounts
    if (m_Mount && m_Mount->getDeviceName() == name)
    {
        m_Mount->disconnect(this);
        m_Mount = nullptr;
    }


    // Cameras
    if (m_Camera && m_Camera->getDeviceName() == name)
    {
        m_Camera->disconnect(this);
        m_Camera = nullptr;
    }


    // Guiders
    if (m_Guider && m_Guider->getDeviceName() == name)
    {
        m_Guider->disconnect(this);
        m_Guider = nullptr;
    }

    // Adaptive Optics
    // FIXME AO are not yet utilized property in Guide module
    if (m_AO && m_AO->getDeviceName() == name)
    {
        m_AO->disconnect(this);
        m_AO = nullptr;
    }
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
        else if(guideSubframe->isChecked())
        {
            appendLogText(
                i18n("To receive PHD2 images other than the Guide Star Image, SubFrame must be unchecked.  Unchecking it now to enable your image captures.  You can re-enable it before Guiding"));
            guideSubframe->setChecked(false);
        }
        phd2Guider->loop();
        stopB->setEnabled(true);
    }
    else if (guiderType == GUIDE_INTERNAL)
        capture();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
QVariantMap Guide::getAllSettings() const
{
    QVariantMap settings;

    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->currentText());

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    return settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Guide::setAllSettings(const QVariantMap &settings)
{
    // Disconnect settings that we don't end up calling syncSettings while
    // performing the changes.
    disconnectSettings();

    for (auto &name : settings.keys())
    {
        // Combo
        auto comboBox = findChild<QComboBox*>(name);
        if (comboBox)
        {
            syncControl(settings, name, comboBox);
            continue;
        }

        // Double spinbox
        auto doubleSpinBox = findChild<QDoubleSpinBox*>(name);
        if (doubleSpinBox)
        {
            syncControl(settings, name, doubleSpinBox);
            continue;
        }

        // spinbox
        auto spinBox = findChild<QSpinBox*>(name);
        if (spinBox)
        {
            syncControl(settings, name, spinBox);
            continue;
        }

        // checkbox
        auto checkbox = findChild<QCheckBox*>(name);
        if (checkbox)
        {
            syncControl(settings, name, checkbox);
            continue;
        }
    }

    // Sync to options
    for (auto &key : settings.keys())
    {
        auto value = settings[key];
        // Save immediately
        Options::self()->setProperty(key.toLatin1(), value);

        m_Settings[key] = value;
        m_GlobalSettings[key] = value;
    }

    emit settingsUpdated(getAllSettings());

    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Guide, m_Settings);

    // Restablish connections
    connectSettings();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool Guide::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QComboBox *pComboBox = nullptr;
    QRadioButton *pRadioButton = nullptr;
    bool ok = false;

    if ((pSB = qobject_cast<QSpinBox *>(widget)))
    {
        const int value = settings[key].toInt(&ok);
        if (ok)
        {
            pSB->setValue(value);
            return true;
        }
    }
    else if ((pDSB = qobject_cast<QDoubleSpinBox *>(widget)))
    {
        const double value = settings[key].toDouble(&ok);
        if (ok)
        {
            pDSB->setValue(value);
            return true;
        }
    }
    else if ((pCB = qobject_cast<QCheckBox *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value != pCB->isChecked())
            pCB->setChecked(value);
        return true;
    }
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value)
            pRadioButton->setChecked(true);
        return true;
    }
    // ONLY FOR STRINGS, not INDEX
    else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
    {
        const QString value = settings[key].toString();
        pComboBox->setCurrentText(value);
        return true;
    }

    return false;
};

void Guide::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, &Guide::refreshOpticalTrain);
    connect(trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(opticalTrainCombo->currentText());
    });
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        if (guiderType == GUIDE_PHD2 && m_GuiderInstance->isConnected())
        {
            appendLogText(i18n("Cannot change active optical train while PHD2 is connected"));
            return;
        }

        ProfileSettings::Instance()->setOneSetting(ProfileSettings::GuideOpticalTrain,
                OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index)));
        refreshOpticalTrain();
        emit trainChanged();
    });
}

void Guide::refreshOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(true);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::GuideOpticalTrain);

    if (trainID.isValid())
    {
        auto id = trainID.toUInt();

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            qCWarning(KSTARS_EKOS_GUIDE) << "Optical train doesn't exist for id" << id;
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
        }

        auto name = OpticalTrainManager::Instance()->name(id);

        opticalTrainCombo->setCurrentText(name);

        auto scope = OpticalTrainManager::Instance()->getScope(name);
        m_FocalLength = scope["focal_length"].toDouble(-1);
        m_Aperture = scope["aperture"].toDouble(-1);
        m_FocalRatio = scope["focal_ratio"].toDouble(-1);
        m_Reducer = OpticalTrainManager::Instance()->getReducer(name);

        // DSLR Lens Aperture
        if (m_Aperture < 0 && m_FocalRatio > 0)
            m_Aperture = m_FocalLength / m_FocalRatio;

        auto mount = OpticalTrainManager::Instance()->getMount(name);
        setMount(mount);

        auto camera = OpticalTrainManager::Instance()->getCamera(name);
        if (camera)
        {
            if (guiderType == GUIDE_INTERNAL)
                starCenter = QVector3D();

            camera->setScopeInfo(m_FocalLength * m_Reducer, m_Aperture);
            opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(camera->getDeviceName(), scope["name"].toString()));
        }
        setCamera(camera);

        syncTelescopeInfo();

        auto guider = OpticalTrainManager::Instance()->getGuider(name);
        setGuider(guider);

        auto ao = OpticalTrainManager::Instance()->getAdaptiveOptics(name);
        setAdaptiveOptics(ao);

        // Load train settings
        OpticalTrainSettings::Instance()->setOpticalTrainID(id);
        auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Guide);
        if (settings.isValid())
        {
            auto map = settings.toJsonObject().toVariantMap();
            if (map != m_Settings)
            {
                m_Settings.clear();
                setAllSettings(map);
            }
        }
        else
            m_Settings = m_GlobalSettings;
    }

    opticalTrainCombo->blockSignals(false);
}

void Guide::loadGlobalSettings()
{
    QString key;
    QVariant value;

    QVariantMap settings;
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
    {
        if (oneWidget->objectName() == "opticalTrainCombo")
            continue;

        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid() && oneWidget->count() > 0)
        {
            oneWidget->setCurrentText(value.toString());
            settings[key] = value;
        }
    }

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toDouble());
            settings[key] = value;
        }
    }

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toInt());
            settings[key] = value;
        }
    }

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
    }

    m_GlobalSettings = m_Settings = settings;
}

void Guide::connectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Guide::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        connect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        connect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        connect(oneWidget, &QCheckBox::toggled, this, &Ekos::Guide::syncSettings);

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        connect(oneWidget, &QRadioButton::toggled, this, &Ekos::Guide::syncSettings);

    // Train combo box should NOT be synced.
    disconnect(opticalTrainCombo, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Guide::syncSettings);
}

void Guide::disconnectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Guide::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        disconnect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        disconnect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Ekos::Guide::syncSettings);

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        disconnect(oneWidget, &QRadioButton::toggled, this, &Ekos::Guide::syncSettings);

}

void Guide::updateSetting(const QString &key, const QVariant &value)
{
    // Save immediately
    Options::self()->setProperty(key.toLatin1(), value);
    m_Settings[key] = value;
    m_GlobalSettings[key] = value;

    m_DebounceTimer.start();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Guide::settleSettings()
{
    Options::self()->save();
    emit settingsUpdated(getAllSettings());
    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Guide, m_Settings);
}

void Guide::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QComboBox *cbox = nullptr;
    QRadioButton *rb = nullptr;

    QString key;
    QVariant value;

    if ( (dsb = qobject_cast<QDoubleSpinBox*>(sender())))
    {
        key = dsb->objectName();
        value = dsb->value();

    }
    else if ( (sb = qobject_cast<QSpinBox*>(sender())))
    {
        key = sb->objectName();
        value = sb->value();
    }
    else if ( (cb = qobject_cast<QCheckBox*>(sender())))
    {
        key = cb->objectName();
        value = cb->isChecked();
    }
    else if ( (cbox = qobject_cast<QComboBox*>(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }
    else if ( (rb = qobject_cast<QRadioButton*>(sender())))
    {
        key = rb->objectName();
        if (rb->isChecked() == false)
        {
            m_Settings.remove(key);
            return;
        }
        value = true;
    }

    updateSetting(key, value);
}


}
