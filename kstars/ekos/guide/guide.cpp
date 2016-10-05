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
#include "scroll_graph.h"
#include "externalguide/phd2.h"

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
#define MAX_DITHER_RETIRES  20
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

    // AO Driver
    AODriver= NULL;

    // ST4 Driver
    GuideDriver=NULL;

    // Auto Star
    autoStarCaptured = false;

    guideView = new FITSView(guideWidget);
    guideView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    guideView->setBaseSize(guideWidget->size());
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(guideView);
    guideWidget->setLayout(vlayout);
    connect(guideView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(setTrackingStar(int,int)));

    ccdPixelSizeX =  ccdPixelSizeY =  mountAperture =  mountFocalLength = pixScaleX = pixScaleY = -1;
    guideDeviationRA = guideDeviationDEC = 0;

    useGuideHead = false;
    rapidGuideReticleSet = false;        

    // Load all settings
    loadSettings();

    // Set color scheme
    refreshColorScheme();

    // Image Filters
    foreach(QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    // Progress Indicator
    pi = new QProgressIndicator(this);
    controlLayout->addWidget(pi, 0, 1, 1, 1);

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

    // ST4 Selection
    connect(ST4Combo, SIGNAL(currentIndexChanged(int)), this, SLOT(setST4(int)));
    //connect(ST4Combo, SIGNAL(activated(QString)), this, SLOT(setDefaultST4(QString)));
    //connect(ST4Combo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this, [&](const QString & st4){Options::setDefaultST4Driver(st4);});

    // Binning Combo Selection
    connect(binningCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCCDBin(int)));

    // Drift Graph scales
    //connect( spinBox_XScale, 		SIGNAL(valueChanged(int)),	this, SLOT(onXscaleChanged(int)) );
    //connect( spinBox_YScale, 		SIGNAL(valueChanged(int)),	this, SLOT(onYscaleChanged(int)) );

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

    // Drift Graph

    /*driftGraph->setBackground(QBrush(Qt::black));
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
    driftGraph->yAxis->setLabelColor(Qt::white);*/

    driftGraph->addGraph(); // blue line
    driftGraph->graph(0)->setPen(QPen(QColor(40, 110, 255)));
    driftGraph->addGraph(); // red line
    driftGraph->graph(1)->setPen(QPen(QColor(255, 110, 40)));

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%m:%s");
    driftGraph->xAxis->setTicker(timeTicker);
    driftGraph->axisRect()->setupFullAxesBox();
    //driftGraph->yAxis->setRange(-3, 3);

    // make left and bottom axes transfer their ranges to right and top axes:
    connect(driftGraph->xAxis, SIGNAL(rangeChanged(QCPRange)), driftGraph->xAxis2, SLOT(setRange(QCPRange)));
    connect(driftGraph->yAxis, SIGNAL(rangeChanged(QCPRange)), driftGraph->yAxis2, SLOT(setRange(QCPRange)));



    //driftGraph = new ScrollGraph( this, driftGraph_WIDTH, driftGraph_HEIGHT );
    //driftGraphics->setSize(driftGraph_WIDTH, driftGraph_HEIGHT);
    //driftGraphics->setSize(200, 200);
    //driftGraphics->setVisibleRanges( 200, 60 );
    //driftGraphics->update();
    //driftGraphics->on_paint();
    //ui.frame_Graph->resize( driftGraph_WIDTH + 2*ui.frame_Graph->frameWidth(), driftGraph_HEIGHT + 2*ui.frame_Graph->frameWidth() );

    guiderType = static_cast<GuiderType>(Options::guiderType());

    switch (guiderType)
    {
    case GUIDE_INTERNAL:
    {
        guider= new InternalGuider();
        InternalGuider *internalGuider = dynamic_cast<InternalGuider*>(guider);
        // Options
        KConfigDialog* dialog = new KConfigDialog(this, "guidesettings", Options::self());
        opsCalibration = new OpsCalibration(internalGuider);
        dialog->addPage(opsCalibration, i18n("Calibration Settings"));
        opsGuide = new OpsGuide(internalGuider);
        dialog->addPage(opsGuide, i18n("Guide Settings"));
        connect(guideOptionsB, SIGNAL(clicked()), dialog, SLOT(show()));

        connect(internalGuider, SIGNAL(newPulse(GuideDirection,int)), this, SLOT(sendPulse(GuideDirection,int)));
        connect(internalGuider, SIGNAL(newPulse(GuideDirection,int,GuideDirection,int)), this, SLOT(sendPulse(GuideDirection,int,GuideDirection,int)));
        connect(internalGuider, SIGNAL(DESwapChanged(bool)), swapCheck, SLOT(setChecked(bool)));
        //connect(swapCheck, SIGNAL(toggled(bool)), internalGuider, SLOT(setDECSwap(bool)));

        internalGuider->setGuideView(guideView);

        calibrateB->setEnabled(true);
        captureB->setEnabled(true);
    }
        break;

    case GUIDE_PHD2:
        //guider = new PHD2();

        // Do NOT receive BLOBs from the driver
        //currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_NEVER, currentCCD->getDeviceName(), useGuideHead ? "CCD2" : "CCD1");

        calibrateB->setEnabled(false);
        captureB->setEnabled(false);
        break;


    case GUIDE_LINGUIDER:
        //guider = new LINGuider();

        // Do NOT receive BLOBs from the driver
        //currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_NEVER, currentCCD->getDeviceName(), useGuideHead ? "CCD2" : "CCD1");
        calibrateB->setEnabled(false);
        captureB->setEnabled(false);
        break;
    }


    state = GUIDE_IDLE;

    connect(guider, SIGNAL(frameCaptureRequested()), this, SLOT(capture()));
    connect(guider, SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));
    connect(guider, SIGNAL(newStatus(Ekos::GuideState)), this, SLOT(setStatus(Ekos::GuideState)));
    connect(guider, SIGNAL(newStarPosition(QVector3D,bool)), this, SLOT(setStarPosition(QVector3D,bool)));

    connect(guider, SIGNAL(newAxisDelta(double,double)), this, SLOT(setAxisDelta(double,double)));
    connect(guider, SIGNAL(newAxisPulse(double,double)), this, SLOT(setAxisPulse(double,double)));
    connect(guider, SIGNAL(newAxisSigma(double,double)), this, SLOT(setAxisSigma(double,double)));

    // FIXME we emit this directly TODO
    //connect(guider, SIGNAL(newProfilePixmap(QPixmap &)), this, SIGNAL(newProfilePixmap(QPixmap &)));


    guider->Connect();
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

        if (currentCCD->hasGuideHead() && guiderCombo->currentText().contains("Guider"))
            useGuideHead=true;
        else
            useGuideHead=false;

        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        targetChip->setImageView(guideView, FITS_GUIDE);

        syncCCDInfo();
    }
}

/*void Guide::setGuiderProcess(int guiderProcess)
{
    // Don't do anything unless we have a CCD and it is online
    if (currentCCD == NULL || currentCCD->isConnected() == false)
        return;

    if (guiderProcess == GUIDE_PHD2)
    {
        // Disable calibration tab
        tabWidget->setTabEnabled(0, false);
        // Enable guide tab
        tabWidget->setTabEnabled(1, true);
        // Set current tab to guide
        tabWidget->setCurrentIndex(1);

        guider->setPHD2(phd2);

        // Do not receive BLOBs from the driver
        currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_NEVER, currentCCD->getDeviceName(), useGuideHead ? "CCD2" : "CCD1");
    }
    else
    {
        // Enable calibration tab
        tabWidget->setTabEnabled(0, true);
        // Disable guide tab?
        // TODO: Check if calibration is already complete, then no need to disable guiding tab
        tabWidget->setTabEnabled(1, false);
        // Set current tab to calibration
        tabWidget->setCurrentIndex(0);

        guider->setPHD2(NULL);

        // Receive BLOBs from the driver
        currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), useGuideHead ? "CCD2" : "CCD1");
    }
}*/

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
    int binX=1,binY=1;
    if (targetChip->canBin())
    {
        int binX,binY, maxBinX, maxBinY;
        targetChip->getBinning(&binX, &binY);
        targetChip->getMaxBin(&maxBinX, &maxBinY);

        binningCombo->blockSignals(true);

        binningCombo->clear();

        for (int i=1; i <= maxBinX; i++)
            binningCombo->addItem(QString("%1x%2").arg(i).arg(i));

        binningCombo->setCurrentIndex(binX-1);

        binningCombo->blockSignals(false);
    }

    if (ccdPixelSizeX != -1 && ccdPixelSizeY != -1 && mountAperture != -1 && mountFocalLength != -1)
    {
        guider->setGuiderParams(ccdPixelSizeX, ccdPixelSizeY, mountAperture, mountFocalLength);
        //pmath->setGuiderParameters(ccd_hor_pixel, ccd_ver_pixel, aperture, focal_length);
        //phd2->setCCDMountParams(ccd_hor_pixel, ccd_ver_pixel, focal_length);

        emit guideChipUpdated(targetChip);

        //guider->setTargetChip(targetChip);

        int x,y,w,h;
        if (targetChip->getFrame(&x,&y,&w,&h))
        {
            guider->setFrameParams(x,y,w,h, binX, binY);
            //pmath->setVideoParameters(w, h);
        }

        //guider->setInterface();

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

    ST4Driver=GuideDriver=newST4;

    if (Options::defaultST4Driver().isEmpty() == false)
        setST4(Options::defaultST4Driver());
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
    if (currentCCD == NULL)
        return false;        

    double seqExpose = exposureIN->value();

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        return false;
    }

    //If calibrating, reset frame

    // FIXME


    // TODO Check later, we should reset frame somewhere else
    /*
    if (calibration->getCalibrationStage() == internalCalibration::CAL_CAPTURE_IMAGE)
    {
        targetChip->resetFrame();
        guider->setSubFramed(false);
    }*/

    targetChip->setCaptureMode(FITS_GUIDE);
    targetChip->setFrameType(FRAME_LIGHT);

    if (darkFrameCheck->isChecked())
        targetChip->setCaptureFilter(FITS_NONE);
    else
        targetChip->setCaptureFilter((FITSScale) filterCombo->currentIndex());


    /*if (guider->isGuiding())
    {
        if (guider->isRapidGuide() == false)
            connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

        targetChip->capture(seqExpose);
        return true;
    }

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    targetChip->capture(seqExpose);*/

    guideView->setBaseSize(guideWidget->size());
    setBusy(true);

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

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)), Qt::UniqueConnection);
    targetChip->capture(seqExpose);

    return true;
}

bool Guide::abort()
{
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    setBusy(false);

    switch (state)
    {
        case GUIDE_IDLE:
        case GUIDE_CONNECTED:
        case GUIDE_DISCONNECTED:
        if (targetChip->isCapturing())
            targetChip->abortExposure();
        break;

    case GUIDE_CALIBRATING:
    case GUIDE_CALIBRATION_STAR:
    case GUIDE_CALIBRATION_CAPTURE:
    case GUIDE_GUIDING:
        guider->abort();
    default:
        break;

    }

    return true;
}

void Guide::setBusy(bool enable)
{
    if (enable)
    {
        calibrateB->setEnabled(false);
        guideB->setEnabled(false);
        captureB->setEnabled(false);

        stopB->setEnabled(true);

        pi->startAnimation();
    }
    else
    {
        calibrateB->setEnabled(true);
        captureB->setEnabled(true);

        if (state >= GUIDE_CALIBRATION_SUCESS)
            guideB->setEnabled(true);

        stopB->setEnabled(false);

        pi->stopAnimation();
    }
}

void Guide::newFITS(IBLOB *bp)
{
    INDI_UNUSED(bp);

    //FITSViewer *fv = currentCCD->getViewer();

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    // Do we need to take a dark frame?
    if (Options::guideDarkFrameEnabled())
    {
        int x,y,w,h;
        int binx,biny;

        targetChip->getFrame(&x,&y,&w,&h);
        targetChip->getBinning(&binx,&biny);

        FITSView *currentImage   = targetChip->getImageView(FITS_GUIDE);
        FITSData *darkData       = NULL;
        uint16_t offsetX = x / binx;
        uint16_t offsetY = y / biny;

        darkData = DarkLibrary::Instance()->getDarkFrame(targetChip, exposureIN->value());

        connect(DarkLibrary::Instance(), SIGNAL(darkFrameCompleted(bool)), this, SLOT(setCaptureComplete()));
        connect(DarkLibrary::Instance(), SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));

        if (darkData)
            DarkLibrary::Instance()->subtract(darkData, currentImage, targetChip->getCaptureFilter(), offsetX, offsetY);
        else
        {
            //if (calibration->useAutoStar() == false)
            //KMessageBox::information(NULL, i18n("If the guide camera is not equipped with a shutter, cover the telescope or camera in order to take a dark exposure."), i18n("Dark Exposure"), "dark_exposure_dialog_notification");

            DarkLibrary::Instance()->captureAndSubtract(targetChip, currentImage, exposureIN->value(), offsetX, offsetY);
        }
        return;
    }

    setCaptureComplete();
}

void Guide::setCaptureComplete()
{
    DarkLibrary::Instance()->disconnect(this);

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    FITSView *targetImage = targetChip->getImageView(FITS_GUIDE);

    if (targetImage == NULL)
    {
        if (Options::guideLogging())
            qDebug() << "Guide: guide frame is missing! Capturing again...";

        capture();
        return;
    }

    if (Options::guideLogging())
        qDebug() << "Guide: received guide frame.";

    //FITSData *image_data = targetImage->getImageData();
    //Q_ASSERT(image_data);

    //pmath->setImageView(targetImage);
    //guider->setImageView(targetImage);

    int subBinX=1, subBinY=1;
    targetChip->getBinning(&subBinX, &subBinY);

    // It should be false in case we do not need to process the image for motion
    // which happens when we take an image for auto star selection.
    //if (calibration->setImageView(targetImage) == false)
       // return;

    if (starCenter.x() == 0 && starCenter.y() == 0)
    {
        int x,y,w,h;
        targetChip->getFrame(&x,&y,&w,&h);

        starCenter.setX(w/(2*subBinX));
        starCenter.setY(h/(2*subBinY));
        starCenter.setZ(subBinX);
    }

    syncTrackingBoxPosition();

    switch (state)
    {
        case GUIDE_IDLE:
        case GUIDE_ABORTED:
        case GUIDE_CONNECTED:
        case GUIDE_DISCONNECTED:
        case GUIDE_CALIBRATION_SUCESS:
        case GUIDE_CALIBRATION_ERROR:
            setBusy(false);
        break;

        case GUIDE_CALIBRATION_CAPTURE:
        {
            if (Options::autoStarEnabled())
            {
                autoStarCaptured = selectAutoStar();
                if (autoStarCaptured)
                {
                    appendLogText(i18n("Auto star selected. Starting calibration..."));
                    guider->setStarPosition(starCenter);
                    guider->calibrate();
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
                state = GUIDE_CALIBRATION_STAR;
                emit newStatus(state);
            }
        }
        break;
        case GUIDE_CALIBRATING:
            guider->calibrate();
            break;

        case GUIDE_GUIDING:
            guider->guide();
        break;

        default:
        break;

    }

    /*if (isSuspended)
    {
        if (Options::guideLogging())
            qDebug() << "Guide: Guider is suspended.";

        return;
    }

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
    else if (guider->isGuiding())
    {
        guider->guide();

        if (guider->isGuiding())
            capture();
    }
    else if (calibration->isCalibrating())
    {
        GuideDriver = ST4Driver;
        pmath->performProcessing();
        calibration->processCalibration();

        if (calibration->isCalibrationComplete())
        {
            guider->setReady(true);
            tabWidget->setTabEnabled(1, true);
            //emit guideReady();
        }
    }

    emit newStarPixmap(targetImage->getTrackingBoxPixmap());

    */
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
        QTimer::singleShot( (ra_msecs > dec_msecs ? ra_msecs : dec_msecs) + 100, this, SLOT(capture()));

    return GuideDriver->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
}

bool Guide::sendPulse( GuideDirection dir, int msecs )
{
    if (GuideDriver == NULL || dir==NO_DIR)
        return false;

    if (state == GUIDE_CALIBRATING)
        QTimer::singleShot(msecs+100, this, SLOT(capture()));

    return GuideDriver->doPulse(dir, msecs);

}

QStringList Guide::getST4Devices()
{
    QStringList devices;

    foreach(ISD::ST4* driver, ST4List)
        devices << driver->getDeviceName();

    return devices;
}

/*void Guide::viewerClosed()
{
    pmath->set_image(NULL);
    guider->setImage(NULL);
    calibration->setImage(NULL);
}*/

void Guide::processRapidStarData(ISD::CCDChip *targetChip, double dx, double dy, double fit)
{
    // FIXME
    /*
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

    */

}

void Guide::startRapidGuide()
{

    // FIXME
    /*
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
    */

}

void Guide::stopRapidGuide()
{

    // FIXME
    /*
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    pmath->setRapidGuide(false);

    rapidGuideReticleSet = false;

    currentCCD->disconnect(SIGNAL(newGuideStarData(ISD::CCDChip*,double,double,double)));

    currentCCD->configureRapidGuide(targetChip, false, false, false);

    currentCCD->setRapidGuide(targetChip, false);
    */

}


bool Guide::calibrate()
{
    saveSettings();

    autoStarCaptured = false;

    if (starCenter.isNull() || Options::autoStarEnabled())
    {
        state = GUIDE_CALIBRATION_CAPTURE;
        emit newStatus(state);

        appendLogText(i18n("Capturing image..."));
        capture();
        return true;
    }

    guider->setStarPosition(starCenter);
    bool rc = guider->calibrate();
    if (rc)
        setBusy(true);

    return rc;
}

bool Guide::guide()
{
    saveSettings();

    bool rc = guider->guide();

    if (rc)
    {
        //TODO reset data?
        //driftGraphics->resetData();
    }

    return rc;
}

void Guide::setSuspended(bool enable)
{
    /*
    if (enable == isSuspended || (enable && isGuiding() == false))
        return;

    isSuspended = enable;

    if (isSuspended)
    {
        if (Options::usePHD2Guider())
            phd2->pauseGuiding();
    }
    else
    {
        if (Options::useEkosGuider())
            capture();
        else
            phd2->resumeGuiding();
        //phd2->startGuiding();
    }

    if (isSuspended)
    {
        appendLogText(i18n("Guiding suspended."));
    }
    else
    {
        appendLogText(i18n("Guiding resumed."));
    }
    */
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
    // calibration->setCalibrationTwoAxis(enable);
}

void Guide::setCalibrationAutoStar(bool enable)
{
    //calibration->setCalibrationAutoStar(enable);
}

void Guide::setCalibrationAutoSquareSize(bool enable)
{
    //calibration->setCalibrationAutoSquareSize(enable);
}

void Guide::setCalibrationPulseDuration(int pulseDuration)
{
    // calibration->setCalibrationPulseDuration(pulseDuration);
}

void Guide::setGuideBoxSizeIndex(int boxSize)
{
    //boxSizeCombo->setCurrentIndex(boxSize);
}

void Guide::setGuideAlgorithm(const QString & algorithm)
{
    //guider->setGuideOptions(algorithm, guider->useSubFrame(), guider->useRapidGuide());
}

void Guide::setSubFrameEnabled(bool enable)
{
    //guider->setGuideOptions(guider->getAlgorithm(), enable , guider->useRapidGuide());
}

void Guide::setGuideRapidEnabled(bool enable)
{
    //guider->setGuideOptions(guider->getAlgorithm(), guider->useSubFrame() , enable);
}

void Guide::setDither(bool enable, double value)
{
    //guider->setDither(enable, value);
}

QList<double> Guide::getGuidingDeviation()
{
    QList<double> deviation;

    deviation << guideDeviationRA << guideDeviationDEC;

    return deviation;
}

void Guide::startAutoCalibrateGuiding()
{
    // FIXME
    /*
    if (Options::useEkosGuider())
        connect(calibration, SIGNAL(newStatus(Ekos::GuideState)), this, SLOT(checkAutoCalibrateGuiding(Ekos::GuideState)));
    else
        connect(phd2, SIGNAL(newStatus(Ekos::GuideState)), this, SLOT(checkAutoCalibrateGuiding(Ekos::GuideState)));

    startCalibration();

    */
}

void Guide::checkAutoCalibrateGuiding(Ekos::GuideState state)
{
    // FIXME
    /*
    if (state < GUIDE_CALIBRATION_SUCESS || state > GUIDE_CALIBRATION_ERROR)
        return;

    if (Options::useEkosGuider())
        disconnect(calibration, SIGNAL(newStatus(GuideState)), this, SLOT(checkAutoCalibrateGuiding(GuideState)));
    else
        disconnect(phd2, SIGNAL(newStatus(GuideState)), this, SLOT(checkAutoCalibrateGuiding(GuideState)));

    if (state == GUIDE_CALIBRATION_SUCESS)
    {
        appendLogText(i18n("Auto calibration successful. Starting guiding..."));
        startGuiding();
    }
    else
    {
        appendLogText(i18n("Auto calibration failed."));
    }
    */
}

void Guide::setStatus(Ekos::GuideState newState)
{
    if (newState == state)
        return;

    state = newState;

    switch (state)
    {
        case GUIDE_CALIBRATION_SUCESS:
            setBusy(false);
        break;

          case GUIDE_CALIBRATION_ERROR:
            setBusy(false);
        break;

        case GUIDE_GUIDING:
            appendLogText(i18n("Autoguiding started."));
            setBusy(true);
        break;

    default:
        break;
    }

    emit newStatus(newState);
}

void Guide::updateCCDBin(int index)
{
    // FIXME
    /*
    if (currentCCD == NULL && Options::usePHD2Guider())
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    targetChip->setBinning(index+1, index+1);

    */

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

void Guide::checkExposureValue(ISD::CCDChip *targetChip, double exposure, IPState state)
{
    // FIXME
    /*
    INDI_UNUSED(exposure);

    if (state == IPS_ALERT && (guider->isGuiding() || guider->isDithering() || calibration->isCalibrating()))
    {
        appendLogText(i18n("Exposure failed. Restarting exposure..."));
        targetChip->capture(exposureIN->value());
    }
    */
}

void Guide::setDarkFrameEnabled(bool enable)
{
    // FIXME
    /*
    Options::setUseGuideDarkFrame(enable);
*/
    /*if (enable && calibration && calibration->useAutoStar())
        appendLogText(i18n("Warning: In auto mode, you will not be asked to cover cameras unequipped with shutters in order to capture a dark frame. The dark frame capture will proceed without warning."
                           " You can capture dark frames with auto mode off and they shall be saved in the dark library for use when ever needed."));*/
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
            boxSizeCombo->setCurrentIndex(boxSizeCombo->currentIndex()-1);
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

bool Guide::setGuideType(int type)
{
    // FIXME
    return false;
}

void Guide::updateTrackingBoxSize(int currentIndex)
{
    Options::setGuideSquareSizeIndex(currentIndex);

    syncTrackingBoxPosition();
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
    //Options::setEnableRAGuide(enable);
}

void Guide::onEnableDirDEC(bool enable)
{
    //Options::setEnableDECGuide(enable);
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
    // FIXME
    /*
    QObject *obj = sender();

    if (northControlCheck == dynamic_cast<QCheckBox*>(obj))
    {
        Options::setEnableNorthDECGuide(enable);
    }
    else if (southControlCheck == dynamic_cast<QCheckBox*>(obj))
    {
        Options::setEnableSouthDECGuide(enable);
    }
    else if (westControlCheck == dynamic_cast<QCheckBox*>(obj))
    {
        Options::setEnableWestRAGuide(enable);
    }
    else if (eastControlCheck == dynamic_cast<QCheckBox*>(obj))
    {
        Options::setEnableEastRAGuide(enable);
    }

    */
}

void Guide::onRapidGuideChanged(bool enable)
{
    // FIXME
    /*
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

        */

}

void Guide::loadSettings()
{
    // Exposure
    exposureIN->setValue(Options::guideExposure());
    // Box Size
    boxSizeCombo->setCurrentIndex(Options::guideSquareSizeIndex());
    // Dark frame?
    darkFrameCheck->setChecked(Options::guideDarkFrameEnabled());
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

void Guide::refreshColorScheme()
{
    // Drift color legend
    RADriftLabel->setStyleSheet(QString("background-color: %1;").arg(KStarsData::Instance()->colorScheme()->colorNamed("RAGuideError").name(QColor::HexRgb)));
    DEDriftLabel->setStyleSheet(QString("background-color: %1;").arg(KStarsData::Instance()->colorScheme()->colorNamed("DEGuideError").name(QColor::HexRgb)));
}

void Guide::setTrackingStar(int x, int y)
{
    QVector3D newStarPosition(x,y, -1);
    setStarPosition(newStarPosition, true);

    if (state == GUIDE_CALIBRATION_STAR)
    {
        guider->setStarPosition(newStarPosition);
        guider->calibrate();
    }
}

void Guide::setAxisDelta(double ra, double de)
{
    static QTime time(QTime::currentTime());
    // calculate two new data points:
    double key = time.elapsed()/1000.0; // time elapsed since start of demo, in seconds

    driftGraph->graph(0)->addData(key, ra);
    driftGraph->graph(1)->addData(key, de);

    driftGraph->xAxis->setRange(key, 8, Qt::AlignRight);
    driftGraph->replot();
    //driftGraphics->addPoint(ra, de);
    //driftGraphics->update();



    l_DeltaRA->setText(QString::number(ra, 'f', 2));
    l_DeltaDEC->setText(QString::number(de, 'f', 2));

    emit newAxisDelta(ra,de);
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

}


