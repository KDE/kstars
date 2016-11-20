/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "focus.h"
#include "Options.h"

#include <KMessageBox>
#include <KLocalizedString>
#include <KPlotting/KPlotWidget>
#include <KPlotting/KPlotObject>
#include <KPlotting/KPlotAxis>

#include <KNotifications/KNotification>

#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "indi/indifilter.h"

#include "auxiliary/kspaths.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsview.h"
#include "ekos/ekosmanager.h"
#include "ekos/auxiliary/darklibrary.h"

#include "kstars.h"
#include "focusadaptor.h"

#include <basedevice.h>

#define MAXIMUM_ABS_ITERATIONS      30
#define MAXIMUM_RESET_ITERATIONS    2
#define DEFAULT_SUBFRAME_DIM        128
#define AUTO_STAR_TIMEOUT           45000
#define MINIMUM_PULSE_TIMER         32
#define MAX_RECAPTURE_RETRIES       3

namespace Ekos
{

Focus::Focus()
{
    setupUi(this);

    new FocusAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Focus",  this);

    currentFocuser = NULL;
    currentCCD     = NULL;
    currentFilter  = NULL;
    filterName     = NULL;
    filterSlot     = NULL;

    canAbsMove        = false;
    canRelMove        = false;
    canTimerMove      = false;
    inAutoFocus       = false;
    inFocusLoop       = false;
    captureInProgress = false;
    inSequenceFocus   = false;
    starSelected      = false;
    //frameModified     = false;
    subFramed         = false;
    resetFocus        = false;    
    filterPositionPending= false;

    waitStarSelectTimer.setInterval(AUTO_STAR_TIMEOUT);
    connect(&waitStarSelectTimer, SIGNAL(timeout()), this, SLOT(checkAutoStarTimeout()));

    rememberUploadMode = ISD::CCD::UPLOAD_CLIENT;
    HFRInc =0;
    noStarCount=0;
    reverseDir = false;
    initialFocuserAbsPosition = -1;

    focusAlgorithm = ALGORITHM_GRADIENT;

    state = Ekos::FOCUS_IDLE;

    pulseDuration = 1000;

    resetFocusIteration=0;
    //fy=fw=fh=0;
    //orig_x = orig_y = orig_w = orig_h =-1;
    lockedFilterIndex=-1;
    maxHFR=1;
    minimumRequiredHFR = -1;
    currentFilterIndex=-1;
    minPos=1e6;
    maxPos=0;
    frameNum=0;

    showFITSViewerB->setIcon(QIcon::fromTheme("kstars_fitsviewer", QIcon(":/icons/breeze/default/kstars_fitsviewer.svg")));
    connect(showFITSViewerB, SIGNAL(clicked()), this, SLOT(showFITSViewer()));

    connect(startFocusB, SIGNAL(clicked()), this, SLOT(start()));
    connect(stopFocusB, SIGNAL(clicked()), this, SLOT(checkStopFocus()));

    connect(focusOutB, SIGNAL(clicked()), this, SLOT(focusOut()));
    connect(focusInB, SIGNAL(clicked()), this, SLOT(focusIn()));

    connect(captureB, SIGNAL(clicked()), this, SLOT(capture()));    

    connect(startLoopB, SIGNAL(clicked()), this, SLOT(startFraming()));

    connect(kcfg_subFrame, SIGNAL(toggled(bool)), this, SLOT(toggleSubframe(bool)));

    connect(resetFrameB, SIGNAL(clicked()), this, SLOT(resetFrame()));

    connect(CCDCaptureCombo, SIGNAL(activated(QString)), this, SLOT(setDefaultCCD(QString)));
    connect(CCDCaptureCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(checkCCD(int)));

    connect(focuserCombo, SIGNAL(activated(int)), this, SLOT(checkFocuser(int)));
    connect(FilterCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkFilter(int)));
    connect(FilterPosCombo, SIGNAL(activated(int)), this, SLOT(updateFilterPos(int)));
    connect(lockFilterCheck, SIGNAL(toggled(bool)), this, SLOT(filterLockToggled(bool)));
    connect(setAbsTicksB, SIGNAL(clicked()), this, SLOT(setAbsoluteFocusTicks()));
    connect(binningCombo, SIGNAL(activated(int)), this, SLOT(setActiveBinning(int)));
    connect(focusBoxSize, SIGNAL(valueChanged(int)), this, SLOT(updateBoxSize(int)));

    focusAlgorithm = static_cast<StarAlgorithm>(Options::focusAlgorithm());
    focusAlgorithmCombo->setCurrentIndex(focusAlgorithm);

    connect(focusAlgorithmCombo, static_cast<void (QComboBox::*) (int)>(&QComboBox::activated), this, [&](int index)
    {
        focusAlgorithm=static_cast<StarAlgorithm>(index);
        thresholdSpin->setEnabled(focusAlgorithm == ALGORITHM_THRESHOLD);
        Options::setFocusAlgorithm(index);
    });

    activeBin=Options::focusXBin();
    binningCombo->setCurrentIndex(activeBin-1);

    connect(clearDataB, SIGNAL(clicked()) , this, SLOT(clearDataPoints()));

    lastFocusDirection = FOCUS_NONE;

    focusType = FOCUS_MANUAL;

    profilePlot->setBackground(QBrush(Qt::black));
    profilePlot->xAxis->setBasePen(QPen(Qt::white, 1));
    profilePlot->yAxis->setBasePen(QPen(Qt::white, 1));
    profilePlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    profilePlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    profilePlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    profilePlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    profilePlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    profilePlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    profilePlot->xAxis->setBasePen(QPen(Qt::white, 1));
    profilePlot->yAxis->setBasePen(QPen(Qt::white, 1));
    profilePlot->xAxis->setTickPen(QPen(Qt::white, 1));
    profilePlot->yAxis->setTickPen(QPen(Qt::white, 1));
    profilePlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    profilePlot->yAxis->setSubTickPen(QPen(Qt::white, 1));
    profilePlot->xAxis->setTickLabelColor(Qt::white);
    profilePlot->yAxis->setTickLabelColor(Qt::white);
    profilePlot->xAxis->setLabelColor(Qt::white);
    profilePlot->yAxis->setLabelColor(Qt::white);

    firstGaus   = NULL;

    currentGaus = profilePlot->addGraph();
    currentGaus->setLineStyle(QCPGraph::lsLine);
    currentGaus->setPen(QPen(Qt::red, 2));

    lastGaus = profilePlot->addGraph();
    lastGaus->setLineStyle(QCPGraph::lsLine);
    QPen pen(Qt::darkGreen);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(2);
    lastGaus->setPen(pen);

    HFRPlot->setBackground(QBrush(Qt::black));

    HFRPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    HFRPlot->yAxis->setBasePen(QPen(Qt::white, 1));

    HFRPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    HFRPlot->yAxis->setTickPen(QPen(Qt::white, 1));

    HFRPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    HFRPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));

    HFRPlot->xAxis->setTickLabelColor(Qt::white);
    HFRPlot->yAxis->setTickLabelColor(Qt::white);

    HFRPlot->xAxis->setLabelColor(Qt::white);
    HFRPlot->yAxis->setLabelColor(Qt::white);

    HFRPlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    HFRPlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    HFRPlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    HFRPlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    HFRPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    HFRPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);

    HFRPlot->yAxis->setLabel(i18n("HFR"));

    v_graph = HFRPlot->addGraph();
    v_graph->setLineStyle(QCPGraph::lsNone);
    v_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::white, Qt::red, 3));

    resetButtons();

    appendLogText(i18n("Idle."));

    foreach(QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    filterCombo->setCurrentIndex(Options::focusEffect());
    defaultScale = static_cast<FITSScale>(Options::focusEffect());
    connect(filterCombo, SIGNAL(activated(int)), this, SLOT(filterChangeWarning(int)));

    exposureIN->setValue(Options::focusExposure());
    toleranceIN->setValue(Options::focusTolerance());
    stepIN->setValue(Options::focusTicks());
    autoStarCheck->setChecked(Options::focusAutoStarEnabled());
    focusBoxSize->setValue(Options::focusBoxSize());
    maxTravelIN->setValue(Options::focusMaxTravel());
    kcfg_subFrame->setChecked(Options::focusSubFrame());
    suspendGuideCheck->setChecked(Options::suspendGuiding());
    lockFilterCheck->setChecked(Options::lockFocusFilter());
    darkFrameCheck->setChecked(Options::useFocusDarkFrame());
    thresholdSpin->setValue(Options::focusThreshold());
    //focusFramesSpin->setValue(Options::focusFrames());

    connect(thresholdSpin, SIGNAL(valueChanged(double)), this, SLOT(setThreshold(double)));
    //connect(focusFramesSpin, SIGNAL(valueChanged(int)), this, SLOT(setFrames(int)));

    focusView = new FITSView(focusingWidget, FITS_FOCUS);
    focusView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    focusView->setBaseSize(focusingWidget->size());
    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(focusView);
    focusingWidget->setLayout(vlayout);
    connect(focusView, SIGNAL(trackingStarSelected(int,int)), this, SLOT(focusStarSelected(int,int)), Qt::UniqueConnection);

    focusView->setStarsEnabled(true);

    // Reset star center on auto star check toggle
    connect(autoStarCheck, &QCheckBox::toggled, this, [&](bool enabled){if (enabled) { starCenter = QVector3D(); starSelected=false; focusView->setTrackingBox(QRect());}});
}

Focus::~Focus()
{
    //qDeleteAll(HFRAbsolutePoints);
   // HFRAbsolutePoints.clear();
}

void Focus::resetFrame()
{
    if (currentCCD)
    {
        ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

        if (targetChip)
        {
            //fx=fy=fw=fh=0;
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
            //targetChip->setFocusFrame(0,0,0,0);

            starSelected = false;
            starCenter = QVector3D();            
            subFramed = false;

            focusView->setTrackingBox(QRect());
        }
    }
}

bool Focus::setCCD(QString device)
{
    for (int i=0; i < CCDCaptureCombo->count(); i++)
        if (device == CCDCaptureCombo->itemText(i))
        {
            CCDCaptureCombo->setCurrentIndex(i);
            return true;
        }

    return false;
}

void Focus::setDefaultCCD(QString ccd)
{
    Options::setDefaultFocusCCD(ccd);
}

void Focus::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
    {
        ccdNum = CCDCaptureCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum >=0 && ccdNum <= CCDs.count())
    {
        currentCCD = CCDs.at(ccdNum);

        ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
        if (targetChip)
        {
            targetChip->setImageView(focusView, FITS_FOCUS);

            binningCombo->setEnabled(targetChip->canBin());
            kcfg_subFrame->setEnabled(targetChip->canSubframe());
            if (targetChip->canBin())
            {
                int subBinX=1,subBinY=1;
                binningCombo->clear();
                targetChip->getMaxBin(&subBinX, &subBinY);
                for (int i=1; i <= subBinX; i++)
                    binningCombo->addItem(QString("%1x%2").arg(i).arg(i));

                binningCombo->setCurrentIndex(activeBin-1);

            }

            QStringList isoList = targetChip->getISOList();
            ISOCombo->clear();

            if (isoList.isEmpty())
            {
                ISOCombo->setEnabled(false);
                ISOLabel->setEnabled(false);
            }
            else
            {
                ISOCombo->setEnabled(true);
                ISOLabel->setEnabled(true);
                ISOCombo->addItems(isoList);
                ISOCombo->setCurrentIndex(targetChip->getISOIndex());
            }

        }
    }

    syncCCDInfo();
}

void Focus::syncCCDInfo()
{
    if (currentCCD == NULL)
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    kcfg_subFrame->setEnabled(targetChip->canSubframe());

    if (frameSettings.contains(targetChip) == false)
    {
        int x,y,w,h;
        if (targetChip->getFrame(&x, &y, &w, &h))
        {
            int binx=1,biny=1;
            targetChip->getBinning(&binx, &biny);
            if (w > 0 && h > 0)
            {
                QVariantMap settings;

                settings["x"] = x;
                settings["y"] = y;
                settings["w"] = w;
                settings["h"] = h;
                settings["binx"] = binx;
                settings["biny"] = biny;

                frameSettings[targetChip] = settings;
            }
        }
    }
}

void Focus::addFilter(ISD::GDInterface *newFilter)
{
    foreach(ISD::GDInterface *filter, Filters)
    {
        if (!strcmp(filter->getDeviceName(), newFilter->getDeviceName()))
            return;
    }

    FilterCaptureLabel->setEnabled(true);
    FilterCaptureCombo->setEnabled(true);
    FilterPosLabel->setEnabled(true);
    FilterPosCombo->setEnabled(true);
    lockFilterCheck->setEnabled(true);

    FilterCaptureCombo->addItem(newFilter->getDeviceName());

    Filters.append(static_cast<ISD::Filter *>(newFilter));

    checkFilter(0);

    FilterCaptureCombo->setCurrentIndex(0);

}

bool Focus::setFilter(QString device, int filterSlot)
{
    bool deviceFound=false;

    for (int i=0; i < FilterCaptureCombo->count(); i++)
        if (device == FilterCaptureCombo->itemText(i))
        {
            checkFilter(i);
            deviceFound = true;
            break;
        }

    if (deviceFound == false)
        return false;

    if (filterSlot < FilterCaptureCombo->count())
        FilterCaptureCombo->setCurrentIndex(filterSlot);

    return true;
}

void Focus::checkFilter(int filterNum)
{
    if (filterNum == -1)
    {
        filterNum = FilterCaptureCombo->currentIndex();
        if (filterNum == -1)
            return;
    }

    QStringList filterAlias = Options::filterAlias();

    if (filterNum <= Filters.count())
        currentFilter = Filters.at(filterNum);

    FilterPosCombo->clear();

    filterName   = currentFilter->getBaseDevice()->getText("FILTER_NAME");
    filterSlot = currentFilter->getBaseDevice()->getNumber("FILTER_SLOT");

    if (filterSlot == NULL)
    {
        KMessageBox::error(0, i18n("Unable to find FILTER_SLOT property in driver %1", currentFilter->getBaseDevice()->getDeviceName()));
        return;
    }

    currentFilterIndex = filterSlot->np[0].value - 1;

    for (int i=0; i < filterSlot->np[0].max; i++)
    {
        QString item;

        if (filterName != NULL && (i < filterName->ntp))
            item = filterName->tp[i].text;
        else if (i < filterAlias.count() && filterAlias[i].isEmpty() == false)
            item = filterAlias.at(i);
        else
            item = QString("Filter_%1").arg(i+1);

        FilterPosCombo->addItem(item);

    }

    if (lockFilterCheck->isChecked() == false)
        FilterPosCombo->setCurrentIndex( currentFilterIndex);
    else
    {
        if (lockedFilterIndex < 0)
        {
            //lockedFilterIndex = currentFilterIndex;
            lockedFilterIndex = Options::lockFocusFilterIndex();
            emit filterLockUpdated(currentFilter, lockedFilterIndex);
        }
        FilterPosCombo->setCurrentIndex(lockedFilterIndex);
    }

    // If we are waiting to change the filter wheel, let's check if the condition is now met.
    if (filterPositionPending)
    {
        if (lockedFilterIndex == currentFilterIndex)
        {
            filterPositionPending = false;
            capture();
        }
    }

}

void Focus::filterLockToggled(bool enable)
{
    if (enable)
    {
        lockedFilterIndex = FilterPosCombo->currentIndex();
        if (lockedFilterIndex >= 0)
            Options::setLockFocusFilterIndex(lockedFilterIndex);
        emit filterLockUpdated(currentFilter, lockedFilterIndex);
    }
    else if (filterSlot != NULL)
    {
        FilterPosCombo->setCurrentIndex(filterSlot->np[0].value-1);
        emit filterLockUpdated(NULL, 0);
    }
}

void Focus::updateFilterPos(int index)
{
    if (lockFilterCheck->isChecked() == true)
    {
        lockedFilterIndex = index;
        Options::setLockFocusFilterIndex(lockedFilterIndex);
        emit filterLockUpdated(currentFilter, lockedFilterIndex);
    }
}

void Focus::addFocuser(ISD::GDInterface *newFocuser)
{
    focuserCombo->addItem(newFocuser->getDeviceName());

    Focusers.append(static_cast<ISD::Focuser*>(newFocuser));

    currentFocuser = static_cast<ISD::Focuser *> (newFocuser);

    checkFocuser();
}

bool Focus::setFocuser(QString device)
{
    for (int i=0; i < focuserCombo->count(); i++)
        if (device == focuserCombo->itemText(i))
        {
            checkFocuser(i);
            return true;
        }

    return false;
}

void Focus::checkFocuser(int FocuserNum)
{
    if (FocuserNum == -1)
        FocuserNum = focuserCombo->currentIndex();

    if (FocuserNum == -1)
        return;

    if (FocuserNum <= Focusers.count())
        currentFocuser = Focusers.at(FocuserNum);

    if (currentFocuser->canAbsMove())
    {
        canAbsMove = true;
        getAbsFocusPosition();

        absTicksSpin->setEnabled(true);
        setAbsTicksB->setEnabled(true);
    }
    else
    {
        canAbsMove = false;
        absTicksSpin->setEnabled(false);
        setAbsTicksB->setEnabled(false);
    }

    if (currentFocuser->canRelMove())
    {
        // We pretend this is an absolute focuser
        canRelMove = true;
        currentPosition = 50000;
        absMotionMax  = 100000;
        absMotionMin  = 0;
    }

    canTimerMove = currentFocuser->canTimerMove();

    focusType = (canRelMove || canAbsMove || canTimerMove) ? FOCUS_AUTO : FOCUS_MANUAL;

    connect(currentFocuser, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processFocusNumber(INumberVectorProperty*)), Qt::UniqueConnection);    

    resetButtons();

    //if (!inAutoFocus && !inFocusLoop && !captureInProgress && !inSequenceFocus)
      //  emit autoFocusFinished(true, -1);
}

void Focus::addCCD(ISD::GDInterface *newCCD)
{
    if (CCDs.contains(static_cast<ISD::CCD *>(newCCD)))
         return;

    CCDs.append(static_cast<ISD::CCD *>(newCCD));

    CCDCaptureCombo->addItem(newCCD->getDeviceName());
}

void Focus::getAbsFocusPosition()
{
    INumberVectorProperty *absMove = NULL;
    if (canAbsMove)
    {
        absMove = currentFocuser->getBaseDevice()->getNumber("ABS_FOCUS_POSITION");

        if (absMove)
        {
           currentPosition = absMove->np[0].value;
           absMotionMax  = absMove->np[0].max;
           absMotionMin  = absMove->np[0].min;

           absTicksSpin->setMinimum(absMove->np[0].min);
           absTicksSpin->setMaximum(absMove->np[0].max);
           absTicksSpin->setSingleStep(absMove->np[0].step);

           absTicksSpin->setValue(currentPosition);
        }
    }

}

void Focus::start()
{
    if (currentCCD == NULL)
    {
        appendLogText(i18n("No CCD connected."));
        return;
    }

    lastFocusDirection = FOCUS_NONE;

    waitStarSelectTimer.stop();

    lastHFR = 0;

    if (canAbsMove)
    {
        absIterations = 0;
        getAbsFocusPosition();
        pulseDuration = stepIN->value();
    }
    else if (canRelMove)
    {
        appendLogText(i18n("Setting dummy central position to 50000"));
        absIterations = 0;
        pulseDuration = stepIN->value();
        currentPosition = 50000;
        absMotionMax  = 100000;
        absMotionMin  = 0;
    }
    else
    {
      pulseDuration=stepIN->value();

      if (pulseDuration <= MINIMUM_PULSE_TIMER)
      {
          appendLogText(i18n("Starting pulse step is too low. Increase the step size to %1 or higher...", MINIMUM_PULSE_TIMER*5));
          return;
      }
    }

    inAutoFocus = true;    
    frameNum=0;

    resetButtons();

    reverseDir = false;

    /*if (fw > 0 && fh > 0)
        starSelected= true;
    else
        starSelected= false;*/

    clearDataPoints();

    if (firstGaus)
    {
        profilePlot->removeGraph(firstGaus);
        firstGaus = NULL;
    }

    Options::setFocusTicks(stepIN->value());
    Options::setFocusTolerance(toleranceIN->value());
    Options::setFocusExposure(exposureIN->value());
    Options::setFocusXBin(activeBin);
    Options::setFocusMaxTravel(maxTravelIN->value());
    Options::setFocusBoxSize(focusBoxSize->value());
    Options::setFocusSubFrame(kcfg_subFrame->isChecked());
    Options::setFocusAutoStarEnabled(autoStarCheck->isChecked());
    Options::setSuspendGuiding(suspendGuideCheck->isChecked());
    Options::setLockFocusFilter(lockFilterCheck->isChecked());
    Options::setUseFocusDarkFrame(darkFrameCheck->isChecked());

    if (Options::focusLogging())
        qDebug() << "Focus: Starting focus with box size: " << focusBoxSize->value() << " Step Size: " <<  stepIN->value() << " Threshold: " << thresholdSpin->value() << " Tolerance: "  << toleranceIN->value()
                 << " Frames: " << 1 /*focusFramesSpin->value()*/ << " Maximum Travel: " << maxTravelIN->value();

    if (autoStarCheck->isChecked())
        appendLogText(i18n("Autofocus in progress..."));
    else
        appendLogText(i18n("Please wait until image capture is complete..."));

    if (suspendGuideCheck->isChecked())
         emit suspendGuiding();

    //emit statusUpdated(true);
    state = Ekos::FOCUS_PROGRESS;
    emit newStatus(state);

    // Denoise with median filter
    //defaultScale = FITS_MEDIAN;

    capture();
}

void Focus::checkStopFocus()
{
    if (inSequenceFocus == true)
    {
        inSequenceFocus = false;
        setAutoFocusResult(false);
    }

    if (captureInProgress && inAutoFocus == false && inFocusLoop == false)
    {
        captureB->setEnabled(true);
        stopFocusB->setEnabled(false);

        appendLogText(i18n("Capture aborted."));
    }

    abort();
}

void Focus::abort()
{
    stop(true);
}

void Focus::stop(bool aborted)
{
    if (Options::focusLogging())
        qDebug() << "Focus: Stopppig Focus";

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    inAutoFocus = false;
    inFocusLoop = false;
    starSelected= false;
    minimumRequiredHFR    = -1;
    noStarCount = 0;
    frameNum=0;
    //maxHFR=1;

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    if (rememberUploadMode != currentCCD->getUploadMode())
        currentCCD->setUploadMode(rememberUploadMode);

    targetChip->abortExposure();

    //resetFrame();

    resetButtons();

    absIterations = 0;
    HFRInc=0;
    reverseDir = false;

    //emit statusUpdated(false);
    if (aborted)
    {
        state = Ekos::FOCUS_ABORTED;
        emit newStatus(state);
    }
}

void Focus::capture()
{
    if (currentCCD == NULL)
    {
        appendLogText(i18n("No CCD connected."));
        return;
    }

    waitStarSelectTimer.stop();

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    double seqExpose = exposureIN->value();

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        return;
    }

    if (currentFilter != NULL && lockFilterCheck->isChecked())
    {
        if (currentFilter->isConnected() == false)
        {
            appendLogText(i18n("Error: Lost connection to filter wheel."));
            return;
        }

        if (lockedFilterIndex != currentFilterIndex)
        {
            int lockedFilterPosition = lockedFilterIndex + 1;
            filterPositionPending = true;
            appendLogText(i18n("Changing filter to %1", FilterPosCombo->currentText()));
            currentFilter->runCommand(INDI_SET_FILTER, &lockedFilterPosition);
            return;
        }
    }

    if (currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
    {
        rememberUploadMode = ISD::CCD::UPLOAD_LOCAL;
        currentCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
    }

    targetChip->setBinning(activeBin, activeBin);

    targetChip->setCaptureMode(FITS_FOCUS);

    // Always disable filtering if using a dark frame and then re-apply after subtraction. TODO: Implement this in capture and guide and align
    if (darkFrameCheck->isChecked())
        targetChip->setCaptureFilter(FITS_NONE);
    else
        targetChip->setCaptureFilter(defaultScale);

    if (ISOCombo->isEnabled() && ISOCombo->currentIndex() != -1 && targetChip->getISOIndex() != ISOCombo->currentIndex())
        targetChip->setISOIndex(ISOCombo->currentIndex());

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    targetChip->setFrameType(FRAME_LIGHT);

    if (frameSettings.contains(targetChip))
    {
        QVariantMap settings = frameSettings[targetChip];
        targetChip->setFrame(settings["x"].toInt(), settings["y"].toInt(), settings["w"].toInt(), settings["h"].toInt());
        settings["binx"] = activeBin;
        settings["biny"] = activeBin;
        frameSettings[targetChip] = settings;
    }

    captureInProgress = true;

    focusView->setBaseSize(focusingWidget->size());

    targetChip->capture(seqExpose);

    if (inFocusLoop == false)
    {
        appendLogText(i18n("Capturing image..."));

        if (inAutoFocus == false)
        {
            captureB->setEnabled(false);
            stopFocusB->setEnabled(true);
        }
    }
}

void Focus::focusIn(int ms)
{
  if (currentFocuser == NULL)
       return;

  if (currentFocuser->isConnected() == false)
  {
            appendLogText(i18n("Error: Lost connection to Focuser."));
            return;
  }

    if (ms == -1)
        ms = stepIN->value();

    if (Options::focusLogging())
        qDebug() << "Focus: Focus in (" << ms << ")" ;

    lastFocusDirection = FOCUS_IN;

     currentFocuser->focusIn();

     if (canAbsMove)
     {
         currentFocuser->moveAbs(currentPosition-ms);
         appendLogText(i18n("Focusing inward..."));
     }
     else if (canRelMove)
     {
         currentFocuser->moveRel(ms);
         appendLogText(i18n("Focusing inward..."));
     }
     else
     {
       currentFocuser->moveByTimer(ms);
       appendLogText(i18n("Focusing inward by %1 ms...", ms));
     }



}

void Focus::focusOut(int ms)
{
    if (currentFocuser == NULL)
         return;

    if (currentFocuser->isConnected() == false)
    {
       appendLogText(i18n("Error: Lost connection to Focuser."));
       return;
    }

    lastFocusDirection = FOCUS_OUT;

    if (ms == -1)
        ms = stepIN->value();

    if (Options::focusLogging())
        qDebug() << "Focus: Focus out (" << ms << ")" ;

     currentFocuser->focusOut();

    if (canAbsMove)
    {
           currentFocuser->moveAbs(currentPosition+ms);
           appendLogText(i18n("Focusing outward..."));
    }
    else if (canRelMove)
    {
            currentFocuser->moveRel(ms);
            appendLogText(i18n("Focusing outward..."));
    }
    else
    {
            currentFocuser->moveByTimer(ms);
            appendLogText(i18n("Focusing outward by %1 ms...", ms));
    }
}

void Focus::newFITS(IBLOB *bp)
{
    if (bp == NULL)
    {
        capture();
        return;
    }

    // Ignore guide head if there is any.
    if (!strcmp(bp->name, "CCD2"))
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    if (darkFrameCheck->isChecked())
    {        
        FITSData *darkData       = NULL;
        QVariantMap settings = frameSettings[targetChip];
        uint16_t offsetX = settings["x"].toInt() / settings["binx"].toInt();
        uint16_t offsetY = settings["y"].toInt() / settings["biny"].toInt();

        darkData = DarkLibrary::Instance()->getDarkFrame(targetChip, exposureIN->value());

        connect(DarkLibrary::Instance(), SIGNAL(darkFrameCompleted(bool)), this, SLOT(setCaptureComplete()));
        connect(DarkLibrary::Instance(), SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));

        targetChip->setCaptureFilter(defaultScale);

        if (darkData)
            DarkLibrary::Instance()->subtract(darkData, focusView, defaultScale, offsetX, offsetY);
        else
        {
            bool rc = DarkLibrary::Instance()->captureAndSubtract(targetChip, focusView, exposureIN->value(), offsetX, offsetY);
            darkFrameCheck->setChecked(rc);
        }

        return;
    }

    setCaptureComplete();
}

void Focus::setCaptureComplete()
{
    DarkLibrary::Instance()->disconnect(this);

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    int subBinX=1, subBinY=1;
    targetChip->getBinning(&subBinX, &subBinY);

    // Always reset capture mode to NORMAL
    // JM 2016-09-28: Disable setting back to FITS_NORMAL as it might be causing issues. Each module should set capture module separately.
    //targetChip->setCaptureMode(FITS_NORMAL);

    syncTrackingBoxPosition();

    //connect(targetImage, SIGNAL(trackingStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)), Qt::UniqueConnection);

    if (inFocusLoop == false)
        appendLogText(i18n("Image received."));

    if (captureInProgress && inFocusLoop == false && inAutoFocus==false)
    {
        captureB->setEnabled(true);
        stopFocusB->setEnabled(false);
        currentCCD->setUploadMode(rememberUploadMode);
    }

    captureInProgress = false;

    FITSData *image_data = focusView->getImageData();

    starPixmap = focusView->getTrackingBoxPixmap();
    emit newStarPixmap(starPixmap);

    // If we're not framing, let's try to detect stars
    if (inFocusLoop == false || (inFocusLoop && focusView->isTrackingBoxEnabled()))
    {
        if (image_data->areStarsSearched() == false)
        {
            //if (starSelected == false && autoStarCheck->isChecked() && subFramed == false)
            //if (autoStarCheck->isChecked() && subFramed == false)
                //focusView->findStars(ALGORITHM_CENTROID);

            currentHFR = -1;

            if (starSelected)
            {
                focusView->findStars(focusAlgorithm);
                focusView->updateFrame();
                currentHFR= image_data->getHFR(HFR_MAX);
            }
            else
            {
                focusView->findStars(ALGORITHM_CENTROID);
                focusView->updateFrame();
                currentHFR= image_data->getHFR(HFR_MAX);
            }
            /*
            if (subFramed && focusView->isTrackingBoxEnabled())
            {
                focusView->findStars(focusAlgorithm);
                focusView->updateFrame();
                currentHFR= image_data->getHFR(HFR_MAX);
            }
            else if (autoStarCheck->isChecked())
            {
                focusView->findStars(ALGORITHM_CENTROID);
                focusView->updateFrame();
                currentHFR= image_data->getHFR(HFR_MAX);
            }*/
        }

        if (Options::focusLogging())
            qDebug() << "Focus newFITS #" << frameNum+1 << ": Current HFR " << currentHFR;

        HFRFrames[frameNum++] = currentHFR;

        if (frameNum >= 1 /*focusFramesSpin->value()*/)
        {
            currentHFR=0;
            for (int i=0; i < frameNum; i++)
                currentHFR+= HFRFrames[i];

            currentHFR /= frameNum;
            frameNum =0;
        }
        else
        {
            capture();
            return;
        }

        emit newHFR(currentHFR);

        QString HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

        if (/*focusType == FOCUS_MANUAL && */ lastHFR == -1)
                appendLogText(i18n("FITS received. No stars detected."));

        HFROut->setText(HFRText);

        if (currentHFR > 0)
        {
            // Center tracking box around selected star
            //if (starSelected && inAutoFocus)
            if (starCenter.isNull() == false && (inAutoFocus || minimumRequiredHFR >= 0))
            {
                Edge *maxStarHFR = image_data->getMaxHFRStar();

                if (maxStarHFR)
                {
                    starSelected=true;
                    starCenter.setX(qMax(0, static_cast<int>(maxStarHFR->x)));
                    starCenter.setY(qMax(0, static_cast<int>(maxStarHFR->y)));

                    syncTrackingBoxPosition();
                }
            }

            if (currentHFR > maxHFR)
                maxHFR = currentHFR;            

            if (inFocusLoop || (inAutoFocus && canAbsMove == false && canRelMove == false))
            {
                if (hfr_position.empty())
                    hfr_position.append(1);
                else
                    hfr_position.append(hfr_position.last()+1);
                hfr_value.append(currentHFR);

                drawHFRPlot();
            }
        }
    }

    // If just framing, let's capture again
    if (inFocusLoop)
    {
        capture();
        return;
    }

    //if (starSelected == false)
    if (starCenter.isNull())
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

        if (autoStarCheck->isChecked())
        {
            Edge *maxStar = image_data->getMaxHFRStar();
            if (maxStar == NULL)
            {
                appendLogText(i18n("Failed to automatically select a star. Please select a star manually."));                                

                //if (fw == 0 || fh == 0)
                    //targetChip->getFrame(&fx, &fy, &fw, &fh);

                //targetImage->setTrackingBox(QRect((fw-focusBoxSize->value())/2, (fh-focusBoxSize->value())/2, focusBoxSize->value(), focusBoxSize->value()));
                focusView->setTrackingBox(QRect(w-focusBoxSize->value()/(subBinX*2), h-focusBoxSize->value()/(subBinY*2), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY));
                focusView->setTrackingBoxEnabled(true);

                state = Ekos::FOCUS_WAITING;
                emit newStatus(state);

                waitStarSelectTimer.start();

                return;
            }

            if (subFramed == false && kcfg_subFrame->isEnabled() && kcfg_subFrame->isChecked())
            {
                int offset = (static_cast<double>(focusBoxSize->value()) / subBinX) * 1.5;
                int subX=(maxStar->x - offset) * subBinX;
                int subY=(maxStar->y - offset) * subBinY;
                int subW=offset*2*subBinX;
                int subH=offset*2*subBinY;

                int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
                targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

                if (subX< minX)
                    subX=minX;
                if (subY< minY)
                    subY= minY;
                if ((subW+subX)> maxW)
                    subW=maxW-subX;
                if ((subH+subY)> maxH)
                    subH=maxH-subY;

                //targetChip->setFocusFrame(subX, subY, subW, subH);

                //fx += subX;
                //fy += subY;
                //fw = subW;
                //fh = subH;
                //frameModified = true;

                //x += subX;
                //y += subY;
                //w = subW;
                //h = subH;

                QVariantMap settings = frameSettings[targetChip];
                settings["x"] = subX;
                settings["y"] = subY;
                settings["w"] = subW;
                settings["h"] = subH;
                settings["binx"] = subBinX;
                settings["biny"] = subBinY;

                frameSettings[targetChip] = settings;

                starCenter.setX(subW/(2*subBinX));
                starCenter.setY(subH/(2*subBinY));
                starCenter.setZ(subBinX);

                subFramed = true;

                focusView->setFirstLoad(true);

                capture();
            }
            else
            {
                starCenter.setX(maxStar->x);
                starCenter.setY(maxStar->y);
                starCenter.setZ(subBinX);

                if (inAutoFocus)
                    capture();
            }

            syncTrackingBoxPosition();
            defaultScale = static_cast<FITSScale>(filterCombo->currentIndex());            
            return;
        }
        else
        {
            appendLogText(i18n("Capture complete. Select a star to focus."));

            starSelected = false;
            //if (fw == 0 || fh == 0)
                //targetChip->getFrame(&fx, &fy, &fw, &fh);

            int subBinX=1,subBinY=1;
            targetChip->getBinning(&subBinX, &subBinY);

            focusView->setTrackingBox(QRect((w-focusBoxSize->value())/(subBinX*2), (h-focusBoxSize->value())/(2*subBinY), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY));
            focusView->setTrackingBoxEnabled(true);

            state = Ekos::FOCUS_WAITING;
            emit newStatus(state);

            waitStarSelectTimer.start();
            //connect(targetImage, SIGNAL(trackingStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)), Qt::UniqueConnection);
            return;
        }
    }

    if (minimumRequiredHFR >= 0)
    {
        if (currentHFR == -1)
        {
            if (noStarCount++ < MAX_RECAPTURE_RETRIES)
            {
                appendLogText(i18n("No stars detected, capturing again..."));
                // On Last Attempt reset focus frame to capture full frame and recapture star if possible
                if (noStarCount == MAX_RECAPTURE_RETRIES)
                    resetFrame();
                capture();
                return;
            }
            else
            {
                noStarCount = 0;
                setAutoFocusResult(false);
            }
        }
        else if (currentHFR > minimumRequiredHFR)
        {
           inSequenceFocus = true;           
           start();
        }
        else
        {
            setAutoFocusResult(true);
        }

        minimumRequiredHFR = -1;

        return;
    }

    drawProfilePlot();

    if (Options::focusLogging())
    {
        QDir dir;
        QString path = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "autofocus/" + QDateTime::currentDateTime().toString("yyyy-MM-dd");
        dir.mkpath(path);
        QString name = "autofocus_frame_" + QDateTime::currentDateTime().toString("HH:mm:ss") + ".fits";
        QString filename = path + QStringLiteral("/") + name;
        focusView->getImageData()->saveFITS(filename);
    }

    if (inAutoFocus==false)
        return;

    if (state != Ekos::FOCUS_PROGRESS)
    {
        state = Ekos::FOCUS_PROGRESS;
        emit newStatus(state);
    }

    if (canAbsMove || canRelMove)
        autoFocusAbs();
    else
        autoFocusRel();
}

void Focus::clearDataPoints()
{
    maxHFR=1;
    hfr_position.clear();
    hfr_value.clear();

    drawHFRPlot();
}

void Focus::drawHFRPlot()
{
    v_graph->setData(hfr_position, hfr_value);

    if (inFocusLoop == false && (canAbsMove || canRelMove))
    {
        //HFRPlot->xAxis->setLabel(i18n("Position"));
        HFRPlot->xAxis->setRange(minPos-pulseDuration, maxPos+pulseDuration);
        HFRPlot->yAxis->setRange(currentHFR/1.5, maxHFR);
    }
    else
    {
        //HFRPlot->xAxis->setLabel(i18n("Iteration"));
        HFRPlot->xAxis->setRange(1, hfr_value.count()+1);
        HFRPlot->yAxis->setRange(currentHFR/1.5, maxHFR*1.25);
    }

    HFRPlot->replot();

}

void Focus::drawProfilePlot()
{
    QVector<double> currentIndexes;
    QVector<double> currentFrequencies;

    // HFR = 50% * 1.36 = 68% aka one standard deviation
    double stdDev = currentHFR * 1.36;
    float start= -stdDev*4;
    float end  = stdDev*4;
    float step = stdDev*4 / 20.0;
    for (float x=start; x < end; x+= step)
    {
        currentIndexes.append(x);
        currentFrequencies.append((1/(stdDev*sqrt(2*M_PI))) * exp(-1 * ( x*x ) / (2 * (stdDev * stdDev))));
    }

    currentGaus->setData(currentIndexes, currentFrequencies);

    if (lastGausIndexes.count() > 0)
        lastGaus->setData(lastGausIndexes, lastGausFrequencies);

    if (focusType == FOCUS_AUTO && firstGaus == NULL)
    {
        firstGaus = profilePlot->addGraph();
        QPen pen;
        pen.setStyle(Qt::DashDotLine);
        pen.setWidth(2);
        pen.setColor(Qt::darkMagenta);
        firstGaus->setPen(pen);

        firstGaus->setData(currentIndexes, currentFrequencies);
    }
    else if (firstGaus)
    {
        profilePlot->removeGraph(firstGaus);
        firstGaus=NULL;
    }

    profilePlot->rescaleAxes();
    profilePlot->replot();

    lastGausIndexes      = currentIndexes;
    lastGausFrequencies  = currentFrequencies;

    profilePixmap = profilePlot->grab();
    emit newProfilePixmap(profilePixmap);
}

void Focus::autoFocusAbs()
{
    static int lastHFRPos=0, minHFRPos=0, initSlopePos=0, focusOutLimit=0, focusInLimit=0;
    static double minHFR=0, initSlopeHFR=0;
    double targetPosition=0, delta=0;

    QString deltaTxt = QString("%1").arg(fabs(currentHFR-minHFR)*100.0, 0,'g', 3);
    QString HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    if (Options::focusLogging())
    {
        qDebug() << "Focus: ########################################";
        qDebug() << "Focus: ========================================";
        qDebug() << "Focus: Current HFR: " << currentHFR << " Current Position: " << currentPosition;
        qDebug() << "Focus: Last minHFR: " << minHFR << " Last MinHFR Pos: " << minHFRPos;
        qDebug() << "Focus: Delta: " << deltaTxt << "%";
        qDebug() << "Focus: ========================================";
    }

    if (minHFR)
         appendLogText(i18n("FITS received. HFR %1 @ %2. Delta (%3%)", HFRText, currentPosition, deltaTxt));
    else
        appendLogText(i18n("FITS received. HFR %1 @ %2.", HFRText, currentPosition));

    if (++absIterations > MAXIMUM_ABS_ITERATIONS)
    {
        appendLogText(i18n("Autofocus failed to reach proper focus. Try increasing tolerance value."));
        abort();
        setAutoFocusResult(false);
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        if (noStarCount < MAX_RECAPTURE_RETRIES)
        {
            appendLogText(i18n("No stars detected, capturing again..."));
            capture();
            noStarCount++;
            return;
        }
        else if (noStarCount == MAX_RECAPTURE_RETRIES)
        {
            currentHFR = 20;
            noStarCount++;
        }
        else
        {
            appendLogText(i18n("Failed to detect any stars. Reset frame and try again."));
            abort();
            setAutoFocusResult(false);
            return;
        }
    }
    else
        noStarCount = 0;

    /*if (currentHFR > maxHFR || HFRAbsolutePoints.empty())
    {
        maxHFR = currentHFR;

        if (HFRAbsolutePoints.empty())
        {
            maxPos=1;
            minPos=1e6;
        }
    }*/

    if (hfr_position.empty())
    {
        maxPos=1;
        minPos=1e6;
    }

    if (currentPosition > maxPos)
        maxPos = currentPosition;
    if (currentPosition < minPos)
        minPos = currentPosition;

    //HFRPoint *p = new HFRPoint();

    //p->HFR = currentHFR;
    //p->pos = currentPosition;

    hfr_position.append(currentPosition);
    hfr_value.append(currentHFR);

    //HFRAbsolutePoints.append(p);

    drawHFRPlot();

    switch (lastFocusDirection)
    {
        case FOCUS_NONE:
            lastHFR = currentHFR;
            initialFocuserAbsPosition = currentPosition;
            minHFR=currentHFR;
            minHFRPos=currentPosition;
            HFRDec=0;
            HFRInc=0;
            focusOutLimit=0;
            focusInLimit=0;
            focusOut(pulseDuration);
            break;

        case FOCUS_IN:
        case FOCUS_OUT:
        if (reverseDir && focusInLimit && focusOutLimit && fabs(currentHFR - minHFR) < (toleranceIN->value()/100.0) && HFRInc == 0 )
            {
                if (absIterations <= 2)
                {
                    appendLogText(i18n("Change in HFR is too small. Try increasing the step size or decreasing the tolerance."));
                    abort();
                    setAutoFocusResult(false);
                }
                else if (noStarCount > 0)
                {
                    appendLogText(i18n("Failed to detect focus star in frame. Capture and select a focus star."));
                    abort();
                    setAutoFocusResult(false);
                }
                else
                {
                    appendLogText(i18n("Autofocus complete."));
                    stop();
                    emit resumeGuiding();
                    setAutoFocusResult(true);
                }
                break;
            }
            else if (currentHFR < lastHFR)
            {
                double slope=0;

                // Let's try to calculate slope of the V curve.
                if (initSlopeHFR == 0 && HFRInc == 0 && HFRDec >= 1)
                {
                    initSlopeHFR = lastHFR;
                    initSlopePos = lastHFRPos;

                    if (Options::focusLogging())
                        qDebug() << "Focus: Setting initial slop to " << initSlopePos << " @ HFR " << initSlopeHFR;
                }

                // Let's now limit the travel distance of the focuser
                if (lastFocusDirection == FOCUS_OUT && lastHFRPos < focusInLimit && fabs(currentHFR - lastHFR) > 0.1)
                {
                    focusInLimit = lastHFRPos;
                    if (Options::focusLogging())
                        qDebug() << "Focus: New FocusInLimit " << focusInLimit;
                }
                else if (lastFocusDirection == FOCUS_IN && lastHFRPos > focusOutLimit && fabs(currentHFR - lastHFR) > 0.1)
                {
                    focusOutLimit = lastHFRPos;
                    if (Options::focusLogging())
                        qDebug() << "Focus: New FocusOutLimit " << focusOutLimit;
                }

                // If we have slope, get next target position
                if (initSlopeHFR && absMotionMax > 50)
                {
                    double factor=0.5;
                    slope = (currentHFR - initSlopeHFR) / (currentPosition - initSlopePos);
                    if (fabs(currentHFR-minHFR)*100.0 < 0.5)
                        factor = 1 - fabs(currentHFR-minHFR)*10;
                    targetPosition = currentPosition + (currentHFR*factor - currentHFR)/slope;
                    if (targetPosition < 0)
                    {
                        factor = 1;
                        while (targetPosition < 0 && factor > 0)
                        {
                           factor -= 0.005;
                            targetPosition = currentPosition + (currentHFR*factor - currentHFR)/slope;
                        }
                    }
                    if (Options::focusLogging())
                        qDebug() << "Focus: Using slope to calculate target pulse...";
                }
                // Otherwise proceed iteratively
                else
                {
                     if (lastFocusDirection == FOCUS_IN)
                         targetPosition = currentPosition - pulseDuration;
                     else
                         targetPosition = currentPosition + pulseDuration;

                     if (Options::focusLogging())
                        qDebug() << "Focus: Proceeding iteratively to next target pulse ...";
                }

                if (Options::focusLogging())
                    qDebug() << "Focus: V-Curve Slope " << slope << " current Position " << currentPosition << " targetPosition " << targetPosition;

                lastHFR = currentHFR;

                // Let's keep track of the minimum HFR
                if (lastHFR < minHFR)
                {
                    minHFR = lastHFR;
                    minHFRPos = currentPosition;
                    if (Options::focusLogging())
                    {
                        qDebug() << "Focus: new minHFR " << minHFR << " @ positioin " << minHFRPos;
                        qDebug() << "Focus: ########################################";
                    }

                }

                lastHFRPos = currentPosition;

                // HFR is decreasing, we are on the right direction
                HFRDec++;
                HFRInc=0;
            }
            else

            {
                // HFR increased, let's deal with it.
                HFRInc++;
                HFRDec=0;

                // Reality Check: If it's first time, let's capture again and see if it changes.
                /*if (HFRInc <= 1 && reverseDir == false)
                {
                    capture();
                    return;
                }
                // Looks like we're going away from optimal HFR
                else
                {*/
                    reverseDir = true;
                    lastHFR = currentHFR;
                    lastHFRPos = currentPosition;
                    initSlopeHFR=0;
                    HFRInc=0;

                    if (Options::focusLogging())
                        qDebug() << "Focus: We are going away from optimal HFR ";

                    // Let's set new limits
                    if (lastFocusDirection == FOCUS_IN)
                    {
                        focusInLimit = currentPosition;
                        if (Options::focusLogging())
                            qDebug() << "Focus: Setting focus IN limit to " << focusInLimit;
                    }
                    else
                    {
                        focusOutLimit = currentPosition;
                        if (Options::focusLogging())
                            qDebug() << "Focus: Setting focus OUT limit to " << focusOutLimit;
                    }

                    // Decrease pulse
                    pulseDuration = pulseDuration * 0.75;

                    // Let's get close to the minimum HFR position so far detected
                    if (lastFocusDirection == FOCUS_OUT)
                          targetPosition = minHFRPos-pulseDuration/2;
                     else
                          targetPosition = minHFRPos+pulseDuration/2;

                    if (Options::focusLogging())
                        qDebug() << "Focus: new targetPosition " << targetPosition;

               // }
            }

        // Limit target Pulse to algorithm limits
        if (focusInLimit != 0 && lastFocusDirection == FOCUS_IN && targetPosition < focusInLimit)
        {
            targetPosition = focusInLimit;
            if (Options::focusLogging())
                qDebug() << "Focus: Limiting target pulse to focus in limit " << targetPosition;
        }
        else if (focusOutLimit != 0 && lastFocusDirection == FOCUS_OUT && targetPosition > focusOutLimit)
        {
            targetPosition = focusOutLimit;
            if (Options::focusLogging())
                qDebug() << "Focus: Limiting target pulse to focus out limit " << targetPosition;
        }

        // Limit target pulse to focuser limits
        if (targetPosition < absMotionMin)
            targetPosition = absMotionMin;
        else if (targetPosition > absMotionMax)
            targetPosition = absMotionMax;

        // Ops, we can't go any further, we're done.
        if (targetPosition == currentPosition)
        {
            appendLogText(i18n("Autofocus complete."));
            stop();
            emit resumeGuiding();
            setAutoFocusResult(true);
            return;
        }

        // Ops, deadlock
        if (focusOutLimit && focusOutLimit == focusInLimit)
        {
            appendLogText(i18n("Deadlock reached. Please try again with different settings."));
            abort();
            setAutoFocusResult(false);
            return;
        }

        if (fabs(targetPosition - initialFocuserAbsPosition) > maxTravelIN->value())
        {
            if (Options::focusLogging())
                qDebug() << "Focus: targetPosition (" << targetPosition << ") - initHFRAbsPos (" << initialFocuserAbsPosition << ") exceeds maxTravel distance of " << maxTravelIN->value();

            appendLogText("Maximum travel limit reached. Autofocus aborted.");
            abort();
            setAutoFocusResult(false);
            break;

        }

        // Get delta for next move
        delta = (targetPosition - currentPosition);

        if (Options::focusLogging())
        {
            qDebug() << "Focus: delta (targetPosition - currentPosition) " << delta;
            qDebug() << "Focus: Focusing " << ((delta < 0) ? "IN"  : "OUT");
            qDebug() << "Focus: ########################################";
        }

        // Now cross your fingers and wait
       if (delta > 0)
            focusOut(delta);
        else
          focusIn(fabs(delta));
        break;

    }

}

void Focus::autoFocusRel()
{
    static int noStarCount=0;
    static double minHFR=1e6;
    QString deltaTxt = QString("%1").arg(fabs(currentHFR-minHFR)*100.0, 0,'g', 2);
    QString minHFRText = QString("%1").arg(minHFR, 0, 'g', 3);
    QString HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    appendLogText(i18n("FITS received. HFR %1. Delta (%2%) Min HFR (%3)", HFRText, deltaTxt, minHFRText));

    if (pulseDuration <= MINIMUM_PULSE_TIMER)
    {
        appendLogText(i18n("Autofocus failed to reach proper focus. Try adjusting the tolerance value."));
        abort();
        setAutoFocusResult(false);
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        if (noStarCount++ < MAX_RECAPTURE_RETRIES)
        {
            appendLogText(i18n("No stars detected, capturing again..."));
            capture();
            return;
        }
        else
            currentHFR = 20;
    }
    else
        noStarCount = 0;

    switch (lastFocusDirection)
    {
        case FOCUS_NONE:
            lastHFR = currentHFR;
            minHFR=1e6;
            focusIn(pulseDuration);
            break;

        case FOCUS_IN:
            //if (fabs(currentHFR - lastHFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
            if (fabs(currentHFR - minHFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
            {
                appendLogText(i18n("Autofocus complete."));
                stop();
                emit resumeGuiding();
                setAutoFocusResult(true);
                break;
            }
            else if (currentHFR < lastHFR)
            {
                if (currentHFR < minHFR)
                    minHFR = currentHFR;

                lastHFR = currentHFR;
                focusIn(pulseDuration);
                HFRInc=0;
            }
            else
            {
                HFRInc++;

                /*if (HFRInc <= 1)
                {
                    capture();
                    return;
                }
                else
                {*/

                    lastHFR = currentHFR;

                    HFRInc=0;

                    pulseDuration *= 0.75;
                    focusOut(pulseDuration);
                //}
            }

            break;

    case FOCUS_OUT:
        if (fabs(currentHFR - minHFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
        //if (fabs(currentHFR - lastHFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
        {
            appendLogText(i18n("Autofocus complete."));
            stop();
            emit resumeGuiding();
            setAutoFocusResult(true);
            break;
        }
        else if (currentHFR < lastHFR)
        {
            if (currentHFR < minHFR)
                minHFR = currentHFR;

            lastHFR = currentHFR;
            focusOut(pulseDuration);
            HFRInc=0;
        }
        else
        {
            HFRInc++;

            /*if (HFRInc <= 1)
                capture();
            else
            {*/

                lastHFR = currentHFR;

                HFRInc=0;

                pulseDuration *= 0.75;
                focusIn(pulseDuration);
            //}
        }

        break;

    }

}

void Focus::processFocusNumber(INumberVectorProperty *nvp)
{

    if (canAbsMove == false && currentFocuser->canAbsMove())
    {
        canAbsMove = true;
        getAbsFocusPosition();

        absTicksSpin->setEnabled(true);
        setAbsTicksB->setEnabled(true);
    }

    if (canRelMove == false && currentFocuser->canRelMove())
        canRelMove = true;

    if (!strcmp(nvp->name, "ABS_FOCUS_POSITION"))
    {
       INumber *pos = IUFindNumber(nvp, "FOCUS_ABSOLUTE_POSITION");
       if (pos)
       {
           currentPosition = pos->value;
           absTicksSpin->setValue(currentPosition);
       }

       if (resetFocus && nvp->s == IPS_OK)
       {
           resetFocus = false;
           appendLogText(i18n("Restarting autofocus process..."));
           start();
       }

       if (canAbsMove && inAutoFocus)
       {
           if (nvp->s == IPS_OK && captureInProgress == false)
               capture();
           else if (nvp->s == IPS_ALERT)
           {
               appendLogText(i18n("Focuser error, check INDI panel."));
               abort();
               setAutoFocusResult(false);
           }

       }

       return;
    }

    if (canAbsMove)
        return;

    if (!strcmp(nvp->name, "REL_FOCUS_POSITION"))
    {
        INumber *pos = IUFindNumber(nvp, "FOCUS_RELATIVE_POSITION");
        if (pos && nvp->s == IPS_OK)
            currentPosition += pos->value * (lastFocusDirection == FOCUS_IN ? -1 : 1);

        if (resetFocus && nvp->s == IPS_OK)
        {
            resetFocus = false;
            appendLogText(i18n("Restarting autofocus process..."));
            start();
        }

        if (canRelMove && inAutoFocus)
        {
            if (nvp->s == IPS_OK && captureInProgress == false)
                capture();
            else if (nvp->s == IPS_ALERT)
            {
                appendLogText(i18n("Focuser error, check INDI panel."));
                abort();
                setAutoFocusResult(false);
            }
        }

        return;
    }

    if (canRelMove)
        return;

    if (!strcmp(nvp->name, "FOCUS_TIMER"))
    {
        if (resetFocus && nvp->s == IPS_OK)
        {
            resetFocus = false;
            appendLogText(i18n("Restarting autofocus process..."));
            start();
        }

        if (canAbsMove == false && canRelMove == false && inAutoFocus)
        {
            if (nvp->s == IPS_OK && captureInProgress == false)
                capture();
            else if (nvp->s == IPS_ALERT)
            {
                appendLogText(i18n("Focuser error, check INDI panel."));
                abort();
                setAutoFocusResult(false);
            }

        }

        return;
    }

}

void Focus::appendLogText(const QString &text)
{

    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    if (Options::focusLogging())
        qDebug() << "Focus: " << text;

    emit newLog();
}

void Focus::clearLog()
{
    logText.clear();
    emit newLog();
}

void Focus::startFraming()
{
    if (currentCCD == NULL)
    {
        appendLogText(i18n("No CCD connected."));
        return;
    }

    waitStarSelectTimer.stop();

    inFocusLoop = true;
    frameNum=0;

    clearDataPoints();

    //emit statusUpdated(true);
    state = Ekos::FOCUS_FRAMING;
    emit newStatus(state);

    resetButtons();

    appendLogText(i18n("Starting continuous exposure..."));

    capture();
}

void Focus::resetButtons()
{
    if (inFocusLoop)
    {
        startFocusB->setEnabled(false);
        startLoopB->setEnabled(false);
        stopFocusB->setEnabled(true);

        captureB->setEnabled(false);

        return;
    }

    if (inAutoFocus)
    {
        stopFocusB->setEnabled(true);

        startFocusB->setEnabled(false);
        startLoopB->setEnabled(false);
        captureB->setEnabled(false);
        focusOutB->setEnabled(false);
        focusInB->setEnabled(false);
        setAbsTicksB->setEnabled(false);

        return;
    }

    if (currentFocuser)
    {
        focusOutB->setEnabled(true);
        focusInB->setEnabled(true);

        startFocusB->setEnabled(focusType == FOCUS_AUTO);
        setAbsTicksB->setEnabled(canAbsMove || canRelMove);

    }

    stopFocusB->setEnabled(false);
    startLoopB->setEnabled(true);

    captureB->setEnabled(true);
}

void Focus::updateBoxSize(int value)
{
    if (currentCCD == NULL)
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    if (targetChip == NULL)
        return;

    QRect trackBox = focusView->getTrackingBox();
    trackBox.setX(trackBox.x()+(trackBox.width()-value)/2);
    trackBox.setY(trackBox.y()+(trackBox.height()-value)/2);
    trackBox.setWidth(value);
    trackBox.setHeight(value);

    focusView->setTrackingBox(trackBox);
}

void Focus::focusStarSelected(int x, int y)
{
    if (state == Ekos::FOCUS_PROGRESS)
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    int subBinX, subBinY;
    targetChip->getBinning(&subBinX, &subBinY);

    // If binning was changed outside of the focus module, recapture
    if (subBinX != activeBin)
    {
        capture();
        return;
    }

    int offset = (static_cast<double>(focusBoxSize->value())/subBinX) * 1.5;

    QRect starRect;

    bool squareMovedOutside=false;

    if (subFramed == false && kcfg_subFrame->isChecked() && targetChip->canSubframe())
    {
        int minX, maxX, minY, maxY, minW, maxW, minH, maxH;//, fx,fy,fw,fh;

        targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);
        //targetChip->getFrame(&fx, &fy, &fw, &fy);

        x = (x - offset) * subBinX;
        y = (y - offset) * subBinY;
        int w=offset*2*subBinX;
        int h=offset*2*subBinY;

        if (x<minX)
            x=minX;
        if (y<minY)
            y=minY;
        if ((x+w)>maxW)
            w=maxW-x;
        if ((y+h)>maxH)
            h=maxH-y;

        //targetChip->getFrame(&orig_x, &orig_y, &orig_w, &orig_h);

        //fx += x;
        //fy += y;
        //fw = w;
        //fh = h;

        //targetChip->setFocusFrame(fx, fy, fw, fh);
        //frameModified=true;

        QVariantMap settings = frameSettings[targetChip];
        settings["x"] = x;
        settings["y"] = y;
        settings["w"] = w;
        settings["h"] = h;
        settings["binx"] = subBinX;
        settings["biny"] = subBinY;

        frameSettings[targetChip] = settings;

        subFramed = true;

        focusView->setFirstLoad(true);

        capture();

        //starRect = QRect((w-focusBoxSize->value())/(subBinX*2), (h-focusBoxSize->value())/(subBinY*2), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY);
        starCenter.setX(w/(2*subBinX));
        starCenter.setY(h/(2*subBinY));
    }
    else
    {
        //starRect = QRect(x-focusBoxSize->value()/(subBinX*2), y-focusBoxSize->value()/(subBinY*2), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY);
        double dist = sqrt( (starCenter.x()-x)*(starCenter.x()-x) + (starCenter.y()-y)*(starCenter.y()-y) );
        squareMovedOutside = (dist > (focusBoxSize->value()/subBinX));
        starCenter.setX(x);
        starCenter.setY(y);
        //starRect = QRect( starCenter.x()-focusBoxSize->value()/(2*subBinX), starCenter.y()-focusBoxSize->value()/(2*subBinY), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY);
        starRect = QRect( starCenter.x()-focusBoxSize->value()/(2*subBinX), starCenter.y()-focusBoxSize->value()/(2*subBinY), focusBoxSize->value()/subBinX, focusBoxSize->value()/subBinY);
        focusView->setTrackingBox(starRect);


    }

    starCenter.setZ(subBinX);

    //starSelected=true;

    defaultScale = static_cast<FITSScale>(filterCombo->currentIndex());    

    if (starSelected == false)
    {
        appendLogText(i18n("Focus star is selected."));
        starSelected = true;
    }

    if (squareMovedOutside && inAutoFocus == false && autoStarCheck->isChecked())
    {
        autoStarCheck->blockSignals(true);
        autoStarCheck->setChecked(false);
        autoStarCheck->blockSignals(false);
        appendLogText(i18n("Disabling Auto Star Selection as star selection box was moved manually."));
        starSelected = false;
    }

    waitStarSelectTimer.stop();
    state = inAutoFocus ? FOCUS_PROGRESS : FOCUS_IDLE;

    emit newStatus(state);
}

void Focus::checkFocus(double requiredHFR)
{
    minimumRequiredHFR = requiredHFR;

    capture();
}

void Focus::toggleSubframe(bool enable)
{
    if (enable == false)
        resetFrame();

    starSelected = false;
    starCenter = QVector3D();
}

void Focus::filterChangeWarning(int index)
{
    // index = 4 is MEDIAN filter which helps reduce noise
    if (index != 0 && index != FITS_MEDIAN)
        appendLogText(i18n("Warning: Only use filters for preview as they may interface with autofocus operation."));

    Options::setFocusEffect(index);

    defaultScale = static_cast<FITSScale>(index);
}

void Focus::setExposure(double value)
{
    exposureIN->setValue(value);
}

void Focus::setBinning(int subBinX, int subBinY)
{
  INDI_UNUSED(subBinY);
  binningCombo->setCurrentIndex(subBinX-1);
}

void Focus::setImageFilter(const QString & value)
{
    for (int i=0; i < filterCombo->count(); i++)
        if (filterCombo->itemText(i) == value)
        {
            filterCombo->setCurrentIndex(i);
            break;
        }
}

void Focus::setAutoStarEnabled(bool enable)
{
    autoStarCheck->setChecked(enable);
}

void Focus::setAutoSubFrameEnabled(bool enable)
{
    kcfg_subFrame->setChecked(enable);
}

void Focus::setAutoFocusParameters(int boxSize, int stepSize, int maxTravel, double tolerance)
{
    focusBoxSize->setValue(boxSize);
    stepIN->setValue(stepSize);
    maxTravelIN->setValue(maxTravel);
    toleranceIN->setValue(tolerance);
}

void Focus::setAutoFocusResult(bool status)
{    
    // In case of failure, go back to last position if the focuser is absolute
    if (status == false &&  canAbsMove && currentFocuser && initialFocuserAbsPosition >= 0)
    {
        currentFocuser->moveAbs(initialFocuserAbsPosition);
        appendLogText(i18n("Autofocus failed, moving back to initial focus position %1.", initialFocuserAbsPosition));

        // If we're doing in sequence focusing using an absolute focuser, let's retry focusing starting from last known good position before we give up
        if (inSequenceFocus && resetFocusIteration++ < MAXIMUM_RESET_ITERATIONS && resetFocus == false)
        {
            resetFocus = true;
            // Reset focus frame in case the star in subframe was lost
            resetFrame();
            return;
        }
    }

    resetFocusIteration = 0;

    //emit autoFocusFinished(status, currentHFR);

    if (status)
    {
        KNotification::event( QLatin1String( "FocusSuccessful" ) , i18n("Autofocus operation completed successfully"));
        state = Ekos::FOCUS_COMPLETE;
    }
     else
    {
        KNotification::event( QLatin1String( "FocusFailed" ) , i18n("Autofocus operation failed with errors"));
        state = Ekos::FOCUS_FAILED;
    }

    emit newStatus(state);
}

void Focus::checkAutoStarTimeout()
{
    //if (starSelected == false && inAutoFocus)
    if (starCenter.isNull() && inAutoFocus)
    {
        appendLogText(i18n("No star was selected. Aborting..."));
        initialFocuserAbsPosition=-1;
        abort();
        setAutoFocusResult(false);
    }
    else if (state == FOCUS_WAITING)
    {
        state = FOCUS_IDLE;
        emit newStatus(state);
    }
}

void Focus::setAbsoluteFocusTicks()
{
    if (currentFocuser == NULL)
         return;

    if (currentFocuser->isConnected() == false)
    {
       appendLogText(i18n("Error: Lost connection to Focuser."));
       return;
    }

    if (Options::focusLogging())
        qDebug() << "Focus: Setting focus ticks to " << absTicksSpin->value();

    currentFocuser->moveAbs(absTicksSpin->value());
}

void Focus::setActiveBinning(int bin)
{
    activeBin = bin + 1;
}

void Focus::setThreshold(double value)
{
    Options::setFocusThreshold(value);
}

// TODO remove from kstars.kcfg
/*void Focus::setFrames(int value)
{
    Options::setFocusFrames(value);
}*/

void Focus::syncTrackingBoxPosition()
{
    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    Q_ASSERT(targetChip);

    int subBinX=1, subBinY=1;
    targetChip->getBinning(&subBinX, &subBinY);

    if (starCenter.isNull() == false)
    {
        double boxSize = focusBoxSize->value();
        int x,y,w,h;
        targetChip->getFrame(&x,&y,&w,&h);
        // If box size is larger than image size, set it to lower index
        if (boxSize/subBinX >= w || boxSize/subBinY >= h)
        {
            focusBoxSize->setValue( (boxSize/subBinX >= w) ? w : h);
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
        focusView->setTrackingBoxEnabled(true);
        focusView->setTrackingBox(starRect);
    }
}

void Focus::showFITSViewer()
{
    FITSData *data = focusView->getImageData();
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
