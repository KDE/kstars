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
    // This needs to be very early. Many settings widgets are inside of opsGuide and references throughout this file.
    opsGuide = new OpsGuide();

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

    // #5 Load all settings
    loadSettings();

    // #6 Init Connections
    initConnections();

    // Image Filters
    for (auto &filter : FITSViewer::filterTypes)
        filterCombo->addItem(filter);
    filterCombo->setCurrentIndex(guideFilterIndex);

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

    // Init Internal Guider always
    internalGuider        = new InternalGuider();
    KConfigDialog *dialog = new KConfigDialog(this, "guidesettings", Options::self());
    opsCalibration        = new OpsCalibration(internalGuider);
    KPageWidgetItem *page = dialog->addPage(opsCalibration, i18n("Calibration"));
    page->setIcon(QIcon::fromTheme("tool-measure"));

    connect(opsGuide, &OpsGuide::settingsUpdated, [this]()
    {
        onThresholdChanged(Options::guideAlgorithm());
        configurePHD2Camera();
        configSEPMultistarOptions(); // due to changes in 'Guide Setting: Algorithm'
    });

    page = dialog->addPage(opsGuide, i18n("Guide"));
    page->setIcon(QIcon::fromTheme("kstars_guides"));

    opsDither = new OpsDither();
    page = dialog->addPage(opsDither, i18n("Dither"));
    page->setIcon(QIcon::fromTheme("transform-move"));

    opsGPG = new OpsGPG(internalGuider);
    page = dialog->addPage(opsGPG, i18n("GPG RA Guider"));
    page->setIcon(QIcon::fromTheme("pathshape"));

    internalGuider->setGuideView(guideView);

    // Set current guide type
    setGuiderType(-1);

    //This allows the current guideSubframe option to be loaded.
    if(guiderType == GUIDE_PHD2)
        setExternalGuiderBLOBEnabled(!Options::guideSubframeEnabled());

    // Initialize non guided dithering random generator.
    resetNonGuidedDither();

    //Note:  This is to prevent a button from being called the default button
    //and then executing when the user hits the enter key such as when on a Text Box
    QList<QPushButton *> qButtons = findChildren<QPushButton *>();
    for (auto &button : qButtons)
        button->setAutoDefault(false);

    connect(KStars::Instance(), &KStars::colorSchemeChanged, driftGraph, &GuideDriftGraph::refreshColorScheme);
}

Guide::~Guide()
{
    delete guider;
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
    guideView->setTrackingBoxEnabled(false);
    starCenter = QVector3D();

    if (Options::resetGuideCalibration())
        clearCalibration();

    // GPG guide algorithm should be reset on any slew.
    if (Options::gPGEnabled())
        guider->resetGPG();

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

void Guide::addCamera(ISD::GDInterface *newCCD)
{
    ISD::CCD *ccd = static_cast<ISD::CCD *>(newCCD);

    if (CCDs.contains(ccd))
        return;
    if(guiderType != GUIDE_INTERNAL)
    {
        connect(ccd, &ISD::CCD::newBLOBManager, [ccd, this](INDI::Property * prop)
        {
            if (prop->isNameMatch("CCD1") ||  prop->isNameMatch("CCD2"))
            {
                ccd->setBLOBEnabled(false); //This will disable PHD2 external guide frames until it is properly connected.
                currentCCD = ccd;
            }
        });
        guiderCombo->clear();
        guiderCombo->setEnabled(false);
        if (guiderType == GUIDE_PHD2)
            guiderCombo->addItem("PHD2");
        else
            guiderCombo->addItem("LinGuider");
    }
    else
    {
        guiderCombo->setEnabled(true);
        guiderCombo->addItem(ccd->getDeviceName());
    }

    CCDs.append(ccd);
    checkCCD();
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
    ISD::CCD *ccdMatch = nullptr;
    QString currentPHD2CameraName = "None";
    foreach(ISD::CCD *ccd, CCDs)
    {
        if(phd2Guider->getCurrentCamera().contains(ccd->getDeviceName()))
        {
            ccdMatch = ccd;
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
    if(currentCCD)
        setExternalGuiderBLOBEnabled(false);

    //Updating the currentCCD
    currentCCD = ccdMatch;

    //This updates the last camera name for the next time it is checked.
    lastPHD2CameraName = currentPHD2CameraName;

    //This sets a boolean that allows you to tell if the PHD2 camera is in Ekos
    phd2Guider->setCurrentCameraIsNotInEkos(currentCCD == nullptr);

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

void Guide::addGuideHead(ISD::GDInterface *newCCD)
{
    if (guiderType != GUIDE_INTERNAL)
        return;

    ISD::CCD *ccd = static_cast<ISD::CCD *>(newCCD);

    CCDs.append(ccd);

    QString guiderName = ccd->getDeviceName() + QString(" Guider");

    if (guiderCombo->findText(guiderName) == -1)
    {
        guiderCombo->addItem(guiderName);
        //CCDs.append(static_cast<ISD::CCD *> (newCCD));
    }

    //checkCCD(CCDs.count()-1);
    //guiderCombo->setCurrentIndex(CCDs.count()-1);

    //setGuiderProcess(Options::useEkosGuider() ? GUIDE_INTERNAL : GUIDE_PHD2);
}

void Guide::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = dynamic_cast<ISD::Telescope *>(newTelescope);

    syncTelescopeInfo();
}

bool Guide::setCamera(const QString &device)
{
    if (guiderType != GUIDE_INTERNAL)
        return true;

    for (int i = 0; i < guiderCombo->count(); i++)
        if (device == guiderCombo->itemText(i))
        {
            guiderCombo->setCurrentIndex(i);
            checkCCD(i);
            return true;
        }

    return false;
}

QString Guide::camera()
{
    if (currentCCD)
        return currentCCD->getDeviceName();

    return QString();
}

void Guide::checkCCD(int ccdNum)
{
    if (guiderType != GUIDE_INTERNAL)
        return;

    if (ccdNum == -1)
    {
        ccdNum = guiderCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum <= CCDs.count())
    {
        currentCCD = CCDs.at(ccdNum);

        if (currentCCD->hasGuideHead() && guiderCombo->currentText().contains("Guider"))
            useGuideHead = true;
        else
            useGuideHead = false;

        ISD::CCDChip *targetChip =
            currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip && targetChip->isCapturing())
            return;

        if (guiderType != GUIDE_INTERNAL)
        {
            syncCCDInfo();
            return;
        }

        // Make sure to disconnect all CCDs first from slots of Ekos::Guide
        for (const auto &oneCamera : CCDs)
            oneCamera->disconnect(this);

        connect(currentCCD, &ISD::CCD::numberUpdated, this, &Ekos::Guide::processCCDNumber, Qt::UniqueConnection);
        connect(currentCCD, &ISD::CCD::newExposureValue, this, &Ekos::Guide::checkExposureValue, Qt::UniqueConnection);

        targetChip->setImageView(guideView, FITS_GUIDE);

        syncCCDInfo();
    }
}

void Guide::syncCCDInfo()
{
    if (!currentCCD)
        return;

    auto nvp = currentCCD->getBaseDevice()->getNumber(useGuideHead ? "GUIDER_INFO" : "CCD_INFO");

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
    if (currentTelescope == nullptr || currentTelescope->isConnected() == false)
        return;

    auto nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {
        auto np = nvp->findWidgetByName("TELESCOPE_APERTURE");

        if (np && np->getValue() > 0)
            primaryAperture = np->getValue();

        np = nvp->findWidgetByName("GUIDER_APERTURE");
        if (np && np->getValue() > 0)
            guideAperture = np->getValue();

        aperture = primaryAperture;

        //if (currentCCD && currentCCD->getTelescopeType() == ISD::CCD::TELESCOPE_GUIDE)
        if (FOVScopeCombo->currentIndex() == ISD::CCD::TELESCOPE_GUIDE)
            aperture = guideAperture;

        np = nvp->findWidgetByName("TELESCOPE_FOCAL_LENGTH");
        if (np && np->getValue() > 0)
            primaryFL = np->getValue();

        np = nvp->findWidgetByName("GUIDER_FOCAL_LENGTH");
        if (np && np->getValue() > 0)
            guideFL = np->getValue();

        focal_length = primaryFL;

        //if (currentCCD && currentCCD->getTelescopeType() == ISD::CCD::TELESCOPE_GUIDE)
        if (FOVScopeCombo->currentIndex() == ISD::CCD::TELESCOPE_GUIDE)
            focal_length = guideFL;
    }

    updateGuideParams();
}

void Guide::updateGuideParams()
{
    if (currentCCD == nullptr)
        return;

    if (currentCCD->hasGuideHead() == false)
        useGuideHead = false;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

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

        binningCombo->blockSignals(true);

        binningCombo->clear();

        for (int i = 1; i <= maxBinX; i++)
            binningCombo->addItem(QString("%1x%2").arg(i).arg(i));

        if (useGuideHead)
            binningCombo->setCurrentIndex(subBinX - 1);
        else if (static_cast<int>(Options::guideBinSizeIndex() + 1) <= maxBinX)
        {
            subBinX = subBinY = Options::guideBinSizeIndex() + 1;
            binningCombo->setCurrentIndex(Options::guideBinSizeIndex());
        }
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
            ISD::CCD::TELESCOPE_PRIMARY,
            i18nc("F-Number, Focal length, Aperture",
                  "<nobr>F<b>%1</b> Focal length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                  QString::number(primaryFL / primaryAperture, 'f', 1), QString::number(primaryFL, 'f', 2),
                  QString::number(primaryAperture, 'f', 2)),
            Qt::ToolTipRole);
        FOVScopeCombo->setItemData(
            ISD::CCD::TELESCOPE_GUIDE,
            i18nc("F-Number, Focal length, Aperture",
                  "<nobr>F<b>%1</b> Focal length: <b>%2</b> mm Aperture: <b>%3</b> mm<sup>2</sup></nobr>",
                  QString::number(guideFL / guideAperture, 'f', 1), QString::number(guideFL, 'f', 2),
                  QString::number(guideAperture, 'f', 2)),
            Qt::ToolTipRole);

        guider->setGuiderParams(ccdPixelSizeX, ccdPixelSizeY, aperture, focal_length);
        emit guideChipUpdated(targetChip);

        int x, y, w, h;
        if (targetChip->getFrame(&x, &y, &w, &h))
        {
            guider->setFrameParams(x, y, w, h, subBinX, subBinY);
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

void Guide::addST4(ISD::ST4 *newST4)
{
    if (guiderType != GUIDE_INTERNAL)
        return;

    for (auto &guidePort : ST4List)
    {
        if (guidePort->getDeviceName() == newST4->getDeviceName())
            return;
    }

    ST4List.append(newST4);

    ST4Combo->addItem(newST4->getDeviceName());

    setST4(0);
}

bool Guide::setST4(const QString &device)
{
    if (guiderType != GUIDE_INTERNAL)
        return true;

    for (int i = 0; i < ST4List.count(); i++)
        if (ST4List.at(i)->getDeviceName() == device)
        {
            ST4Combo->setCurrentIndex(i);
            setST4(i);
            return true;
        }

    return false;
}

QString Guide::st4()
{
    if (guiderType != GUIDE_INTERNAL || ST4Combo->currentIndex() == -1)
        return QString();

    return ST4Combo->currentText();
}

void Guide::setST4(int index)
{
    if (ST4List.empty() || index >= ST4List.count() || guiderType != GUIDE_INTERNAL)
        return;

    ST4Driver = ST4List.at(index);

    GuideDriver = ST4Driver;
}

bool Guide::capture()
{
    buildOperationStack(GUIDE_CAPTURE);

    return executeOperationStack();
}

bool Guide::captureOneFrame()
{
    captureTimeout.stop();

    if (currentCCD == nullptr)
        return false;

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: lost connection to CCD."));
        return false;
    }

    // If CCD Telescope Type does not match desired scope type, change it
    if (currentCCD->getTelescopeType() != FOVScopeCombo->currentIndex())
        currentCCD->setTelescopeType(static_cast<ISD::CCD::TelescopeType>(FOVScopeCombo->currentIndex()));

    double seqExpose = exposureIN->value();

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    prepareCapture(targetChip);

    guideView->setBaseSize(guideWidget->size());
    setBusy(true);

    // Check if we have a valid frame setting
    if (frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];
        targetChip->setFrame(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(),
                             settings["h"].toInt());
        targetChip->setBinning(settings["binx"].toInt(), settings["biny"].toInt());
    }

    connect(currentCCD, &ISD::CCD::newImage, this, &Ekos::Guide::processData, Qt::UniqueConnection);
    qCDebug(KSTARS_EKOS_GUIDE) << "Capturing frame...";

    double finalExposure = seqExpose;

    // Increase exposure for calibration frame if we need auto-select a star
    // To increase chances we detect one.
    if (operationStack.contains(GUIDE_STAR_SELECT) && Options::guideAutoStarEnabled() &&
            !((guiderType == GUIDE_INTERNAL) && internalGuider->SEPMultiStarEnabled()))
        finalExposure *= 3;

    // Timeout is exposure duration + timeout threshold in seconds
    captureTimeout.start(finalExposure * 1000 + CAPTURE_TIMEOUT_THRESHOLD);

    targetChip->capture(finalExposure);

    return true;
}

void Guide::prepareCapture(ISD::CCDChip *targetChip)
{
    targetChip->setBatchMode(false);
    targetChip->setCaptureMode(FITS_GUIDE);
    targetChip->setFrameType(FRAME_LIGHT);
    if (darkFrameCheck->isChecked())
        targetChip->setCaptureFilter(FITS_NONE);
    else
        targetChip->setCaptureFilter(static_cast<FITSScale>(filterCombo->currentIndex()));
    currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);
}

bool Guide::abort()
{
    if (currentCCD && guiderType == GUIDE_INTERNAL)
    {
        captureTimeout.stop();
        pulseTimer.stop();
        ISD::CCDChip *targetChip =
            currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip->isCapturing())
            targetChip->abortExposure();
    }

    manualDitherB->setEnabled(false);

    setBusy(false);

    switch (state)
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
            guider->abort();
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
            autoStarCheck->setEnabled(true);
            if(currentCCD)
                subFrameCheck->setEnabled(true);
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

        connect(guideView, &FITSView::trackingStarSelected, this, &Ekos::Guide::setTrackingStar, Qt::UniqueConnection);
    }
}

void Guide::processCaptureTimeout()
{
    auto restartExposure = [&]()
    {
        appendLogText(i18n("Exposure timeout. Restarting exposure..."));
        currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);
        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        targetChip->abortExposure();
        prepareCapture(targetChip);
        targetChip->capture(exposureIN->value());
        captureTimeout.start(exposureIN->value() * 1000 + CAPTURE_TIMEOUT_THRESHOLD);
    };

    m_CaptureTimeoutCounter++;

    if (m_DeviceRestartCounter >= 3)
    {
        m_CaptureTimeoutCounter = 0;
        m_DeviceRestartCounter = 0;
        if (state == GUIDE_GUIDING)
            appendLogText(i18n("Exposure timeout. Aborting Autoguide."));
        else if (state == GUIDE_DITHERING)
            appendLogText(i18n("Exposure timeout. Aborting Dithering."));
        else if (state == GUIDE_CALIBRATING)
            appendLogText(i18n("Exposure timeout. Aborting Calibration."));

        captureTimeout.stop();
        abort();
        return;
    }

    if (m_CaptureTimeoutCounter > 1)
    {
        QString camera = currentCCD->getDeviceName();
        QString via = ST4Driver ? ST4Driver->getDeviceName() : "";
        emit driverTimedout(camera);
        QTimer::singleShot(5000, [ &, camera, via]()
        {
            m_DeviceRestartCounter++;
            reconnectDriver(camera, via);
        });
        return;
    }

    else
        restartExposure();
}

void Guide::reconnectDriver(const QString &camera, const QString &via)
{
    for (auto &oneCamera : CCDs)
    {
        if (oneCamera->getDeviceName() == camera)
        {
            // Set camera again to the one we restarted
            guiderCombo->setCurrentIndex(guiderCombo->findText(camera));
            ST4Combo->setCurrentIndex(ST4Combo->findText(via));
            checkCCD();

            if (guiderType == GUIDE_INTERNAL)
            {
                // restart capture
                m_CaptureTimeoutCounter = 0;
                captureOneFrame();
            }

            return;
        }
    }

    QTimer::singleShot(5000, this, [ &, camera, via]()
    {
        reconnectDriver(camera, via);
    });
}

void Guide::processData(const QSharedPointer<FITSData> &data)
{
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
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
        m_ImageData = data;
    else
        m_ImageData.reset();

    if (guiderType == GUIDE_INTERNAL)
        internalGuider->setImageData(m_ImageData);

    captureTimeout.stop();
    m_CaptureTimeoutCounter = 0;

    disconnect(currentCCD, &ISD::CCD::newImage, this, &Ekos::Guide::processData);

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
    if (guideView != nullptr)
        guideView->clearNeighbors();

    DarkLibrary::Instance()->disconnect(this);

    if (operationStack.isEmpty() == false)
    {
        executeOperationStack();
        return;
    }

    switch (state)
    {
        case GUIDE_IDLE:
        case GUIDE_ABORTED:
        case GUIDE_CONNECTED:
        case GUIDE_DISCONNECTED:
        case GUIDE_CALIBRATION_SUCESS:
        case GUIDE_CALIBRATION_ERROR:
        case GUIDE_DITHERING_ERROR:
            setBusy(false);
            break;

        case GUIDE_CAPTURE:
            state = GUIDE_IDLE;
            emit newStatus(state);
            setBusy(false);
            break;

        case GUIDE_LOOPING:
            capture();
            break;

        case GUIDE_CALIBRATING:
            guider->calibrate();
            break;

        case GUIDE_GUIDING:
            guider->guide();
            break;

        case GUIDE_DITHERING:
            guider->dither(Options::ditherPixels());
            break;

        // Feature only of internal guider
        case GUIDE_MANUAL_DITHERING:
            dynamic_cast<InternalGuider*>(guider)->processManualDithering();
            break;

        case GUIDE_REACQUIRE:
            guider->reacquire();
            break;

        case GUIDE_DITHERING_SETTLE:
            if (Options::ditherNoGuiding())
                return;
            capture();
            break;

        case GUIDE_SUSPENDED:
            if (Options::gPGEnabled())
                guider->guide();
            break;

        default:
            break;
    }

    emit newImage(guideView);
    emit newStarPixmap(guideView->getTrackingBoxPixmap(10));
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
    if (ST4Driver == nullptr || guider == nullptr)
        return;

    if (guiderType == GUIDE_INTERNAL)
    {
        dynamic_cast<InternalGuider *>(guider)->setDECSwap(enable);
        ST4Driver->setDECSwap(enable);
    }
}

bool Guide::sendPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs)
{
    if (GuideDriver == nullptr || (ra_dir == NO_DIR && dec_dir == NO_DIR))
        return false;

    // If we're calibrating and we send a pulse, then schedule a subsequent capture.
    if (state == GUIDE_CALIBRATING)
        pulseTimer.start((ra_msecs > dec_msecs ? ra_msecs : dec_msecs) + 100);

    return GuideDriver->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
}

bool Guide::sendPulse(GuideDirection dir, int msecs)
{
    if (GuideDriver == nullptr || dir == NO_DIR)
        return false;

    if (state == GUIDE_CALIBRATING)
        pulseTimer.start(msecs + 100);

    return GuideDriver->doPulse(dir, msecs);
}

QStringList Guide::getST4Devices()
{
    QStringList devices;

    foreach (ISD::ST4 *driver, ST4List)
        devices << driver->getDeviceName();

    return devices;
}


bool Guide::calibrate()
{
    // Set status to idle and let the operations change it as they get executed
    state = GUIDE_IDLE;
    emit newStatus(state);

    if (guiderType == GUIDE_INTERNAL)
    {
        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

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

    qCDebug(KSTARS_EKOS_GUIDE) << "Starting calibration using CCD:" << currentCCD->getDeviceName() << "via" <<
                               ST4Combo->currentText();

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
        guider->guide();

        //If PHD2 gets a Guide command and it is looping, it will accept a lock position
        //but if it was not looping it will ignore the lock position and do an auto star automatically
        //This is not the default behavior in Ekos if auto star is not selected.
        //This gets around that by noting the position of the tracking box, and enforcing it after the state switches to guide.
        if(!Options::guideAutoStarEnabled())
        {
            if(guiderType == GUIDE_PHD2 && guideView->isTrackingBoxEnabled())
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

    if (Options::defaultCaptureCCD() == guiderCombo->currentText())
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, executeGuide]()
        {
            //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
            KSMessageBox::Instance()->disconnect(this);
            executeGuide();
        });

        KSMessageBox::Instance()->questionYesNo(
            i18n("The guide camera is identical to the primary imaging camera. Are you sure you want to continue?"));

        return false;
    }

    if (m_MountStatus == ISD::Telescope::MOUNT_PARKED)
    {
        KSMessageBox::Instance()->sorry(i18n("The mount is parked. Unpark to start guiding."));
        return false;
    }

    executeGuide();
    return true;
}

bool Guide::dither()
{
    if (Options::ditherNoGuiding() && state == GUIDE_IDLE)
    {
        nonGuidedDither();
        return true;
    }

    if (state == GUIDE_DITHERING || state == GUIDE_DITHERING_SETTLE)
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
        if (state != GUIDE_GUIDING)
            capture();

        setStatus(GUIDE_DITHERING);

        return true;
    }
    else
        return guider->dither(Options::ditherPixels());
}

bool Guide::suspend()
{
    if (state == GUIDE_SUSPENDED)
        return true;
    else if (state >= GUIDE_CAPTURE)
        return guider->suspend();
    else
        return false;
}

bool Guide::resume()
{
    if (state == GUIDE_GUIDING)
        return true;
    else if (state == GUIDE_SUSPENDED)
        return guider->resume();
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

void Guide::setPierSide(ISD::Telescope::PierSide newSide)
{
    guider->setPierSide(newSide);

    // If pier side changes in internal guider
    // and calibration was already done
    // then let's swap
    if (guiderType == GUIDE_INTERNAL &&
            state != GUIDE_GUIDING &&
            state != GUIDE_CALIBRATING &&
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

void Guide::setMountStatus(ISD::Telescope::Status newState)
{
    m_MountStatus = newState;

    if (newState == ISD::Telescope::MOUNT_PARKING || newState == ISD::Telescope::MOUNT_SLEWING)
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
            guider->resetGPG();

        // If we're guiding, and the mount either slews or parks, then we abort.
        if (state == GUIDE_GUIDING || state == GUIDE_DITHERING)
        {
            if (newState == ISD::Telescope::MOUNT_PARKING)
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
        case ISD::Telescope::MOUNT_SLEWING:
        case ISD::Telescope::MOUNT_PARKING:
        case ISD::Telescope::MOUNT_MOVING:
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

void Guide::setMountCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt, int pierSide,
                           const QString &ha)
{
    Q_UNUSED(ha);
    guider->setMountCoords(ra, dec, az, alt, pierSide);
}

void Guide::setExposure(double value)
{
    exposureIN->setValue(value);
}

void Guide::setImageFilter(const QString &value)
{
    for (int i = 0; i < filterCombo->count(); i++)
        if (filterCombo->itemText(i) == value)
        {
            filterCombo->setCurrentIndex(i);
            break;
        }
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

    guider->clearCalibration();

    appendLogText(i18n("Calibration is cleared."));
}

void Guide::setStatus(Ekos::GuideState newState)
{
    if (newState == state)
    {
        // pass through the aborted state
        if (newState == GUIDE_ABORTED)
            emit newStatus(state);
        return;
    }

    GuideState previousState = state;

    state = newState;
    emit newStatus(state);

    switch (state)
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

        case GUIDE_CALIBRATION_SUCESS:
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
                guideTimer = QTime::currentTime();
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
                state = GUIDE_ABORTED;
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
    if (currentCCD == nullptr || guiderType != GUIDE_INTERNAL)
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    targetChip->setBinning(index + 1, index + 1);

    QVariantMap settings      = frameSettings[targetChip];
    settings["binx"]          = index + 1;
    settings["biny"]          = index + 1;
    frameSettings[targetChip] = settings;

    guider->setFrameParams(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(), settings["h"].toInt(),
                           settings["binx"].toInt(), settings["biny"].toInt());

    saveSettings();
}

void Guide::processCCDNumber(INumberVectorProperty *nvp)
{
    if (currentCCD == nullptr || (nvp->device != currentCCD->getDeviceName()) || guiderType != GUIDE_INTERNAL)
        return;

    if ((!strcmp(nvp->name, "CCD_BINNING") && useGuideHead == false) ||
            (!strcmp(nvp->name, "GUIDER_BINNING") && useGuideHead))
    {
        binningCombo->disconnect();
        binningCombo->setCurrentIndex(nvp->np[0].value - 1);
        connect(binningCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this, &Ekos::Guide::updateCCDBin);
    }
}

void Guide::checkExposureValue(ISD::CCDChip *targetChip, double exposure, IPState expState)
{
    // Ignore if not using internal guider, or chip belongs to a different camera.
    if (guiderType != GUIDE_INTERNAL || targetChip->getCCD() != currentCCD)
        return;

    INDI_UNUSED(exposure);

    if (expState == IPS_ALERT &&
            ((state == GUIDE_GUIDING) || (state == GUIDE_DITHERING) || (state == GUIDE_CALIBRATING)))
    {
        appendLogText(i18n("Exposure failed. Restarting exposure..."));
        currentCCD->setTransformFormat(ISD::CCD::FORMAT_FITS);
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
    if(!currentCCD || guiderType == GUIDE_LINGUIDER)
        return;

    if(guiderType == GUIDE_PHD2)
    {
        //This way it won't set the tracking box on the Guide Star Image.
        if(!m_ImageData.isNull())
        {
            if(m_ImageData->width() < 50)
            {
                guideView->setTrackingBoxEnabled(false);
                return;
            }
        }
    }

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
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
        guideView->setTrackingBoxEnabled(true);
        guideView->setTrackingBox(starRect);
    }
}

bool Guide::setGuiderType(int type)
{
    // Use default guider option
    if (type == -1)
        type = Options::guiderType();
    else if (type == guiderType)
        return true;

    if (state == GUIDE_CALIBRATING || state == GUIDE_GUIDING || state == GUIDE_DITHERING)
    {
        appendLogText(i18n("Cannot change guider type while active."));
        return false;
    }

    if (guider != nullptr)
    {
        // Disconnect from host
        if (guider->isConnected())
            guider->Disconnect();

        // Disconnect signals
        guider->disconnect();
    }

    guiderType = static_cast<GuiderType>(type);

    switch (type)
    {
        case GUIDE_INTERNAL:
        {
            connect(internalGuider, SIGNAL(newPulse(GuideDirection, int)), this, SLOT(sendPulse(GuideDirection, int)));
            connect(internalGuider, SIGNAL(newPulse(GuideDirection, int, GuideDirection, int)), this,
                    SLOT(sendPulse(GuideDirection, int, GuideDirection, int)));
            connect(internalGuider, SIGNAL(DESwapChanged(bool)), swapCheck, SLOT(setChecked(bool)));
            connect(internalGuider, SIGNAL(newStarPixmap(QPixmap &)), this, SIGNAL(newStarPixmap(QPixmap &)));

            guider = internalGuider;

            internalGuider->setSquareAlgorithm(opsGuide->kcfg_GuideAlgorithm->currentIndex());

            clearCalibrationB->setEnabled(true);
            guideB->setEnabled(true);
            captureB->setEnabled(true);
            loopB->setEnabled(true);

            configSEPMultistarOptions();
            darkFrameCheck->setEnabled(true);

            guiderCombo->setEnabled(true);
            ST4Combo->setEnabled(true);
            exposureIN->setEnabled(true);
            binningCombo->setEnabled(true);
            boxSizeCombo->setEnabled(true);
            filterCombo->setEnabled(true);

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

            guiderCombo->setToolTip(i18n("Select guide camera."));

            updateGuideParams();
        }
        break;

        case GUIDE_PHD2:
            if (phd2Guider.isNull())
                phd2Guider = new PHD2();

            guider = phd2Guider;
            phd2Guider->setGuideView(guideView);

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

            ST4Combo->setEnabled(false);
            exposureIN->setEnabled(true);
            binningCombo->setEnabled(false);
            boxSizeCombo->setEnabled(false);
            filterCombo->setEnabled(false);
            guiderCombo->setEnabled(false);

            if (Options::resetGuideCalibration())
                appendLogText(i18n("Warning: Reset Guiding Calibration is enabled. It is recommended to turn this option off for PHD2."));

            updateGuideParams();
            break;

        case GUIDE_LINGUIDER:
            if (linGuider.isNull())
                linGuider = new LinGuider();

            guider = linGuider;

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

            ST4Combo->setEnabled(false);
            exposureIN->setEnabled(false);
            binningCombo->setEnabled(false);
            boxSizeCombo->setEnabled(false);
            filterCombo->setEnabled(false);

            guiderCombo->setEnabled(false);

            updateGuideParams();

            break;
    }

    if (guider != nullptr)
    {
        connect(guider, &Ekos::GuideInterface::frameCaptureRequested, this, &Ekos::Guide::capture);
        connect(guider, &Ekos::GuideInterface::newLog, this, &Ekos::Guide::appendLogText);
        connect(guider, &Ekos::GuideInterface::newStatus, this, &Ekos::Guide::setStatus);
        connect(guider, &Ekos::GuideInterface::newStarPosition, this, &Ekos::Guide::setStarPosition);
        connect(guider, &Ekos::GuideInterface::guideStats, this, &Ekos::Guide::guideStats);

        connect(guider, &Ekos::GuideInterface::newAxisDelta, this, &Ekos::Guide::setAxisDelta);
        connect(guider, &Ekos::GuideInterface::newAxisPulse, this, &Ekos::Guide::setAxisPulse);
        connect(guider, &Ekos::GuideInterface::newAxisSigma, this, &Ekos::Guide::setAxisSigma);
        connect(guider, &Ekos::GuideInterface::newSNR, this, &Ekos::Guide::setSNR);

        driftGraph->connectGuider(guider);
        targetPlot->connectGuider(guider);

        connect(guider, &Ekos::GuideInterface::calibrationUpdate, this, &Ekos::Guide::calibrationUpdate);

        connect(guider, &Ekos::GuideInterface::guideEquipmentUpdated, this, &Ekos::Guide::configurePHD2Camera);
    }

    externalConnectB->setEnabled(false);
    externalDisconnectB->setEnabled(false);

    if (guider != nullptr && guiderType != GUIDE_INTERNAL)
    {
        externalConnectB->setEnabled(!guider->isConnected());
        externalDisconnectB->setEnabled(guider->isConnected());
    }

    if (guider != nullptr)
        guider->Connect();

    return true;
}

void Guide::updateTrackingBoxSize(int currentIndex)
{
    if (currentIndex >= 0)
    {
        Options::setGuideSquareSizeIndex(currentIndex);

        if (guiderType == GUIDE_INTERNAL)
            dynamic_cast<InternalGuider *>(guider)->setGuideBoxSize(boxSizeCombo->currentText().toInt());

        syncTrackingBoxPosition();
    }
}

void Guide::onThresholdChanged(int index)
{
    switch (guiderType)
    {
        case GUIDE_INTERNAL:
            dynamic_cast<InternalGuider *>(guider)->setSquareAlgorithm(index);
            break;

        default:
            break;
    }
}

void Guide::onEnableDirRA(bool enable)
{
    // If RA guiding is enable or disabled, the GPG should be reset.
    if (Options::gPGEnabled())
        guider->resetGPG();
    Options::setRAGuideEnabled(enable);
}

void Guide::onEnableDirDEC(bool enable)
{
    Options::setDECGuideEnabled(enable);
    updatePHD2Directions();
}

void Guide::syncSettings()
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;

    QObject *obj = sender();

    if ((pSB = qobject_cast<QSpinBox *>(obj)))
    {
        if (pSB == opsGuide->spinBox_MaxPulseArcSecRA)
            Options::setRAMaximumPulseArcSec(pSB->value());
        else if (pSB == opsGuide->spinBox_MaxPulseArcSecDEC)
            Options::setDECMaximumPulseArcSec(pSB->value());
    }
    else if ((pDSB = qobject_cast<QDoubleSpinBox *>(obj)))
    {
        if (pDSB == opsGuide->spinBox_PropGainRA)
            Options::setRAProportionalGain(pDSB->value());
        else if (pDSB == opsGuide->spinBox_PropGainDEC)
            Options::setDECProportionalGain(pDSB->value());
        else if (pDSB == opsGuide->spinBox_IntGainRA)
            Options::setRAIntegralGain(pDSB->value());
        else if (pDSB == opsGuide->spinBox_IntGainDEC)
            Options::setDECIntegralGain(pDSB->value());
        else if (pDSB == opsGuide->spinBox_MinPulseArcSecRA)
            Options::setRAMinimumPulseArcSec(pDSB->value());
        else if (pDSB == opsGuide->spinBox_MinPulseArcSecDEC)
            Options::setDECMinimumPulseArcSec(pDSB->value());
    }
    else if ((pCB = qobject_cast<QCheckBox*>(obj)))
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
    // Exposure
    exposureIN->setValue(Options::guideExposure());
    // Bin Size
    binningCombo->setCurrentIndex(Options::guideBinSizeIndex());
    // Box Size
    boxSizeCombo->setCurrentIndex(Options::guideSquareSizeIndex());
    // Effect filter
    guideFilterIndex = Options::guideFilterFITSIndex();
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

    // Transition code: if old values are stored in the proportional gains,
    // change them to a default value.
    if (Options::rAProportionalGain() > 1.0)
        Options::setRAProportionalGain(0.75);
    if (Options::dECProportionalGain() > 1.0)
        Options::setDECProportionalGain(0.75);

    // PID Control - Proportional Gain
    opsGuide->spinBox_PropGainRA->setValue(Options::rAProportionalGain());
    opsGuide->spinBox_PropGainDEC->setValue(Options::dECProportionalGain());

    // Transition code: if old values are stored in the integral gains,
    // change them to a default value.
    if (Options::rAIntegralGain() > 1.0)
        Options::setRAIntegralGain(0.75);
    if (Options::dECIntegralGain() > 1.0)
        Options::setDECIntegralGain(0.75);

    // PID Control - Integral Gain
    opsGuide->spinBox_IntGainRA->setValue(Options::rAIntegralGain());
    opsGuide->spinBox_IntGainDEC->setValue(Options::dECIntegralGain());
    // Max Pulse Duration (arcsec)
    opsGuide->spinBox_MaxPulseArcSecRA->setValue(Options::rAMaximumPulseArcSec());
    opsGuide->spinBox_MaxPulseArcSecDEC->setValue(Options::dECMaximumPulseArcSec());
    // Min Pulse Duration (arcsec)
    opsGuide->spinBox_MinPulseArcSecRA->setValue(Options::rAMinimumPulseArcSec());
    opsGuide->spinBox_MinPulseArcSecDEC->setValue(Options::dECMinimumPulseArcSec());
    // Autostar
    autoStarCheck->setChecked(Options::guideAutoStarEnabled());
}

void Guide::saveSettings()
{
    // Exposure
    Options::setGuideExposure(exposureIN->value());
    // Bin Size
    Options::setGuideBinSizeIndex(binningCombo->currentIndex());
    // Box Size
    Options::setGuideSquareSizeIndex(boxSizeCombo->currentIndex());
    // Effect filter
    Options::setGuideFilterFITSIndex(filterCombo->currentIndex());
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
    // PID Control - Proportional Gain
    Options::setRAProportionalGain(opsGuide->spinBox_PropGainRA->value());
    Options::setDECProportionalGain(opsGuide->spinBox_PropGainDEC->value());
    // PID Control - Integral Gain
    Options::setRAIntegralGain(opsGuide->spinBox_IntGainRA->value());
    Options::setDECIntegralGain(opsGuide->spinBox_IntGainDEC->value());
    // Max Pulse Duration (arcsec)
    Options::setRAMaximumPulseArcSec(opsGuide->spinBox_MaxPulseArcSecRA->value());
    Options::setDECMaximumPulseArcSec(opsGuide->spinBox_MaxPulseArcSecDEC->value());
    // Min Pulse Duration (arcsec)
    Options::setRAMinimumPulseArcSec(opsGuide->spinBox_MinPulseArcSecRA->value());
    Options::setDECMinimumPulseArcSec(opsGuide->spinBox_MinPulseArcSecDEC->value());
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
                guider->setStarPosition(starCenter);

                // Tracking must be engaged
                if (currentTelescope && currentTelescope->canControlTrack() && currentTelescope->isTracking() == false)
                    currentTelescope->setTrackEnabled(true);
            }

            if (guider->calibrate())
            {
                if (guiderType == GUIDE_INTERNAL)
                    disconnect(guideView, SIGNAL(trackingStarSelected(int, int)), this,
                               SLOT(setTrackingStar(int, int)));
                setBusy(true);
            }
            else
            {
                emit newStatus(GUIDE_CALIBRATION_ERROR);
                state = GUIDE_IDLE;
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

    if (currentCCD == nullptr)
        return actionRequired;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
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
                      state == GUIDE_REACQUIRE))
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

                connect(DarkLibrary::Instance(), &DarkLibrary::darkFrameCompleted, this, [&](bool completed)
                {
                    DarkLibrary::Instance()->disconnect(this);
                    if (completed != darkFrameCheck->isChecked())
                        setDarkFrameEnabled(completed);
                    if (completed)
                    {
                        guideView->rescale(ZOOM_KEEP_LEVEL);
                        guideView->updateFrame();
                    }
                    setCaptureComplete();
                });
                connect(DarkLibrary::Instance(), &DarkLibrary::newLog, this, &Ekos::Guide::appendLogText);
                actionRequired = true;
                targetChip->setCaptureFilter(static_cast<FITSScale>(filterCombo->currentIndex()));
                DarkLibrary::Instance()->denoise(targetChip, m_ImageData, exposureIN->value(), targetChip->getCaptureFilter(),
                                                 offsetX, offsetY);
            }
        }
        break;

        case GUIDE_STAR_SELECT:
        {
            state = GUIDE_STAR_SELECT;
            emit newStatus(state);

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
                    state = GUIDE_CALIBRATION_ERROR;
                    emit newStatus(state);
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

    if(!currentCCD)
        return;

    currentCCD->setBLOBEnabled(enable);

    if(currentCCD->isBLOBEnabled())
    {
        if (currentCCD->hasGuideHead() && guiderCombo->currentText().contains("Guider"))
            useGuideHead = true;
        else
            useGuideHead = false;

        ISD::CCDChip *targetChip =
            currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip)
        {
            targetChip->setImageView(guideView, FITS_GUIDE);
            targetChip->setCaptureMode(FITS_GUIDE);
        }
        syncCCDInfo();
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

    bool rc = sendPulse(raPolarity > 0 ? RA_INC_DIR : RA_DEC_DIR, raMsec, decPolarity > 0 ? DEC_INC_DIR : DEC_DEC_DIR,
                        decMsec);

    if (rc)
    {
        qCInfo(KSTARS_EKOS_GUIDE) << "Non-guiding dither successful.";
        QTimer::singleShot( (raMsec > decMsec ? raMsec : decMsec) + Options::ditherSettle() * 1000 + 100, [this]()
        {
            emit newStatus(GUIDE_DITHERING_SUCCESS);
            state = GUIDE_IDLE;
        });
    }
    else
    {
        qCWarning(KSTARS_EKOS_GUIDE) << "Non-guiding dither failed.";
        emit newStatus(GUIDE_DITHERING_ERROR);
        state = GUIDE_IDLE;
    }
}

void Guide::updateTelescopeType(int index)
{
    if (currentCCD == nullptr)
        return;

    focal_length = (index == ISD::CCD::TELESCOPE_PRIMARY) ? primaryFL : guideFL;
    aperture = (index == ISD::CCD::TELESCOPE_PRIMARY) ? primaryAperture : guideAperture;

    Options::setGuideScopeType(index);

    syncTelescopeInfo();
}

void Guide::setDefaultST4(const QString &driver)
{
    Options::setDefaultST4Driver(driver);
}

void Guide::setDefaultCCD(const QString &ccd)
{
    if (guiderType == GUIDE_INTERNAL)
        Options::setDefaultGuideCCD(ccd);
}

void Guide::handleManualDither()
{
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
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
            guider->dither(ditherDialog.magnitude->value());
        else
        {
            InternalGuider * const ig = dynamic_cast<InternalGuider *>(guider);
            if (ig)
                ig->ditherXY(ditherDialog.x->value(), ditherDialog.y->value());
        }
    }
}

bool Guide::connectGuider()
{
    return guider->Connect();
}

bool Guide::disconnectGuider()
{
    return guider->Disconnect();
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

    guideView = new GuideView(guideWidget, FITS_GUIDE);
    guideView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    guideView->setBaseSize(guideWidget->size());
    guideView->createFloatingToolBar();
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(guideView);
    guideWidget->setLayout(vlayout);
    connect(guideView, &FITSView::trackingStarSelected, this, &Ekos::Guide::setTrackingStar);
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
    connect(guiderCombo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated), this,
            &Ekos::Guide::setDefaultCCD);
    connect(guiderCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this,
            [&](int index)
    {
        if (guiderType == GUIDE_INTERNAL)
        {
            starCenter = QVector3D();
            checkCCD(index);
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
    connect(ST4Combo, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated), [&](const QString & text)
    {
        setDefaultST4(text);
        setST4(text);
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

    // PID Control - Proportional Gain
    connect(opsGuide->spinBox_PropGainRA, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);
    connect(opsGuide->spinBox_PropGainDEC, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // PID Control - Integral Gain
    connect(opsGuide->spinBox_IntGainRA, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);
    connect(opsGuide->spinBox_IntGainDEC, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // Max Pulse Duration (ms)
    connect(opsGuide->spinBox_MaxPulseArcSecRA, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);
    connect(opsGuide->spinBox_MaxPulseArcSecDEC, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // Min Pulse Duration (ms)
    connect(opsGuide->spinBox_MinPulseArcSecRA, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);
    connect(opsGuide->spinBox_MinPulseArcSecDEC, &QSpinBox::editingFinished, this, &Ekos::Guide::syncSettings);

    // Capture
    connect(captureB, &QPushButton::clicked, this, [this]()
    {
        state = GUIDE_CAPTURE;
        emit newStatus(state);

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
        guider->Connect();
    });
    connect(externalDisconnectB, &QPushButton::clicked, this, [&]()
    {
        //setExternalGuiderBLOBEnabled(true);
        guider->Disconnect();
    });

    // Pulse Timer
    pulseTimer.setSingleShot(true);
    connect(&pulseTimer, &QTimer::timeout, this, &Ekos::Guide::capture);

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

void Guide::removeDevice(ISD::GDInterface *device)
{
    device->disconnect(this);
    if (currentTelescope && (currentTelescope->getDeviceName() == device->getDeviceName()))
    {
        currentTelescope = nullptr;
    }
    else if (CCDs.contains(static_cast<ISD::CCD *>(device)))
    {
        CCDs.removeAll(static_cast<ISD::CCD *>(device));
        guiderCombo->removeItem(guiderCombo->findText(device->getDeviceName()));
        guiderCombo->removeItem(guiderCombo->findText(device->getDeviceName() + QString(" Guider")));
        if (CCDs.empty())
        {
            currentCCD = nullptr;
            guiderCombo->setCurrentIndex(-1);
        }
        else
        {
            currentCCD = CCDs[0];
            guiderCombo->setCurrentIndex(0);
        }

        QTimer::singleShot(1000, this, [this]()
        {
            checkCCD();
        });
    }

    auto st4 = std::find_if(ST4List.begin(), ST4List.end(), [device](ISD::ST4 * st)
    {
        return (st->getDeviceName() == device->getDeviceName());
    });

    if (st4 != ST4List.end())
    {
        ST4List.removeOne(*st4);

        ST4Combo->removeItem(ST4Combo->findText(device->getDeviceName()));
        if (ST4List.empty())
        {
            ST4Driver = GuideDriver = nullptr;
        }
        else
        {
            setST4(ST4Combo->currentText());
        }
    }

}

QJsonObject Guide::getSettings() const
{
    QJsonObject settings;

    settings.insert("camera", guiderCombo->currentText());
    settings.insert("via", ST4Combo->currentText());
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
    settings.insert("ra_gain", opsGuide->spinBox_PropGainRA->value());
    settings.insert("de_gain", opsGuide->spinBox_PropGainDEC->value());
    settings.insert("dither_enabled", Options::ditherEnabled());
    settings.insert("dither_pixels", Options::ditherPixels());
    settings.insert("dither_frequency", static_cast<int>(Options::ditherFrames()));
    settings.insert("gpGuideGraph::G_enabled", Options::gPGEnabled());

    return settings;
}

void Guide::setSettings(const QJsonObject &settings)
{
    static bool init = false;

    auto syncControl = [settings](const QString & key, QWidget * widget)
    {
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
    if (syncControl("camera", guiderCombo) || init == false)
        checkCCD();
    // Via
    syncControl("via", ST4Combo);
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
    syncControl("ra_gain", opsGuide->spinBox_PropGainRA);
    // DE Gain
    syncControl("de_gain", opsGuide->spinBox_PropGainDEC);
    // Options
    const bool ditherEnabled = settings["dither_enabled"].toBool(Options::ditherEnabled());
    Options::setDitherEnabled(ditherEnabled);
    const double ditherPixels = settings["dither_pixels"].toDouble(Options::ditherPixels());
    Options::setDitherPixels(ditherPixels);
    const int ditherFrequency = settings["dither_frequency"].toInt(Options::ditherFrames());
    Options::setDitherFrames(ditherFrequency);
    const bool gpg = settings["gpGuideGraph::G_enabled"].toBool(Options::gPGEnabled());
    Options::setGPGEnabled(gpg);

    init = true;
}

void Guide::loop()
{
    state = GUIDE_LOOPING;
    emit newStatus(state);

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
