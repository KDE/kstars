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
#include "ekosmanager.h"

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
    darkBuffer     = NULL;

    canAbsMove        = false;
    canRelMove        = false;
    inAutoFocus       = false;
    inFocusLoop       = false;
    captureInProgress = false;
    inSequenceFocus   = false;
    starSelected      = false;
    frameModified     = false;
    resetFocus        = false;
    m_autoFocusSuccesful = false;
    filterPositionPending= false;
    haveDarkFrame        = false;

    calibrationState = CALIBRATE_NONE;

    rememberUploadMode = ISD::CCD::UPLOAD_CLIENT;
    HFRInc =0;
    noStarCount=0;
    reverseDir = false;
    initialFocuserAbsPosition = -1;

    pulseDuration = 1000;

    resetFocusIteration=0;
    fy=fw=fh=0;
    orig_x = orig_y = orig_w = orig_h =-1;
    lockedFilterIndex=-1;
    maxHFR=1;
    minimumRequiredHFR = -1;
    currentFilterIndex=-1;
    minPos=1e6;
    maxPos=0;

    connect(startFocusB, SIGNAL(clicked()), this, SLOT(start()));
    connect(stopFocusB, SIGNAL(clicked()), this, SLOT(checkStopFocus()));

    connect(focusOutB, SIGNAL(clicked()), this, SLOT(FocusOut()));
    connect(focusInB, SIGNAL(clicked()), this, SLOT(FocusIn()));

    connect(captureB, SIGNAL(clicked()), this, SLOT(capture()));

    connect(AutoModeR, SIGNAL(toggled(bool)), this, SLOT(toggleAutofocus(bool)));

    connect(startLoopB, SIGNAL(clicked()), this, SLOT(startFraming()));

    connect(kcfg_subFrame, SIGNAL(toggled(bool)), this, SLOT(toggleSubframe(bool)));

    connect(resetFrameB, SIGNAL(clicked()), this, SLOT(resetFocusFrame()));

    connect(CCDCaptureCombo, SIGNAL(activated(QString)), this, SLOT(setDefaultCCD(QString)));
    connect(CCDCaptureCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(checkCCD(int)));

    connect(focuserCombo, SIGNAL(activated(int)), this, SLOT(checkFocuser(int)));
    connect(FilterCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkFilter(int)));
    connect(FilterPosCombo, SIGNAL(activated(int)), this, SLOT(updateFilterPos(int)));
    connect(lockFilterCheck, SIGNAL(toggled(bool)), this, SLOT(filterLockToggled(bool)));
    connect(filterCombo, SIGNAL(activated(int)), this, SLOT(filterChangeWarning(int)));
    connect(setAbsTicksB, SIGNAL(clicked()), this, SLOT(setAbsoluteFocusTicks()));
    connect(binningCombo, SIGNAL(activated(int)), this, SLOT(setActiveBinning(int)));

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

    exposureIN->setValue(Options::focusExposure());
    toleranceIN->setValue(Options::focusTolerance());
    stepIN->setValue(Options::focusTicks());
    kcfg_autoSelectStar->setChecked(Options::autoSelectStar());
    kcfg_focusBoxSize->setValue(Options::focusBoxSize());    
    maxTravelIN->setValue(Options::focusMaxTravel());
    kcfg_subFrame->setChecked(Options::focusSubFrame());
    suspendGuideCheck->setChecked(Options::suspendGuiding());
    lockFilterCheck->setChecked(Options::lockFocusFilter());
    focusDarkFrameCheck->setChecked(Options::focusDarkFrame());
}

Focus::~Focus()
{
    //qDeleteAll(HFRAbsolutePoints);
   // HFRAbsolutePoints.clear();

    delete (darkBuffer);
}

void Focus::toggleAutofocus(bool enable)
{
    if (enable)
    {
        focusType = FOCUS_AUTO;
        drawHFRPlot();
    }
    else
    {
        focusType = FOCUS_MANUAL;
        drawHFRPlot();
    }

    if (inFocusLoop || inAutoFocus)
        abort();
    else
        resetButtons();
}

void Focus::resetFrame()
{
    if (currentCCD)
    {
        ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

        if (frameModified && orig_w > 0 && !inAutoFocus && !inFocusLoop && !inSequenceFocus && targetChip && targetChip->canSubframe())
        {
                    targetChip->setFrame(orig_x, orig_y, orig_w, orig_h);
                    frameModified = false;                    
        }

        haveDarkFrame=false;
        calibrationState = CALIBRATE_NONE;
    }
}

void Focus::resetFocusFrame()
{
    if (currentCCD)
    {
        ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

        if (targetChip)
        {
            fx=fy=fw=fh=0;
            targetChip->resetFrame();
            targetChip->setFocusFrame(0,0,0,0);
            starSelected = false;
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
        ccdNum = CCDCaptureCombo->currentIndex();

    if (ccdNum >=0 && ccdNum <= CCDs.count())
    {
        currentCCD = CCDs.at(ccdNum);

        ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
        if (targetChip)
        {
            binningCombo->setEnabled(targetChip->canBin());
            kcfg_subFrame->setEnabled(targetChip->canSubframe());
            kcfg_autoSelectStar->setEnabled(targetChip->canSubframe());
            if (targetChip->canBin())
            {
                int binx=1,biny=1;
                binningCombo->clear();
                targetChip->getMaxBin(&binx, &biny);
                for (int i=1; i <= binx; i++)
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
    if (targetChip)      
        targetChip->getFocusFrame(&fx, &fy, &fw, &fh);
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

    connect(currentFocuser, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processFocusNumber(INumberVectorProperty*)), Qt::UniqueConnection);

    AutoModeR->setEnabled(true);

    resetButtons();

    if (!inAutoFocus && !inFocusLoop && !captureInProgress && !inSequenceFocus)
        emit autoFocusFinished(true, -1);
}

void Focus::addCCD(ISD::GDInterface *newCCD)
{
    if (CCDs.contains(static_cast<ISD::CCD *>(newCCD)))
         return;

    CCDs.append(static_cast<ISD::CCD *>(newCCD));

    CCDCaptureCombo->addItem(newCCD->getDeviceName());    

    //checkCCD(CCDs.count()-1);
    //CCDCaptureCombo->setCurrentIndex(CCDs.count()-1);
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
    m_autoFocusSuccesful = false;

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

    Options::setFocusSubFrame(kcfg_subFrame->isChecked());
    Options::setAutoSelectStar(kcfg_autoSelectStar->isChecked());
    Options::setSuspendGuiding(suspendGuideCheck->isChecked());
    Options::setLockFocusFilter(lockFilterCheck->isChecked());
    Options::setFocusDarkFrame(focusDarkFrameCheck->isChecked());

    if (Options::focusLogging())
        qDebug() << "Focus: Starting focus with pulseDuration " << pulseDuration;

    if (kcfg_autoSelectStar->isChecked())
        appendLogText(i18n("Autofocus in progress..."));
    else
        appendLogText(i18n("Please wait until image capture is complete..."));

    if (suspendGuideCheck->isChecked())
         emit suspendGuiding(true);

    emit statusUpdated(true);    

    capture();
}

void Focus::checkStopFocus()
{
    if (inSequenceFocus == true)
    {
        inSequenceFocus = false;
        updateFocusStatus(false);
    }

    if (captureInProgress && inAutoFocus == false && inFocusLoop == false)
    {
        captureB->setEnabled(true);
        stopFocusB->setEnabled(false);

        appendLogText(i18n("Capture aborted."));

        emit statusUpdated(false);
    }

    abort();
}

void Focus::abort()
{

    if (Options::focusLogging())
        qDebug() << "Focus: Stopppig Focus";

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    inAutoFocus = false;
    inFocusLoop = false;
    //starSelected= false;
    minimumRequiredHFR    = -1;
    noStarCount = 0;
    //maxHFR=1;

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));    

    if (rememberUploadMode != currentCCD->getUploadMode())
        currentCCD->setUploadMode(rememberUploadMode);

    targetChip->abortExposure();

    resetFrame();

    FITSView *targetImage = targetChip->getImage(FITS_FOCUS);
    if (targetImage)
        targetImage->updateMode(FITS_FOCUS);

    resetButtons();

    absIterations = 0;
    HFRInc=0;
    reverseDir = false;

    emit statusUpdated(false);

}

void Focus::capture()
{
    if (currentCCD == NULL)
    {
        appendLogText(i18n("No CCD connected."));
        return;
    }

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    double seqExpose = exposureIN->value();
    CCDFrameType ccdFrame = FRAME_LIGHT;

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

    // Check if we need to capture a dark frame
    if (inFocusLoop == false && haveDarkFrame == false && focusDarkFrameCheck->isChecked() && calibrationState == CALIBRATE_NONE)
    {
        ccdFrame = FRAME_DARK;
        calibrationState = CALIBRATE_START;

        if (ISOCombo->isEnabled())
            KMessageBox::information(NULL, i18n("Cover your telescope in order to take a dark frame..."));
    }

    targetChip->setBinning(activeBin, activeBin);

    targetChip->setCaptureMode( (ccdFrame == FRAME_LIGHT) ? FITS_FOCUS : FITS_CALIBRATE);
    targetChip->setCaptureFilter( (FITSScale) filterCombo->currentIndex());

    if (ISOCombo->isEnabled() && ISOCombo->currentIndex() != -1 && targetChip->getISOIndex() != ISOCombo->currentIndex())
        targetChip->setISOIndex(ISOCombo->currentIndex());

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));    

    targetChip->setFrameType(ccdFrame);

    if (fw == 0 || fh == 0)
        targetChip->getFrame(&fx, &fy, &fw, &fh);

     targetChip->setFrame(fx, fy, fw, fh);

     if (fx != orig_x || fy != orig_y || fw != orig_w || fh != orig_h)
         frameModified = true;

    captureInProgress = true;

    targetChip->capture(seqExpose);

    if (inFocusLoop == false)
    {
        if (ccdFrame == FRAME_LIGHT)
            appendLogText(i18n("Capturing image..."));
        else
            appendLogText(i18n("Capturing dark frame..."));

        if (inAutoFocus == false)
        {
            captureB->setEnabled(false);
            stopFocusB->setEnabled(true);
        }
    }
}

void Focus::FocusIn(int ms)
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

void Focus::FocusOut(int ms)
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
    INDI_UNUSED(bp);
    QString HFRText;

    // Ignore guide head if there is any.
    if (!strcmp(bp->name, "CCD2"))
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    if (inFocusLoop == false)
    {
        if (calibrationState == CALIBRATE_START)
        {
            calibrationState = CALIBRATE_DONE;
            capture();
            return;
        }

        // If we're done capturing dark frame, store it.
        if (focusDarkFrameCheck->isChecked() && calibrationState == CALIBRATE_DONE)
        {
            calibrationState = CALIBRATE_NONE;

            delete (darkBuffer);

            FITSView *calibrateImage = targetChip->getImage(FITS_CALIBRATE);
            Q_ASSERT(calibrateImage != NULL);
            FITSData *calibrateData = calibrateImage->getImageData();

            haveDarkFrame = true;
            int totalSize = calibrateData->getSize()*calibrateData->getNumOfChannels();
            darkBuffer = new float[totalSize];
            memcpy(darkBuffer, calibrateData->getImageBuffer(), totalSize*sizeof(float));
        }

        // If we already have a dark frame, subtract it from light frame
        if (haveDarkFrame)
        {
            FITSView *currentImage   = targetChip->getImage(FITS_FOCUS);
            Q_ASSERT(currentImage != NULL);

            currentImage->getImageData()->subtract(darkBuffer);
            currentImage->rescale(ZOOM_KEEP_LEVEL);
        }
    }

    // Always reset capture mode to NORMAL
    targetChip->setCaptureMode(FITS_NORMAL);

    FITSView *targetImage = targetChip->getImage(FITS_FOCUS);

    if (targetImage == NULL)
    {
        appendLogText(i18n("FITS image failed to load, aborting..."));
        abort();
        return;
    }

    if (captureInProgress && inFocusLoop == false && inAutoFocus==false)
    {
        captureB->setEnabled(true);
        stopFocusB->setEnabled(false);
        currentCCD->setUploadMode(rememberUploadMode);
    }

    captureInProgress = false;    

    FITSData *image_data = targetChip->getImageData();

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    starPixmap = targetImage->getTrackingBoxPixmap();
    emit newStarPixmap(starPixmap);

    // If we're not framing, let's try to detect stars
    if (inFocusLoop == false)
    {
        image_data->findStars();

        currentHFR= image_data->getHFR(HFR_MAX);

        if (currentHFR == -1)
        {
            currentHFR = image_data->getHFR();
        }

        if (Options::focusLogging())
            qDebug() << "Focus newFITS: Current HFR " << currentHFR;

        HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

        if (focusType == FOCUS_MANUAL && lastHFR == -1)
                appendLogText(i18n("FITS received. No stars detected."));

        HFROut->setText(HFRText);

        if (currentHFR > 0)
        {
            if (currentHFR > maxHFR)
                maxHFR = currentHFR;

            if (hfr_position.empty())
                hfr_position.append(1);
            else
                hfr_position.append(hfr_position.last()+1);
            hfr_value.append(currentHFR);

            if (focusType == FOCUS_MANUAL || (inAutoFocus && canAbsMove == false && canRelMove == false))
                drawHFRPlot();
        }
    }
    // If just framing, let's capture again
    else
    {
        capture();
        return;

    }

    if (starSelected == false)
    {
        int subBinX=1, subBinY=1;
        targetChip->getBinning(&subBinX, &subBinY);

        if (kcfg_autoSelectStar->isEnabled() && kcfg_autoSelectStar->isChecked() && focusType == FOCUS_AUTO)
        {
            Edge *maxStar = image_data->getMaxHFRStar();
            if (maxStar == NULL)
            {
                appendLogText(i18n("Failed to automatically select a star. Please select a star manually."));
                //targetImage->updateMode(FITS_GUIDE);
                targetImage->setTrackingBoxSize(QSize(kcfg_focusBoxSize->value(), kcfg_focusBoxSize->value()));
                if (fw == 0 || fh == 0)
                    targetChip->getFrame(&fx, &fy, &fw, &fh);
                targetImage->setTrackingBoxCenter(QPointF(fw/2, fh/2));
                targetImage->setTrackingBoxEnabled(true);
                connect(targetImage, SIGNAL(trackingStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)), Qt::UniqueConnection);

                QTimer::singleShot(AUTO_STAR_TIMEOUT, this, SLOT(checkAutoStarTimeout()));

                return;
            }

            if (kcfg_subFrame->isEnabled() && kcfg_subFrame->isChecked())
            {
                int offset = kcfg_focusBoxSize->value();
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

                targetChip->setFocusFrame(subX, subY, subW, subH);

                targetChip->getFrame(&orig_x, &orig_y, &orig_w, &orig_h);
                /*orig_x = fx;
                orig_y = fy;
                orig_w = fw;
                orig_h = fh;*/

                fx += subX;
                fy += subY;
                fw = subW;
                fh = subH;
                frameModified = true;
                haveDarkFrame=false;
                calibrationState = CALIBRATE_NONE;

                targetImage->setTrackingBoxCenter(QPointF(fw/2.0,fh/2.0));
                targetImage->setTrackingBoxSize(QSize(subW/5.0, subH/5.0));
            }
            else
                targetChip->getFrame(&fx, &fy, &fw, &fh);

            starSelected=true;            

            capture();

            return;


        }
        else if (kcfg_subFrame->isEnabled() && kcfg_subFrame->isChecked())
        {
            appendLogText(i18n("Capture complete. Select a star to focus."));
            //targetImage->updateMode(FITS_GUIDE);
            targetImage->setTrackingBoxSize(QSize(kcfg_focusBoxSize->value(),kcfg_focusBoxSize->value()));
            targetImage->setTrackingBoxCenter(QPointF(fw/2, fh/2));
            targetImage->setTrackingBoxEnabled(true);
            connect(targetImage, SIGNAL(trackingStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)), Qt::UniqueConnection);
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
                    resetFocusFrame();
                capture();
                return;
            }
            else
            {
                noStarCount = 0;
                updateFocusStatus(false);
            }
        }
        else if (currentHFR > minimumRequiredHFR)
        {
           inSequenceFocus = true;
           AutoModeR->setChecked(true);
           start();
        }
        else
        {
            updateFocusStatus(true);
        }

        minimumRequiredHFR = -1;

        return;
    }

    drawProfilePlot();

    if (focusType == FOCUS_MANUAL || inAutoFocus==false)
        return;

    if (Options::focusLogging())
    {
        QDir dir;
        QString path = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "autofocus/" + QDateTime::currentDateTime().toString("yyyy-MM-dd");
        dir.mkpath(path);
        QString name = "autofocus_frame_" + QDateTime::currentDateTime().toString("HH:mm:ss") + ".fits";
        QString filename = path + QStringLiteral("/") + name;
        targetImage->getImageData()->saveFITS(filename);
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

    if (focusType == FOCUS_AUTO && (canAbsMove || canRelMove) )
    {
        //HFRPlot->xAxis->setLabel(i18n("Position"));
        HFRPlot->xAxis->setRange(minPos-pulseDuration, maxPos+pulseDuration);
        HFRPlot->yAxis->setRange(currentHFR/1.5, maxHFR);
    }
    else
    {
        //HFRPlot->xAxis->setLabel(i18n("Iteration"));
        HFRPlot->xAxis->setRange(1, hfr_value.count()+1);
        HFRPlot->yAxis->setRange(currentHFR/1.5, maxHFR);
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
    else if (focusType == FOCUS_MANUAL && firstGaus)
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
        updateFocusStatus(false);
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
            FocusOut(pulseDuration);
            break;

        case FOCUS_IN:
        case FOCUS_OUT:
        if (reverseDir && focusInLimit && focusOutLimit && fabs(currentHFR - minHFR) < (toleranceIN->value()/100.0) && HFRInc == 0 )
            {
                if (absIterations <= 2)
                {
                    appendLogText(i18n("Change in HFR is too small. Try increasing the step size or decreasing the tolerance."));
                    abort();
                    updateFocusStatus(false);
                }
                else if (noStarCount > 0)
                {
                    appendLogText(i18n("Failed to detect focus star in frame. Capture and select a focus star."));
                    abort();
                    updateFocusStatus(false);
                }
                else
                {
                    appendLogText(i18n("Autofocus complete."));
                    abort();
                    emit suspendGuiding(false);
                    updateFocusStatus(true);
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
                        while (targetPosition < 0)
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
                reverseDir = true;

                // Reality Check: If it's first time, let's capture again and see if it changes.
                /*if (HFRInc <= 1)
                {
                    capture();
                    return;
                }
                // Looks like we're going away from optimal HFR
                else
                {*/

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

                //}
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
            abort();
            emit suspendGuiding(false);
            updateFocusStatus(true);
            return;
        }

        // Ops, deadlock
        if (focusOutLimit && focusOutLimit == focusInLimit)
        {
            appendLogText(i18n("Deadlock reached. Please try again with different settings."));
            abort();
            updateFocusStatus(false);
            return;
        }

        if (fabs(targetPosition - initialFocuserAbsPosition) > maxTravelIN->value())
        {
            if (Options::focusLogging())
                qDebug() << "Focus: targetPosition (" << targetPosition << ") - initHFRAbsPos (" << initialFocuserAbsPosition << ") exceeds maxTravel distance of " << maxTravelIN->value();

            appendLogText("Maximum travel limit reached. Autofocus aborted.");
            abort();
            updateFocusStatus(false);
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
            FocusOut(delta);
        else
          FocusIn(fabs(delta));
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
        updateFocusStatus(false);
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
            FocusIn(pulseDuration);
            break;

        case FOCUS_IN:
            //if (fabs(currentHFR - lastHFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
            if (fabs(currentHFR - minHFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
            {
                appendLogText(i18n("Autofocus complete."));
                abort();
                emit suspendGuiding(false);
                updateFocusStatus(true);
                break;
            }
            else if (currentHFR < lastHFR)
            {
                if (currentHFR < minHFR)
                    minHFR = currentHFR;

                lastHFR = currentHFR;
                FocusIn(pulseDuration);
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
                    FocusOut(pulseDuration);
                //}
            }

            break;

    case FOCUS_OUT:
        if (fabs(currentHFR - minHFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
        //if (fabs(currentHFR - lastHFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
        {
            appendLogText(i18n("Autofocus complete."));
            abort();
            emit suspendGuiding(false);
            updateFocusStatus(true);
            break;
        }
        else if (currentHFR < lastHFR)
        {
            if (currentHFR < minHFR)
                minHFR = currentHFR;

            lastHFR = currentHFR;
            FocusOut(pulseDuration);
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
                FocusIn(pulseDuration);
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
               updateFocusStatus(false);
           }

       }

       return;
    }

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
                updateFocusStatus(false);
            }
        }

        return;
    }

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
                updateFocusStatus(false);
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

    inFocusLoop = true;

    emit statusUpdated(true);

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
        focusOutB->setEnabled(true);
        focusInB->setEnabled(true);

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

        return;
    }

    if (focusType == FOCUS_AUTO && currentFocuser)
        startFocusB->setEnabled(true);
    else
        startFocusB->setEnabled(false);

    stopFocusB->setEnabled(false);
    startLoopB->setEnabled(true);


    if (focusType == FOCUS_MANUAL)
    {
        if (currentFocuser)
        {
            focusOutB->setEnabled(true);
            focusInB->setEnabled(true);
        }

        startLoopB->setEnabled(true);
    }
    else
    {
        focusOutB->setEnabled(false);
        focusInB->setEnabled(false);
    }

    captureB->setEnabled(true);
}

void Focus::focusStarSelected(int x, int y)
{
    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    int offset = kcfg_focusBoxSize->value();
    int binx, biny;

    FITSView *targetImage = targetChip->getImage(FITS_FOCUS);
    disconnect(targetImage, SIGNAL(trackingStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)));

    targetChip->getBinning(&binx, &biny);
    int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
    targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

    x = (x - offset) * binx;
    y = (y - offset) * biny;
    int w=offset*2*binx;
    int h=offset*2*biny;

    if (x<minX)
        x=minX;
    if (y<minY)
        y=minY;
    if ((x+w)>maxW)
        w=maxW-x;
    if ((y+h)>maxH)
        h=maxH-y;

    //targetImage->updateMode(FITS_FOCUS);

    if (targetChip->canSubframe())
    {

        targetChip->getFrame(&orig_x, &orig_y, &orig_w, &orig_h);

        fx += x;
        fy += y;
        fw = w;
        fh = h;
        targetChip->setFocusFrame(fx, fy, fw, fh);
        frameModified=true;
        haveDarkFrame=false;
        calibrationState = CALIBRATE_NONE;
    }

    starSelected=true;

    targetImage->setTrackingBoxEnabled(false);
    targetImage->setTrackingBoxCenter(QPointF(fw/2.0,fh/2.0));
    targetImage->setTrackingBoxSize(QSize(fw/5.0, fh/5.0));

    capture();
}

void Focus::checkFocus(double requiredHFR)
{
    minimumRequiredHFR = requiredHFR;

    capture();
}

void Focus::toggleSubframe(bool enable)
{
    if (enable == false)
    {
        resetFocusFrame();
        kcfg_autoSelectStar->setChecked(false);
    }

    starSelected = false;
}

void Focus::filterChangeWarning(int index)
{
    if (index != 0)
        appendLogText(i18n("Warning: Only use filters for preview as they may interface with autofocus operation."));
}

void Focus::setExposure(double value)
{
    exposureIN->setValue(value);
}

void Focus::setBinning(int binX, int binY)
{
  INDI_UNUSED(binY);
  binningCombo->setCurrentIndex(binX-1);
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

void Focus::setAutoFocusStar(bool enable)
{
    kcfg_autoSelectStar->setChecked(enable);
}

void Focus::setAutoFocusSubFrame(bool enable)
{
    kcfg_subFrame->setChecked(enable);
}

void Focus::setAutoFocusParameters(int boxSize, int stepSize, int maxTravel, double tolerance)
{
    kcfg_focusBoxSize->setValue(boxSize);
    stepIN->setValue(stepSize);
    maxTravelIN->setValue(maxTravel);
    toleranceIN->setValue(tolerance);
}

bool Focus::setFocusMode(int mode)
{
    // If either of them is disabled, return false
    if ( (mode == 0 && manualModeR->isEnabled() == false) || (mode == 1 && AutoModeR->isEnabled() == false) )
        return false;

    if (mode == 0)
        manualModeR->setChecked(true);
    else
        AutoModeR->setChecked(true);

    checkCCD();

    return true;
}

void Focus::updateFocusStatus(bool status)
{
    m_autoFocusSuccesful = status;

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
            resetFocusFrame();
            return;
        }
    }

    resetFocusIteration = 0;

    emit autoFocusFinished(status, currentHFR);

    if (status)
        KNotification::event( QLatin1String( "FocusSuccessful" ) , i18n("Autofocus operation completed successfully"));
     else
            KNotification::event( QLatin1String( "FocusFailed" ) , i18n("Autofocus operation failed with errors"));
}

void Focus::checkAutoStarTimeout()
{
    if (starSelected == false && inAutoFocus)
    {
        appendLogText(i18n("No star was selected. Aborting..."));
        initialFocuserAbsPosition=-1;
        abort();
        updateFocusStatus(false);
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

}


