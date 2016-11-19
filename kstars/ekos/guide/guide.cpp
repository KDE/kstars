/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guide.h"

#include <QDateTime>
#include <QSharedPointer>

#include <KMessageBox>
#include <KLed>
#include <KLocalizedString>
#include <KConfigDialog>

#include <basedevice.h>

#include "Options.h"
#include "opscalibration.h"
#include "opsguide.h"

#include "internalguide/internalguider.h"
#include "externalguide/phd2.h"
#include "externalguide/linguider.h"

#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/QProgressIndicator.h"

#include "indi/driverinfo.h"
#include "indi/clientmanager.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitsview.h"

#include "guideadaptor.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "auxiliary/qcustomplot.h"

#define driftGraph_WIDTH	200
#define driftGraph_HEIGHT	200
#define MAX_GUIDE_STARS 10

namespace Ekos
{

Guide::Guide() : QWidget()
{
    setupUi(this);

    new GuideAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Guide",  this);

    // Devices
    currentCCD = NULL;
    currentTelescope = NULL;
    guider = NULL;

    // AO Driver
    AODriver= NULL;

    // ST4 Driver
    GuideDriver=NULL;

    // Auto Star
    autoStarCaptured = false;

    // Subframe
    subFramed         = false;

    // To do calibrate + guide in one command
    autoCalibrateGuide = false;

    guideView = new FITSView(guideWidget, FITS_GUIDE);
    guideView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    guideView->setBaseSize(guideWidget->size());
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(guideView);
    guideWidget->setLayout(vlayout);
    connect(guideView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(setTrackingStar(int,int)));

    ccdPixelSizeX =  ccdPixelSizeY =  mountAperture =  mountFocalLength = pixScaleX = pixScaleY = -1;
    guideDeviationRA = guideDeviationDEC = 0;

    useGuideHead = false;
    //rapidGuideReticleSet = false;

    // Load all settings
    loadSettings();

    // Image Filters
    foreach(QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    // Progress Indicator
    pi = new QProgressIndicator(this);
    controlLayout->addWidget(pi, 0, 1, 1, 1);

    showFITSViewerB->setIcon(QIcon::fromTheme("kstars_fitsviewer", QIcon(":/icons/breeze/default/kstars_fitsviewer.svg")));
    connect(showFITSViewerB, SIGNAL(clicked()), this, SLOT(showFITSViewer()));

    // Exposure
    connect(exposureIN, SIGNAL(editingFinished()), this, SLOT(saveDefaultGuideExposure()));

    // Guiding Box Size
    connect(boxSizeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateTrackingBoxSize(int)));

    // Guider CCD Selection
    //connect(guiderCombo, SIGNAL(activated(QString)), this, SLOT(setDefaultCCD(QString)));
    connect(guiderCombo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this, [&](const QString & ccd){Options::setDefaultGuideCCD(ccd);});
    connect(guiderCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(checkCCD(int)));

    // Dark Frame Check
    connect(darkFrameCheck, SIGNAL(toggled(bool)), this, SLOT(setDarkFrameEnabled(bool)));

    // Subframe check
    connect(subFrameCheck, SIGNAL(toggled(bool)), this, SLOT(setSubFrameEnabled(bool)));

    // ST4 Selection
    connect(ST4Combo, SIGNAL(currentIndexChanged(int)), this, SLOT(setST4(int)));
    //connect(ST4Combo, SIGNAL(activated(QString)), this, SLOT(setDefaultST4(QString)));
    //connect(ST4Combo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this, [&](const QString & st4){Options::setDefaultST4Driver(st4);});

    // Binning Combo Selection
    connect(binningCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCCDBin(int)));

    // Guiding Rate - Advisory only
    connect( spinBox_GuideRate, 		SIGNAL(valueChanged(double)), this, SLOT(onInfoRateChanged(double)) );

    // RA/DEC Enable directions
    connect( checkBox_DirRA, SIGNAL(toggled(bool)), this, SLOT(onEnableDirRA(bool)) );
    connect( checkBox_DirDEC, SIGNAL(toggled(bool)), this, SLOT(onEnableDirDEC(bool)) );

    // N/W and W/E direction enable
    connect( northControlCheck, SIGNAL(toggled(bool)), this, SLOT(onControlDirectionChanged(bool)));
    connect( southControlCheck, SIGNAL(toggled(bool)), this, SLOT(onControlDirectionChanged(bool)));
    connect( westControlCheck, SIGNAL(toggled(bool)), this, SLOT(onControlDirectionChanged(bool)));
    connect( eastControlCheck, SIGNAL(toggled(bool)), this, SLOT(onControlDirectionChanged(bool)));

    // Declination Swap
    connect(swapCheck, SIGNAL(toggled(bool)), this, SLOT(setDECSwap(bool)));

    // PID Control - Propotional Gain
    connect( spinBox_PropGainRA, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
    connect( spinBox_PropGainDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );

    // PID Control - Integral Gain
    connect( spinBox_IntGainRA, 		SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
    connect( spinBox_IntGainDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );

    // PID Control - Derivative Gain
    connect( spinBox_DerGainRA, 		SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
    connect( spinBox_DerGainDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );

    // Max Pulse Duration (ms)
    connect( spinBox_MaxPulseRA, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
    connect( spinBox_MaxPulseDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );

    // Min Pulse Duration (ms)
    connect( spinBox_MinPulseRA, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
    connect( spinBox_MinPulseDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );

    // Capture
    connect(captureB, SIGNAL(clicked()), this, SLOT(capture()));

    // Stop
    connect(stopB, SIGNAL(clicked()), this, SLOT(abort()));

    // Calibrate
    connect(calibrateB, SIGNAL(clicked()), this, SLOT(calibrate()));

    // Guide
    connect(guideB, SIGNAL(clicked()), this, SLOT(guide()));

    // Connect External Guide
    connect(externalConnectB, &QPushButton::clicked, this, [&](){guider->Connect();});
    connect(externalDisconnectB, &QPushButton::clicked, this, [&](){guider->Disconnect();});

    // Pulse Timer
    pulseTimer.setSingleShot(true);
    connect(&pulseTimer, SIGNAL(timeout()), this, SLOT(capture()));

    // Drift Graph
    driftGraph->setBackground(QBrush(Qt::black));
    driftGraph->xAxis->setBasePen(QPen(Qt::white, 1));
    driftGraph->yAxis->setBasePen(QPen(Qt::white, 1));
    driftGraph->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    driftGraph->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    driftGraph->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    driftGraph->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    driftGraph->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    driftGraph->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    driftGraph->xAxis->setBasePen(QPen(Qt::white, 1));
    driftGraph->yAxis->setBasePen(QPen(Qt::white, 1));
    driftGraph->xAxis->setTickPen(QPen(Qt::white, 1));
    driftGraph->yAxis->setTickPen(QPen(Qt::white, 1));
    driftGraph->xAxis->setSubTickPen(QPen(Qt::white, 1));
    driftGraph->yAxis->setSubTickPen(QPen(Qt::white, 1));
    driftGraph->xAxis->setTickLabelColor(Qt::white);
    driftGraph->yAxis->setTickLabelColor(Qt::white);
    driftGraph->xAxis->setLabelColor(Qt::white);
    driftGraph->yAxis->setLabelColor(Qt::white);

    driftGraph->xAxis->setRange(0, 120, Qt::AlignRight);

    driftGraph->legend->setVisible(true);
    driftGraph->legend->setFont(QFont("Helvetica",9));
    driftGraph->legend->setTextColor(Qt::white);
    driftGraph->legend->setBrush(QBrush(Qt::black));

    // RA Curve
    driftGraph->addGraph();
    driftGraph->graph(0)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError")));
    driftGraph->graph(0)->setName("RA");
    driftGraph->graph(0)->setLineStyle(QCPGraph::lsStepLeft);

    // DE Curve
    driftGraph->addGraph();
    driftGraph->graph(1)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError")));
    driftGraph->graph(1)->setName("DE");
    driftGraph->graph(1)->setLineStyle(QCPGraph::lsStepLeft);

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%m:%s");
    driftGraph->xAxis->setTicker(timeTicker);
    driftGraph->axisRect()->setupFullAxesBox();
    driftGraph->yAxis->setRange(-3, 3);

    // make left and bottom axes transfer their ranges to right and top axes:
    connect(driftGraph->xAxis, SIGNAL(rangeChanged(QCPRange)), driftGraph->xAxis2, SLOT(setRange(QCPRange)));
    connect(driftGraph->yAxis, SIGNAL(rangeChanged(QCPRange)), driftGraph->yAxis2, SLOT(setRange(QCPRange)));

    driftGraph->setInteractions(QCP::iRangeZoom);

    connect(driftGraph, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(driftMouseOverLine(QMouseEvent*)));
    connect(driftGraph, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(driftMouseClicked(QMouseEvent*)));

    // Init Internal Guider always
    internalGuider = new InternalGuider();
    KConfigDialog* dialog = new KConfigDialog(this, "guidesettings", Options::self());
    opsCalibration = new OpsCalibration(internalGuider);
    dialog->addPage(opsCalibration, i18n("Calibration Settings"));
    opsGuide = new OpsGuide();
    dialog->addPage(opsGuide, i18n("Guide Settings"));
    connect(guideOptionsB, SIGNAL(clicked()), dialog, SLOT(show()));
    connect(opsGuide, SIGNAL(guiderTypeChanged(int)), this, SLOT(setGuiderType(int)));

    internalGuider->setGuideView(guideView);

    state = GUIDE_IDLE;

    // Set current guide type
    setGuiderType(-1);
}

Guide::~Guide()
{
    delete guider;
}

void Guide::addCCD(ISD::GDInterface *newCCD)
{
    ISD::CCD *ccd = static_cast<ISD::CCD*>(newCCD);

    if (CCDs.contains(ccd))
        return;

    CCDs.append(ccd);

    guiderCombo->addItem(ccd->getDeviceName());

    /*
     *
     * Check if this is necessary to save bandwidth
    if (guideType == GUIDE_INTERNAL)
    {
        // Receive BLOBs from the driver
        currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), useGuideHead ? "CCD2" : "CCD1");
    }
    else
        // Do NOT Receive BLOBs from the driver
        currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_NEVER, currentCCD->getDeviceName(), useGuideHead ? "CCD2" : "CCD1");

    */


    //checkCCD(CCDs.count()-1);
    //guiderCombo->setCurrentIndex(CCDs.count()-1);

    // setGuiderProcess(Options::useEkosGuider() ? GUIDE_INTERNAL : GUIDE_PHD2);
}

void Guide::addGuideHead(ISD::GDInterface *newCCD)
{
    ISD::CCD *ccd = static_cast<ISD::CCD *> (newCCD);

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
    currentTelescope = (ISD::Telescope*) newTelescope;

    syncTelescopeInfo();
}

bool Guide::setCCD(QString device)
{
    for (int i=0; i < guiderCombo->count(); i++)
        if (device == guiderCombo->itemText(i))
        {
            guiderCombo->setCurrentIndex(i);
            return true;
        }

    return false;
}

void Guide::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
    {
        ccdNum = guiderCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum <= CCDs.count())
    {
        currentCCD = CCDs.at(ccdNum);

        //connect(currentCCD, SIGNAL(FITSViewerClosed()), this, SLOT(viewerClosed()), Qt::UniqueConnection);
        connect(currentCCD, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processCCDNumber(INumberVectorProperty*)), Qt::UniqueConnection);
        connect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this, SLOT(checkExposureValue(ISD::CCDChip*,double,IPState)), Qt::UniqueConnection);

        // If guider is external and already connected and remote images option was disabled AND it was already
        // disabled, then let's go ahead and disable it.
        if (guiderType != GUIDE_INTERNAL && Options::guideRemoteImagesEnabled() == false && guider->isConnected())
        {
            for (int i=0; i < CCDs.count(); i++)
            {
               ISD::CCD *oneCCD = CCDs[i];
               if (i == ccdNum && oneCCD->getDriverInfo()->getClientManager()->getBLOBMode(oneCCD->getDeviceName(), "CCD1") != B_NEVER)
               {
                   appendLogText(i18n("Disabling remote image reception from %1", oneCCD->getDeviceName()));
                   oneCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_NEVER, oneCCD->getDeviceName(), "CCD1");
                   oneCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_NEVER, oneCCD->getDeviceName(), "CCD2");
               }
               // If it was already disabled, enable it back
               else if (i != ccdNum && oneCCD->getDriverInfo()->getClientManager()->getBLOBMode(oneCCD->getDeviceName(), "CCD1") == B_NEVER)
               {
                   appendLogText(i18n("Enabling remote image reception from %1", oneCCD->getDeviceName()));
                   oneCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, oneCCD->getDeviceName(), "CCD1");
                   oneCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, oneCCD->getDeviceName(), "CCD2");
               }
            }
        }

        if (currentCCD->hasGuideHead() && guiderCombo->currentText().contains("Guider"))
            useGuideHead=true;
        else
            useGuideHead=false;

        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        targetChip->setImageView(guideView, FITS_GUIDE);

        syncCCDInfo();
    }
}

void Guide::syncCCDInfo()
{
    INumberVectorProperty * nvp = NULL;

    if (currentCCD == NULL)
        return;

    if (useGuideHead)
        nvp = currentCCD->getBaseDevice()->getNumber("GUIDER_INFO");
    else
        nvp = currentCCD->getBaseDevice()->getNumber("CCD_INFO");

    if (nvp)
    {
        INumber *np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_X");
        if (np)
            ccdPixelSizeX = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np)
            ccdPixelSizeY = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np)
            ccdPixelSizeY = np->value;
    }

    updateGuideParams();
}

void Guide::syncTelescopeInfo()
{
    if (currentTelescope == NULL)
        return;

    INumberVectorProperty * nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {
        INumber *np = IUFindNumber(nvp, "GUIDER_APERTURE");

        if (np && np->value != 0)
            mountAperture = np->value;
        else
        {
            np = IUFindNumber(nvp, "TELESCOPE_APERTURE");
            if (np)
                mountAperture = np->value;
        }

        np = IUFindNumber(nvp, "GUIDER_FOCAL_LENGTH");
        if (np && np->value != 0)
            mountFocalLength = np->value;
        else
        {
            np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
            if (np)
                mountFocalLength = np->value;
        }
    }

    updateGuideParams();

}

void Guide::updateGuideParams()
{
    if (currentCCD == NULL)
        return;

    if (currentCCD->hasGuideHead() == false)
        useGuideHead = false;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (targetChip == NULL)
    {
        appendLogText(i18n("Connection to the guide CCD is lost."));
        return;
    }

    binningCombo->setEnabled(targetChip->canBin());
    int subBinX=1,subBinY=1;
    if (targetChip->canBin())
    {
        int maxBinX, maxBinY;
        targetChip->getBinning(&subBinX, &subBinY);
        targetChip->getMaxBin(&maxBinX, &maxBinY);

        binningCombo->blockSignals(true);

        binningCombo->clear();

        for (int i=1; i <= maxBinX; i++)
            binningCombo->addItem(QString("%1x%2").arg(i).arg(i));

        binningCombo->setCurrentIndex(subBinX-1);

        binningCombo->blockSignals(false);
    }

    if (frameSettings.contains(targetChip) == false)
    {
        int x,y,w,h;
        if (targetChip->getFrame(&x, &y, &w, &h))
        {
            if (w > 0 && h > 0)
            {
                QVariantMap settings;

                settings["x"] = x;
                settings["y"] = y;
                settings["w"] = w;
                settings["h"] = h;
                settings["binx"] = subBinX;
                settings["biny"] = subBinY;

                frameSettings[targetChip] = settings;
            }
        }
    }


    if (ccdPixelSizeX != -1 && ccdPixelSizeY != -1 && mountAperture != -1 && mountFocalLength != -1)
    {
        guider->setGuiderParams(ccdPixelSizeX, ccdPixelSizeY, mountAperture, mountFocalLength);
        emit guideChipUpdated(targetChip);

        int x,y,w,h;
        if (targetChip->getFrame(&x,&y,&w,&h))
        {
            guider->setFrameParams(x,y,w,h, subBinX, subBinY);
        }

        l_Focal->setText(QString::number(mountFocalLength, 'f', 1));
        l_Aperture->setText(QString::number(mountAperture, 'f', 1));
        l_FbyD->setText(QString::number(mountFocalLength/mountAperture, 'f', 1));

        // Pixel scale in arcsec/pixel
        pixScaleX = 206264.8062470963552 * ccdPixelSizeX / 1000.0 / mountFocalLength;
        pixScaleY = 206264.8062470963552 * ccdPixelSizeY / 1000.0 / mountFocalLength;

        // FOV in arcmin
        double fov_w = (w*pixScaleX)/60.0;
        double fov_h = (h*pixScaleY)/60.0;

        l_FOV->setText(QString("%1x%2").arg(QString::number(fov_w, 'f', 1)).arg(QString::number(fov_h, 'f', 1)));
    }
}

void Guide::addST4(ISD::ST4 *newST4)
{
    foreach(ISD::ST4 *guidePort, ST4List)
    {
        if (!strcmp(guidePort->getDeviceName(),newST4->getDeviceName()))
            return;
    }

    ST4List.append(newST4);

    ST4Combo->blockSignals(true);
    ST4Combo->addItem(newST4->getDeviceName());
    ST4Combo->blockSignals(false);

    // Always set the ST4Driver to the first added driver. If the default driver
    // is at another index, setST4(device) will be called from Ekos which will set a new index and that will then
    // trigger setST4(index) to update the final ST4 driver.
    ST4Driver=GuideDriver=ST4List[0];
}

bool Guide::setST4(QString device)
{
    for (int i=0; i < ST4List.count(); i++)
        if (ST4List.at(i)->getDeviceName() == device)
        {
            ST4Combo->setCurrentIndex(i);
            return true;
        }

    return false;
}

void Guide::setST4(int index)
{
    if (ST4List.empty() || index >= ST4List.count())
        return;

    ST4Driver = ST4List.at(index);

    GuideDriver = ST4Driver;

    Options::setDefaultST4Driver(ST4Driver->getDeviceName());
}

void Guide::setAO(ISD::ST4 *newAO)
{
    AODriver = newAO;
    //guider->setAO(true);
}

bool Guide::capture()
{
    buildOperationStack(GUIDE_CAPTURE);

    return executeOperationStack();
}

bool Guide::captureOneFrame()
{
    if (currentCCD == NULL)
        return false;

    double seqExpose = exposureIN->value();

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        return false;
    }

    targetChip->setCaptureMode(FITS_GUIDE);
    targetChip->setFrameType(FRAME_LIGHT);

    if (darkFrameCheck->isChecked())
        targetChip->setCaptureFilter(FITS_NONE);
    else
        targetChip->setCaptureFilter((FITSScale) filterCombo->currentIndex());

    guideView->setBaseSize(guideWidget->size());
    setBusy(true);

    if (frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];
        targetChip->setFrame(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(), settings["h"].toInt());
    }

#if 0
    switch (state)
    {
    case GUIDE_GUIDING:
        if (Options::rapidGuideEnabled() == false)
            connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)), Qt::UniqueConnection);
        targetChip->capture(seqExpose);
        return true;
        break;

    default:
        break;
    }
#endif

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)), Qt::UniqueConnection);
    if (Options::guideLogging())
        qDebug() << "Guide: Capturing frame...";

    targetChip->capture(seqExpose);

    return true;
}

bool Guide::abort()
{
    if (guiderType == GUIDE_INTERNAL)
    {
        pulseTimer.stop();
        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip->isCapturing())
            targetChip->abortExposure();
    }

    setBusy(false);

    switch (state)
    {
    case GUIDE_IDLE:
    case GUIDE_CONNECTED:
        break;
    case GUIDE_DISCONNECTED:
        if (currentCCD && guiderType != GUIDE_INTERNAL && Options::guideRemoteImagesEnabled() == false)
        {
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD1");
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD2");
        }
        break;

    case GUIDE_CALIBRATING:
    case GUIDE_DITHERING:
    case GUIDE_STAR_SELECT:
    case GUIDE_CAPTURE:
    case GUIDE_GUIDING:
        guider->abort();
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
        calibrateB->setEnabled(false);
        guideB->setEnabled(false);
        captureB->setEnabled(false);

        darkFrameCheck->setEnabled(false);
        subFrameCheck->setEnabled(false);

        stopB->setEnabled(true);

        pi->startAnimation();

        //disconnect(guideView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(setTrackingStar(int,int)));
    }
    else
    {
        if (guiderType == GUIDE_INTERNAL)
        {
            captureB->setEnabled(true);
            darkFrameCheck->setEnabled(true);
            subFrameCheck->setEnabled(true);
        }

        // All can calibrate except for PHD2
        if (guiderType != GUIDE_PHD2)
            calibrateB->setEnabled(true);

        if (calibrationComplete || guiderType != GUIDE_INTERNAL)
            guideB->setEnabled(true);

        stopB->setEnabled(false);

        pi->stopAnimation();

        connect(guideView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(setTrackingStar(int,int)), Qt::UniqueConnection);
    }
}

void Guide::newFITS(IBLOB *bp)
{
    INDI_UNUSED(bp);

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    if (Options::guideLogging())
        qDebug() << "Guide: received guide frame.";

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    int subBinX=1, subBinY=1;
    targetChip->getBinning(&subBinX, &subBinY);

    if (starCenter.x() == 0 && starCenter.y() == 0)
    {
        int x=0, y=0, w=0,h=0;

        if (frameSettings.contains(targetChip))
        {
            QVariantMap settings = frameSettings[targetChip];
            x = settings["x"].toInt();
            y = settings["y"].toInt();
            w = settings["w"].toInt();
            h = settings["h"].toInt();
        }
        else
            targetChip->getFrame(&x, &y, &w, &h);

        starCenter.setX(w/(2*subBinX));
        starCenter.setY(h/(2*subBinY));
        starCenter.setZ(subBinX);
    }

    syncTrackingBoxPosition();

    setCaptureComplete();
    /*if (operationStack.isEmpty())
        setCaptureComplete();
    else
        executeOperationStack();*/
}

void Guide::setCaptureComplete()
{
    if (operationStack.isEmpty() == false)
    {
        executeOperationStack();
        return;
    }

    DarkLibrary::Instance()->disconnect(this);

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

    case GUIDE_CALIBRATING:
        guider->calibrate();
        break;

    case GUIDE_GUIDING:
        guider->guide();
        break;

    case GUIDE_DITHERING:
        guider->dither(Options::ditherPixels());
        break;

    default:
        break;
    }

    emit newStarPixmap(guideView->getTrackingBoxPixmap());
}

void Guide::appendLogText(const QString &text)
{

    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    if (Options::guideLogging())
        qDebug() << "Guide: " << text;

    emit newLog();
}

void Guide::clearLog()
{
    logText.clear();
    emit newLog();
}

void Guide::setDECSwap(bool enable)
{
    if (ST4Driver == NULL || guider == NULL)
        return;

    if (guiderType == GUIDE_INTERNAL)
    {
        dynamic_cast<InternalGuider*>(guider)->setDECSwap(enable);
        ST4Driver->setDECSwap(enable);
    }
}

bool Guide::sendPulse( GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs )
{

    if (GuideDriver == NULL || (ra_dir == NO_DIR && dec_dir == NO_DIR))
        return false;

    if (state == GUIDE_CALIBRATING)
        pulseTimer.start( (ra_msecs > dec_msecs ? ra_msecs : dec_msecs) + 100);

    return GuideDriver->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
}

bool Guide::sendPulse( GuideDirection dir, int msecs )
{
    if (GuideDriver == NULL || dir==NO_DIR)
        return false;

    if (state == GUIDE_CALIBRATING)
        pulseTimer.start(msecs+100);

    return GuideDriver->doPulse(dir, msecs);
}

QStringList Guide::getST4Devices()
{
    QStringList devices;

    foreach(ISD::ST4* driver, ST4List)
        devices << driver->getDeviceName();

    return devices;
}

#if 0
void Guide::processRapidStarData(ISD::CCDChip *targetChip, double dx, double dy, double fit)
{
    // Check if guide star is lost
    if (dx == -1 && dy == -1 && fit == -1)
    {
        KMessageBox::error(NULL, i18n("Lost track of the guide star. Rapid guide aborted."));

        guider->abort();
        return;
    }

    FITSView *targetImage = targetChip->getImage(FITS_GUIDE);

    if (targetImage == NULL)
    {
        pmath->setImageView(NULL);
        guider->setImageView(NULL);
        calibration->setImageView(NULL);
    }

    if (rapidGuideReticleSet == false)
    {
        // Let's set reticle parameter on first capture to those of the star, then we check if there
        // is any set
        double x,y,angle;
        pmath->getReticleParameters(&x, &y, &angle);
        pmath->setReticleParameters(dx, dy, angle);
        rapidGuideReticleSet = true;
    }

    pmath->setRapidStarData(dx, dy);

    if (guider->isDithering())
    {
        pmath->performProcessing();
        if (guider->dither() == false)
        {
            appendLogText(i18n("Dithering failed. Autoguiding aborted."));
            emit newStatus(GUIDE_DITHERING_ERROR);
            guider->abort();
            //emit ditherFailed();
        }
    }
    else
    {
        guider->guide();
        capture();
    }

}

void Guide::startRapidGuide()
{
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (currentCCD->setRapidGuide(targetChip, true) == false)
    {
        appendLogText(i18n("The CCD does not support Rapid Guiding. Aborting..."));
        guider->abort();
        return;
    }

    rapidGuideReticleSet = false;

    pmath->setRapidGuide(true);
    currentCCD->configureRapidGuide(targetChip, true);
    connect(currentCCD, SIGNAL(newGuideStarData(ISD::CCDChip*,double,double,double)), this, SLOT(processRapidStarData(ISD::CCDChip*,double,double,double)));
}

void Guide::stopRapidGuide()
{
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    pmath->setRapidGuide(false);

    rapidGuideReticleSet = false;

    currentCCD->disconnect(SIGNAL(newGuideStarData(ISD::CCDChip*,double,double,double)));

    currentCCD->configureRapidGuide(targetChip, false, false, false);

    currentCCD->setRapidGuide(targetChip, false);
}
#endif

bool Guide::calibrate()
{
    // Set status to idle and let the operations change it as they get executed
    state = GUIDE_IDLE;
    emit newStatus(state);

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (frameSettings.contains(targetChip))
    {
        targetChip->resetFrame();
        int x,y,w,h;
        targetChip->getFrame(&x, &y, &w, &h);
        QVariantMap settings = frameSettings[targetChip];
        settings["x"] = x;
        settings["y"] = y;
        settings["w"] = w;
        settings["h"] = h;
        frameSettings[targetChip] = settings;

        subFramed = false;
    }

    saveSettings();        

    buildOperationStack(GUIDE_CALIBRATING);

    executeOperationStack();

    return true;
}

bool Guide::guide()
{
    saveSettings();

    bool rc = guider->guide();

    return rc;
}

bool Guide::dither()
{
    if (state == GUIDE_DITHERING)
        return true;

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

    default:
        break;
    }
}

void Guide::setMountStatus(ISD::Telescope::TelescopeStatus newState)
{
    switch (newState)
    {
        case ISD::Telescope::MOUNT_PARKING:
            abort();
        break;

        default:
        break;
    }
}

void Guide::setExposure(double value)
{
    exposureIN->setValue(value);
}

void Guide::setImageFilter(const QString & value)
{
    for (int i=0; i < filterCombo->count(); i++)
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
    Options::setGuideAutoStarEnabled(enable);
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
}

#if 0
void Guide::setGuideRapidEnabled(bool enable)
{
    //guider->setGuideOptions(guider->getAlgorithm(), guider->useSubFrame() , enable);
}
#endif

void Guide::setDitherSettings(bool enable, double value)
{    
    Options::setDitherEnabled(enable);
    Options::setDitherPixels(value);
}

QList<double> Guide::getGuidingDeviation()
{
    QList<double> deviation;

    deviation << guideDeviationRA << guideDeviationDEC;

    return deviation;
}

void Guide::startAutoCalibrateGuide()
{
    // A must for auto stuff
    Options::setGuideAutoStarEnabled(true);

    calibrationComplete = false;
    autoCalibrateGuide = true;

    calibrate();
}

void Guide::setStatus(Ekos::GuideState newState)
{
    if (newState == state)
        return;

    GuideState previousState = state;

    state = newState;
    emit newStatus(state);

    switch (state)
    {
    case GUIDE_CONNECTED:
        appendLogText(i18n("External guider connected."));
        externalConnectB->setEnabled(false);
        externalDisconnectB->setEnabled(true);
        if (guiderType == GUIDE_LINGUIDER)
            calibrateB->setEnabled(true);
        guideB->setEnabled(true);
        if (currentCCD && guiderType != GUIDE_INTERNAL && Options::guideRemoteImagesEnabled() == false)
        {
            appendLogText(i18n("Disabling remote image reception from %1", currentCCD->getDeviceName()));
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_NEVER, currentCCD->getDeviceName(), "CCD1");
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_NEVER, currentCCD->getDeviceName(), "CCD2");
        }
        break;

    case GUIDE_DISCONNECTED:
        appendLogText(i18n("External guider disconnected."));
        externalConnectB->setEnabled(true);
        externalDisconnectB->setEnabled(false);
        calibrateB->setEnabled(false);
        guideB->setEnabled(false);
        if (currentCCD && guiderType != GUIDE_INTERNAL && Options::guideRemoteImagesEnabled() == false)
        {
            appendLogText(i18n("Enabling remote image reception from %1", currentCCD->getDeviceName()));
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD1");
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD2");
        }
        break;

    case GUIDE_CALIBRATION_SUCESS:
        appendLogText(i18n("Calibration completed."));
        calibrationComplete = true;
        if (autoCalibrateGuide)
        {
            autoCalibrateGuide = false;
            guide();
        }
        else
            setBusy(false);
        break;

    case GUIDE_CALIBRATION_ERROR:
        setBusy(false);
        break;

    case GUIDE_CALIBRATING:
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

            driftGraph->graph(0)->data().clear();
            driftGraph->graph(1)->data().clear();
            guideTimer = QTime::currentTime();
            refreshColorScheme();
        }

        break;

    case GUIDE_ABORTED:
        appendLogText(i18n("Autoguiding aborted."));
        setBusy(false);
        break;

    case GUIDE_SUSPENDED:
        appendLogText(i18n("Guiding suspended."));
        break;

    case GUIDE_DITHERING:
        appendLogText(i18n("Dithering in progress."));
        break;

    case GUIDE_DITHERING_ERROR:
        appendLogText(i18n("Dithering failed!"));
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
        // Go back to guiding state immediately
        setStatus(GUIDE_GUIDING);
        capture();
        break;
    default:
        break;
    }
}

void Guide::updateCCDBin(int index)
{
    if (currentCCD == NULL && guiderType != GUIDE_INTERNAL)
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    targetChip->setBinning(index+1, index+1);

    QVariantMap settings = frameSettings[targetChip];
    settings["binx"] = index+1;
    settings["biny"] = index+1;
    frameSettings[targetChip] = settings;
}

void Guide::processCCDNumber(INumberVectorProperty *nvp)
{
    if (currentCCD == NULL || strcmp(nvp->device, currentCCD->getDeviceName()))
        return;

    if ( (!strcmp(nvp->name, "CCD_BINNING") && useGuideHead == false) || (!strcmp(nvp->name, "GUIDER_BINNING") && useGuideHead) )
    {
        binningCombo->disconnect();
        binningCombo->setCurrentIndex(nvp->np[0].value-1);
        connect(binningCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCCDBin(int)));
    }
}

void Guide::checkExposureValue(ISD::CCDChip *targetChip, double exposure, IPState expState)
{
    INDI_UNUSED(exposure);

    if (expState == IPS_ALERT && ( (state == GUIDE_GUIDING) || (state == GUIDE_DITHERING) || (state == GUIDE_CALIBRATING)) )
    {
        appendLogText(i18n("Exposure failed. Restarting exposure..."));
        targetChip->capture(exposureIN->value());
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
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    Q_ASSERT(targetChip);

    int subBinX=1, subBinY=1;
    targetChip->getBinning(&subBinX, &subBinY);

    if (starCenter.isNull() == false)
    {
        double boxSize = boxSizeCombo->currentText().toInt();
        int x,y,w,h;
        targetChip->getFrame(&x,&y,&w,&h);
        // If box size is larger than image size, set it to lower index
        if (boxSize/subBinX >= w || boxSize/subBinY >= h)
        {
            int newIndex = boxSizeCombo->currentIndex()-1;
            if (newIndex >= 0)
                boxSizeCombo->setCurrentIndex(newIndex);
            return;
        }

        // If binning changed, update coords accordingly
        if (subBinX != starCenter.z())
        {
            if (starCenter.z() > 0)
            {
                starCenter.setX(starCenter.x() * (starCenter.z()/subBinX));
                starCenter.setY(starCenter.y() * (starCenter.z()/subBinY));
            }

            starCenter.setZ(subBinX);
        }

        QRect starRect = QRect( starCenter.x()-boxSize/(2*subBinX), starCenter.y()-boxSize/(2*subBinY), boxSize/subBinX, boxSize/subBinY);
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

    if (guider)
        guider->disconnect();

    switch (type)
    {
    case GUIDE_INTERNAL:
    {
        connect(internalGuider, SIGNAL(newPulse(GuideDirection,int)), this, SLOT(sendPulse(GuideDirection,int)));
        connect(internalGuider, SIGNAL(newPulse(GuideDirection,int,GuideDirection,int)), this, SLOT(sendPulse(GuideDirection,int,GuideDirection,int)));
        connect(internalGuider, SIGNAL(DESwapChanged(bool)), swapCheck, SLOT(setChecked(bool)));

        guider = internalGuider;

        calibrateB->setEnabled(true);        
        guideB->setEnabled(false);
        captureB->setEnabled(true);
        darkFrameCheck->setEnabled(true);
        subFrameCheck->setEnabled(true);

        externalConnectB->setEnabled(false);
        externalDisconnectB->setEnabled(false);

        controlGroup->setEnabled(true);

        updateGuideParams();
    }
        break;

    case GUIDE_PHD2:
        if (phd2Guider.isNull())
            phd2Guider = new PHD2();

        guider = phd2Guider;

        calibrateB->setEnabled(false);
        captureB->setEnabled(false);
        darkFrameCheck->setEnabled(false);
        subFrameCheck->setEnabled(false);
        guideB->setEnabled(true);
        externalConnectB->setEnabled(false);
        controlGroup->setEnabled(false);
        break;

    case GUIDE_LINGUIDER:
        if (linGuider.isNull())
            linGuider = new LinGuider();

        guider = linGuider;

        calibrateB->setEnabled(true);
        captureB->setEnabled(false);
        darkFrameCheck->setEnabled(false);
        subFrameCheck->setEnabled(false);
        guideB->setEnabled(true);
        externalConnectB->setEnabled(true);
        controlGroup->setEnabled(false);

        break;
    }

    connect(guider, SIGNAL(frameCaptureRequested()), this, SLOT(capture()));
    connect(guider, SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));
    connect(guider, SIGNAL(newStatus(Ekos::GuideState)), this, SLOT(setStatus(Ekos::GuideState)));
    connect(guider, SIGNAL(newStarPosition(QVector3D,bool)), this, SLOT(setStarPosition(QVector3D,bool)));

    connect(guider, SIGNAL(newAxisDelta(double,double)), this, SLOT(setAxisDelta(double,double)));
    connect(guider, SIGNAL(newAxisPulse(double,double)), this, SLOT(setAxisPulse(double,double)));
    connect(guider, SIGNAL(newAxisSigma(double,double)), this, SLOT(setAxisSigma(double,double)));

    guider->Connect();

    guiderType = static_cast<GuiderType>(type);

    return true;
}

void Guide::updateTrackingBoxSize(int currentIndex)
{
    if (currentIndex >= 0)
    {
        Options::setGuideSquareSizeIndex(currentIndex);
        syncTrackingBoxPosition();
    }
}

bool Guide::selectAutoStar()
{
    if (currentCCD == NULL)
        return false;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    if (targetChip == NULL)
        return false;

    FITSView *targetImage    = targetChip->getImageView(FITS_GUIDE);
    if (targetImage == NULL)
        return false;

    FITSData *imageData = targetImage->getImageData();
    if (imageData == NULL)
        return false;

    imageData->findStars();

    QList<Edge*> starCenters = imageData->getStarCenters();

    if (starCenters.empty())
        return false;

    guideView->setStarsEnabled(true);
    guideView->updateFrame();

    qSort(starCenters.begin(), starCenters.end(), [](const Edge *a, const Edge *b){return a->width > b->width;});

    int maxX = imageData->getWidth();
    int maxY = imageData->getHeight();

    int scores[MAX_GUIDE_STARS];

    int maxIndex = MAX_GUIDE_STARS < starCenters.count() ? MAX_GUIDE_STARS : starCenters.count();

    for (int i=0; i < maxIndex; i++)
    {
        int score=100;

        Edge *center = starCenters.at(i);

        //qDebug() << "#" << i << " X: " << center->x << " Y: " << center->y << " HFR: " << center->HFR << " Width" << center->width;

        // Severely reject stars close to edges
        if (center->x < (center->width*5) || center->y < (center->width*5) || center->x > (maxX-center->width*5) || center->y > (maxY-center->width*5))
            score-=50;

        // Moderately favor brighter stars
        score += center->width*center->width;

        // Moderately reject stars close to other stars
        foreach(Edge *edge, starCenters)
        {
            if (edge == center)
                continue;

            if (abs(center->x - edge->x) < center->width*2 && abs(center->y - edge->y) < center->width*2)
            {
                score -= 15;
                break;
            }
        }

        scores[i] = score;
    }

    int maxScore=0;
    int maxScoreIndex=0;
    for (int i=0; i < maxIndex; i++)
    {
        if (scores[i] > maxScore)
        {
            maxScore = scores[i];
            maxScoreIndex = i;
        }
    }

    /*if (ui.autoSquareSizeCheck->isEnabled() && ui.autoSquareSizeCheck->isChecked())
    {
        // Select appropriate square size
        int idealSize = ceil(starCenters[maxScoreIndex]->width * 1.5);

        if (Options::guideLogging())
            qDebug() << "Guide: Ideal calibration box size for star width: " << starCenters[maxScoreIndex]->width << " is " << idealSize << " pixels";

        // TODO Set square size in GuideModule
    }*/

    QVector3D newStarCenter(starCenters[maxScoreIndex]->x, starCenters[maxScoreIndex]->y, 0);
    setStarPosition(newStarCenter, false);

    return true;
}

/*
void Guide::onXscaleChanged( int i )
{
    int rx, ry;

    driftGraphics->getVisibleRanges( &rx, &ry );
    driftGraphics->setVisibleRanges( i*driftGraphics->getGridN(), ry );
    driftGraphics->update();

}

void Guide::onYscaleChanged( int i )
{
    int rx, ry;

    driftGraphics->getVisibleRanges( &rx, &ry );
    driftGraphics->setVisibleRanges( rx, i*driftGraphics->getGridN() );
    driftGraphics->update();
}
*/


void Guide::onThresholdChanged( int index )
{
    switch (guiderType)
    {
    case GUIDE_INTERNAL:
        dynamic_cast<InternalGuider*>(guider)->setSquareAlgorithm(index);
        break;

    default:
        break;
    }
}

void Guide::onInfoRateChanged( double val )
{
    Options::setGuidingRate(val);

    double gain = 0;

    if( val > 0.01 )
        gain = 1000.0 / (val * 15.0);

    l_RecommendedGain->setText( i18n("P: %1", QString().setNum(gain, 'f', 2 )) );
}

void Guide::onEnableDirRA(bool enable)
{
    Options::setRAGuideEnabled(enable);
}

void Guide::onEnableDirDEC(bool enable)
{
    Options::setDECGuideEnabled(enable);
}

void Guide::onInputParamChanged()
{
    QSpinBox *pSB;
    QDoubleSpinBox *pDSB;

    QObject *obj = sender();

    if( (pSB = dynamic_cast<QSpinBox *>(obj)) )
    {
        if( pSB == spinBox_MaxPulseRA )
            Options::setRAMaximumPulse(pSB->value());
        else if ( pSB == spinBox_MaxPulseDEC )
            Options::setDECMaximumPulse(pSB->value());
        else if ( pSB == spinBox_MinPulseRA )
            Options::setRAMinimumPulse(pSB->value());
        else if( pSB == spinBox_MinPulseDEC )
            Options::setDECMinimumPulse(pSB->value());
    }
    else if( (pDSB = dynamic_cast<QDoubleSpinBox *>(obj)) )
    {
        if( pDSB == spinBox_PropGainRA )
            Options::setRAPropotionalGain(pDSB->value());
        else if ( pDSB == spinBox_PropGainDEC )
            Options::setDECPropotionalGain(pDSB->value());
        else if ( pDSB == spinBox_IntGainRA )
            Options::setRAIntegralGain(pDSB->value());
        else if( pDSB == spinBox_IntGainDEC )
            Options::setDECIntegralGain(pDSB->value());
        else if( pDSB == spinBox_DerGainRA )
            Options::setRADerivativeGain(pDSB->value());
        else if( pDSB == spinBox_DerGainDEC )
            Options::setDECDerivativeGain(pDSB->value());
    }
}

void Guide::onControlDirectionChanged(bool enable)
{
    QObject *obj = sender();

    if (northControlCheck == dynamic_cast<QCheckBox*>(obj))
    {
        Options::setNorthDECGuideEnabled(enable);
    }
    else if (southControlCheck == dynamic_cast<QCheckBox*>(obj))
    {
        Options::setSouthDECGuideEnabled(enable);
    }
    else if (westControlCheck == dynamic_cast<QCheckBox*>(obj))
    {
        Options::setWestRAGuideEnabled(enable);
    }
    else if (eastControlCheck == dynamic_cast<QCheckBox*>(obj))
    {
        Options::setEastRAGuideEnabled(enable);
    }
}

#if 0
void Guide::onRapidGuideChanged(bool enable)
{
    if (m_isStarted)
    {
        guideModule->appendLogText(i18n("You must stop auto guiding before changing this setting."));
        return;
    }

    m_useRapidGuide = enable;

    if (m_useRapidGuide)
    {
        guideModule->appendLogText(i18n("Rapid Guiding is enabled. Guide star will be determined automatically by the CCD driver. No frames are sent to Ekos unless explicitly enabled by the user in the CCD driver settings."));
    }
    else
        guideModule->appendLogText(i18n("Rapid Guiding is disabled."));
}
#endif

void Guide::loadSettings()
{
    // Exposure
    exposureIN->setValue(Options::guideExposure());
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
    // PID Control - Propotional Gain
    spinBox_PropGainRA->setValue(Options::rAPropotionalGain());
    spinBox_PropGainDEC->setValue(Options::dECPropotionalGain());
    // PID Control - Integral Gain
    spinBox_IntGainRA->setValue(Options::rAIntegralGain());
    spinBox_IntGainDEC->setValue(Options::dECIntegralGain());
    // PID Control - Derivative Gain
    spinBox_DerGainRA->setValue(Options::rADerivativeGain());
    spinBox_DerGainDEC->setValue(Options::dECDerivativeGain());
    // Max Pulse Duration (ms)
    spinBox_MaxPulseRA->setValue(Options::rAMaximumPulse());
    spinBox_MaxPulseDEC->setValue(Options::dECMaximumPulse());
    // Min Pulse Duration (ms)
    spinBox_MinPulseRA->setValue(Options::rAMinimumPulse());
    spinBox_MinPulseDEC->setValue(Options::dECMinimumPulse());
}

void Guide::saveSettings()
{
    // Exposure
    Options::setGuideExposure(exposureIN->value());
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
    // PID Control - Propotional Gain
    Options::setRAPropotionalGain(spinBox_PropGainRA->value());
    Options::setDECPropotionalGain(spinBox_PropGainDEC->value());
    // PID Control - Integral Gain
    Options::setRAIntegralGain(spinBox_IntGainRA->value());
    Options::setDECIntegralGain(spinBox_IntGainDEC->value());
    // PID Control - Derivative Gain
    Options::setRADerivativeGain(spinBox_DerGainRA->value());
    Options::setDECDerivativeGain(spinBox_DerGainDEC->value());
    // Max Pulse Duration (ms)
    Options::setRAMaximumPulse(spinBox_MaxPulseRA->value());
    Options::setDECMaximumPulse(spinBox_MaxPulseDEC->value());
    // Min Pulse Duration (ms)
    Options::setRAMinimumPulse(spinBox_MinPulseRA->value());
    Options::setDECMinimumPulse(spinBox_MinPulseDEC->value());
}

void Guide::setTrackingStar(int x, int y)
{    
    QVector3D newStarPosition(x,y, -1);
    setStarPosition(newStarPosition, true);

    /*if (state == GUIDE_STAR_SELECT)
    {
        guider->setStarPosition(newStarPosition);
        guider->calibrate();
    }*/

    if (operationStack.isEmpty() == false)
        executeOperationStack();
}

void Guide::setAxisDelta(double ra, double de)
{
    // Time since timer started.
    double key = guideTimer.elapsed()/1000.0;

    driftGraph->graph(0)->addData(key, ra);
    driftGraph->graph(1)->addData(key, de);

    // Expand range if it doesn't fit already
    if (driftGraph->yAxis->range().contains(ra) == false)
        driftGraph->yAxis->setRange(-1.25*ra, 1.25*ra);

    if (driftGraph->yAxis->range().contains(de) == false)
        driftGraph->yAxis->setRange(-1.25*de, 1.25*de);

    // Show last 120 seconds
    //driftGraph->xAxis->setRange(key, 120, Qt::AlignRight);
    driftGraph->xAxis->setRange(key, driftGraph->xAxis->range().size(), Qt::AlignRight);
    driftGraph->replot();

    l_DeltaRA->setText(QString::number(ra, 'f', 2));
    l_DeltaDEC->setText(QString::number(de, 'f', 2));

    emit newAxisDelta(ra,de);

    profilePixmap = driftGraph->grab(QRect(QPoint(0, 50), QSize(driftGraph->width(), 150)));
    emit newProfilePixmap(profilePixmap);

}

void Guide::setAxisSigma(double ra, double de)
{
    l_ErrRA->setText(QString::number(ra, 'f', 2));
    l_ErrDEC->setText(QString::number(de, 'f', 2));
}

void Guide::setAxisPulse(double ra, double de)
{
    l_PulseRA->setText(QString::number(static_cast<int>(ra)));
    l_PulseDEC->setText(QString::number(static_cast<int>(de)));
}

void Guide::refreshColorScheme()
{
    // Drift color legend
    driftGraph->graph(0)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError")));
    driftGraph->graph(1)->setPen(QPen(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError")));
}

void Guide::driftMouseClicked(QMouseEvent *event)
{
    if (event->buttons() & Qt::RightButton)
    {
        driftGraph->yAxis->setRange(-3, 3);
    }
}

void Guide::driftMouseOverLine(QMouseEvent *event)
{
    double key      = driftGraph->xAxis->pixelToCoord(event->localPos().x());

    if (driftGraph->xAxis->range().contains(key))
    {
        QCPGraph *graph = qobject_cast<QCPGraph *>(driftGraph->plottableAt(event->pos(), false));

        if(graph)
        {
            int    raIndex  = driftGraph->graph(0)->findBegin(key);
            int    deIndex  = driftGraph->graph(1)->findBegin(key);

            double raDelta  = driftGraph->graph(0)->dataMainValue(raIndex);
            double deDelta  = driftGraph->graph(1)->dataMainValue(deIndex);

            // Compute time value:
            QTime localTime = guideTimer;

            localTime = localTime.addSecs(key);

            QToolTip::hideText();
            QToolTip::showText(event->globalPos(),
                               i18nc("Drift graphics tooltip; %1 is local time; %2 is RA deviation; %3 is DE deviation in arcseconds",
                                     "<table>"
                                     "<tr><td>LT:   </td><td>%1</td></tr>"
                                     "<tr><td>RA:   </td><td>%2 \"</td></tr>"
                                     "<tr><td>DE:   </td><td>%3 \"</td></tr>"
                                     "</table>", localTime.toString("hh:mm:ss AP"), QString::number(raDelta, 'f', 2), QString::number(deDelta, 'f', 2)));
        }
        else
            QToolTip::hideText();

        driftGraph->replot();
    }

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
        if (guiderType == GUIDE_INTERNAL && (starCenter.isNull() || (Options::guideAutoStarEnabled())))
        {
            if (Options::guideDarkFrameEnabled())
                operationStack.push(GUIDE_DARK);

            // If subframe is enabled and we need to auto select a star, then we need to make the final capture
            // of the subframed image. This is only done if we aren't already subframed.
            if (subFramed == false && Options::guideSubframeEnabled() && Options::guideAutoStarEnabled())
                operationStack.push(GUIDE_CAPTURE);

            operationStack.push(GUIDE_SUBFRAME);
            operationStack.push(GUIDE_STAR_SELECT);
            operationStack.push(GUIDE_CAPTURE);

            // If we are being ask to go full frame, let's do that first
            if (subFramed == true && Options::guideSubframeEnabled() == false)
                operationStack.push(GUIDE_SUBFRAME);
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
            guider->setStarPosition(starCenter);
        if (guider->calibrate())
        {
            if (guiderType == GUIDE_INTERNAL)
                disconnect(guideView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(setTrackingStar(int,int)));
            setBusy(true);
        }
        else
        {
            emit newStatus(GUIDE_CALIBRATION_ERROR);
            state = GUIDE_IDLE;
            appendLogText(i18n("Calibration failed to start!"));
            setBusy(false);
        }
        break;

    default:
        break;
    }

    // If an additional action is required, return return and continue later
    if (actionRequired)
        return true;
    // Othereise, continue processing the stack
    else
        return executeOperationStack();
}

bool Guide::executeOneOperation(GuideState operation)
{
    bool actionRequired = false;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    int subBinX, subBinY;
    targetChip->getBinning(&subBinX, &subBinY);

    switch (operation)
    {
    case GUIDE_SUBFRAME:
    {
        // Do not subframe if we are capturing calibration frame
        if (subFramed == false && Options::guideSubframeEnabled() == true && targetChip->canSubframe())
        {
            int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
            targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

            int offset = boxSizeCombo->currentText().toInt()/subBinX;            

            int x = starCenter.x();
            int y = starCenter.y();

            x = (x - offset*2) * subBinX;
            y = (y - offset*2) * subBinY;
            int w=offset*4*subBinX;
            int h=offset*4*subBinY;

            if (x<minX)
                x=minX;
            if (y<minY)
                y=minY;
            if ((x+w)>maxW)
                w=maxW-x;
            if ((y+h)>maxH)
                h=maxH-y;

            targetChip->setFrame(x,y,w,h);

            subFramed = true;
            QVariantMap settings = frameSettings[targetChip];
            settings["x"] = x;
            settings["y"] = y;
            settings["w"] = w;
            settings["h"] = h;
            settings["binx"] = subBinX;
            settings["biny"] = subBinY;

            frameSettings[targetChip] = settings;

            starCenter.setX(w/(2*subBinX));
            starCenter.setY(h/(2*subBinX));
        }
        else if (subFramed && Options::guideSubframeEnabled() == false)
        {
            targetChip->resetFrame();

            int x,y,w,h;
            targetChip->getFrame(&x,&y,&w, &h);

            QVariantMap settings;
            settings["x"] = x;
            settings["y"] = y;
            settings["w"] = w;
            settings["h"] = h;
            settings["binx"] = 1;
            settings["biny"] = 1;
            frameSettings[targetChip] = settings;

            subFramed = false;

            starCenter.setX(w/(2*subBinX));
            starCenter.setY(h/(2*subBinX));

            //starCenter.setX(0);
            //starCenter.setY(0);
        }
    }
        break;

    case GUIDE_DARK:
    {
        // Do we need to take a dark frame?
        if (Options::guideDarkFrameEnabled())
        {
            FITSData *darkData       = NULL;
            QVariantMap settings = frameSettings[targetChip];
            uint16_t offsetX = settings["x"].toInt() / settings["binx"].toInt();
            uint16_t offsetY = settings["y"].toInt() / settings["biny"].toInt();

            darkData = DarkLibrary::Instance()->getDarkFrame(targetChip, exposureIN->value());

            connect(DarkLibrary::Instance(), SIGNAL(darkFrameCompleted(bool)), this, SLOT(setCaptureComplete()));
            connect(DarkLibrary::Instance(), SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));

            actionRequired = true;

            targetChip->setCaptureFilter((FITSScale) filterCombo->currentIndex());

            if (darkData)
                DarkLibrary::Instance()->subtract(darkData, guideView, targetChip->getCaptureFilter(), offsetX, offsetY);
            else
            {
                bool rc = DarkLibrary::Instance()->captureAndSubtract(targetChip, guideView, exposureIN->value(), offsetX, offsetY);
                setDarkFrameEnabled(rc);
            }
        }
    }
        break;

    case GUIDE_STAR_SELECT:
    {
        state = GUIDE_STAR_SELECT;
        emit newStatus(state);

        if (Options::guideAutoStarEnabled())
        {
            bool autoStarCaptured = selectAutoStar();
            if (autoStarCaptured)
            {
                appendLogText(i18n("Auto star selected."));
            }
            else
            {
                appendLogText(i18n("Failed to select an auto star."));
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
    FITSData *data = guideView->getImageData();
    if (data)
    {
        QUrl url = QUrl::fromLocalFile(data->getFilename());

        if (fv.isNull())
        {
            if (Options::singleWindowCapturedFITS())
                fv = KStars::Instance()->genericFITSViewer();
            else
                fv = new FITSViewer(Options::independentWindowFITS() ? NULL : KStars::Instance());

            fv->addFITS(&url);
        }
        else
            fv->updateFITS(&url, 0);

        fv->show();
    }
}
}


