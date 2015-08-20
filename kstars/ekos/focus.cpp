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

#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "indi/indifilter.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsview.h"
#include "ekosmanager.h"

#include "ksnotify.h"
#include "kstars.h"
#include "focusadaptor.h"

#include <basedevice.h>

#define MAXIMUM_ABS_ITERATIONS      30
#define MAXIMUM_RESET_ITERATIONS    2
#define DEFAULT_SUBFRAME_DIM        128

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
    inAutoFocus       = false;
    inFocusLoop       = false;
    captureInProgress = false;
    inSequenceFocus   = false;
    starSelected      = false;
    frameModified     = false;
    resetFocus        = false;
    m_autoFocusSuccesful = false;
    filterPositionPending= false;

    HFRInc =0;
    noStarCount=0;
    reverseDir = false;

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

    connect(startFocusB, SIGNAL(clicked()), this, SLOT(startFocus()));
    connect(stopFocusB, SIGNAL(clicked()), this, SLOT(checkStopFocus()));

    connect(focusOutB, SIGNAL(clicked()), this, SLOT(FocusOut()));
    connect(focusInB, SIGNAL(clicked()), this, SLOT(FocusIn()));

    connect(captureB, SIGNAL(clicked()), this, SLOT(capture()));

    connect(AutoModeR, SIGNAL(toggled(bool)), this, SLOT(toggleAutofocus(bool)));

    connect(startLoopB, SIGNAL(clicked()), this, SLOT(startFraming()));

    connect(kcfg_subFrame, SIGNAL(toggled(bool)), this, SLOT(toggleSubframe(bool)));

    connect(resetFrameB, SIGNAL(clicked()), this, SLOT(resetFocusFrame()));
    connect(CCDCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkCCD(int)));
    connect(focuserCombo, SIGNAL(activated(int)), this, SLOT(checkFocuser(int)));
    connect(FilterCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkFilter(int)));
    connect(FilterPosCombo, SIGNAL(activated(int)), this, SLOT(updateFilterPos(int)));
    connect(lockFilterCheck, SIGNAL(toggled(bool)), this, SLOT(filterLockToggled(bool)));
    connect(filterCombo, SIGNAL(activated(int)), this, SLOT(filterChangeWarning(int)));

    connect(clearDataB, SIGNAL(clicked()) , this, SLOT(clearDataPoints()));

    lastFocusDirection = FOCUS_NONE;

    focusType = FOCUS_MANUAL;

    HFRPlot->axis( KPlotWidget::LeftAxis )->setLabel( xi18nc("Half Flux Radius", "HFR") );
    HFRPlot->axis( KPlotWidget::BottomAxis )->setLabel( xi18n("Iterations") );

    //HFRPlot->axis( KPlotWidget::LeftAxis )->setLabel( xi18nc("Half Flux Radius", "HFR") );
    //HFRPlot->axis( KPlotWidget::BottomAxis )->setLabel( xi18n("Absolute Position") );

    resetButtons();

    appendLogText(xi18n("Idle."));

    foreach(QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    exposureIN->setValue(Options::focusExposure());
    toleranceIN->setValue(Options::focusTolerance());
    stepIN->setValue(Options::focusTicks());
    kcfg_autoSelectStar->setChecked(Options::autoSelectStar());
    kcfg_focusBoxSize->setValue(Options::focusBoxSize());
    kcfg_focusXBin->setValue(Options::focusXBin());
    kcfg_focusYBin->setValue(Options::focusYBin());
    maxTravelIN->setValue(Options::focusMaxTravel());
    kcfg_subFrame->setChecked(Options::focusSubFrame());
    suspendGuideCheck->setChecked(Options::suspendGuiding());
    lockFilterCheck->setChecked(Options::lockFocusFilter());

}

Focus::~Focus()
{
    qDeleteAll(HFRAbsolutePoints);
    HFRAbsolutePoints.clear();
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
        stopFocus();
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
            checkCCD(i);
            return true;
        }

    return false;
}

void Focus::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
        ccdNum = CCDCaptureCombo->currentIndex();

    if (ccdNum <= CCDs.count())
        currentCCD = CCDs.at(ccdNum);

    syncCCDInfo();
}

void Focus::syncCCDInfo()
{

    if (currentCCD == NULL)
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    if (targetChip)
    {
        kcfg_focusXBin->setEnabled(targetChip->canBin());
        kcfg_focusYBin->setEnabled(targetChip->canBin());
        kcfg_subFrame->setEnabled(targetChip->canSubframe());
        kcfg_autoSelectStar->setEnabled(targetChip->canSubframe());
        if (targetChip->canBin())
        {
            int binx=1,biny=1;
            targetChip->getMaxBin(&binx, &biny);
            kcfg_focusXBin->setMaximum(binx);
            kcfg_focusYBin->setMaximum(biny);
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

        //if (!inAutoFocus && !inFocusLoop && !captureInProgress && !inSequenceFocus)
        targetChip->getFocusFrame(&fx, &fy, &fw, &fh);
    }
}

void Focus::addFilter(ISD::GDInterface *newFilter)
{
    foreach(ISD::GDInterface *filter, Filters)
    {
        if (!strcmp(filter->getDeviceName(), newFilter->getDeviceName()))
            return;
    }

    filterGroup->setEnabled(true);

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
            lockedFilterIndex = currentFilterIndex;
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
            checkFilter(i);
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
    }

    connect(currentFocuser, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processFocusNumber(INumberVectorProperty*)), Qt::UniqueConnection);

    AutoModeR->setEnabled(true);

    resetButtons();

    if (!inAutoFocus && !inFocusLoop && !captureInProgress && !inSequenceFocus)
        emit autoFocusFinished(true, -1);
}

void Focus::addCCD(ISD::GDInterface *newCCD, bool isPrimaryCCD)
{
    CCDCaptureCombo->addItem(newCCD->getDeviceName());

    CCDs.append(static_cast<ISD::CCD *>(newCCD));   

    if (isPrimaryCCD)
    {
        checkCCD(CCDs.count()-1);
        CCDCaptureCombo->setCurrentIndex(CCDs.count()-1);
    }
    else
    {
        checkCCD(0);
        CCDCaptureCombo->setCurrentIndex(0);
    }

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
        }
    }

}

void Focus::startFocus()
{
    lastFocusDirection = FOCUS_NONE;

    lastHFR = 0;

    if (canAbsMove)
    {
        absIterations = 0;
        getAbsFocusPosition();
        pulseDuration = stepIN->value();
    }
    else
      /* Start 1000 ms */
      pulseDuration=1000;

    inAutoFocus = true;
    m_autoFocusSuccesful = false;

    resetButtons();

    reverseDir = false;

    /*if (fw > 0 && fh > 0)
        starSelected= true;
    else
        starSelected= false;*/

    clearDataPoints();

    Options::setFocusTicks(stepIN->value());
    Options::setFocusTolerance(toleranceIN->value());
    Options::setFocusExposure(exposureIN->value());
    Options::setFocusXBin(kcfg_focusXBin->value());
    Options::setFocusYBin(kcfg_focusYBin->value());
    Options::setFocusMaxTravel(maxTravelIN->value());

    Options::setFocusSubFrame(kcfg_subFrame->isChecked());
    Options::setAutoSelectStar(kcfg_autoSelectStar->isChecked());
    Options::setSuspendGuiding(suspendGuideCheck->isChecked());
    Options::setLockFocusFilter(lockFilterCheck->isChecked());

    if (Options::verboseLogging())
        qDebug() << "Starting focus with pulseDuration " << pulseDuration;

    if (kcfg_autoSelectStar->isChecked())
        appendLogText(xi18n("Autofocus in progress..."));
    else
        appendLogText(xi18n("Please wait until image capture is complete..."));

    if (suspendGuideCheck->isChecked())
         emit suspendGuiding(true);

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
    }

    stopFocus();
}

void Focus::stopFocus()
{

    if (Options::verboseLogging())
        qDebug() << "Stopppig Focus";

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    inAutoFocus = false;
    inFocusLoop = false;
    //starSelected= false;
    minimumRequiredHFR    = -1;
    noStarCount = 0;
    //maxHFR=1;

    currentCCD->disconnect(this);

    targetChip->abortExposure();

    resetFrame();

    FITSView *targetImage = targetChip->getImage(FITS_FOCUS);
    if (targetImage)
        targetImage->updateMode(FITS_FOCUS);

    resetButtons();

    absIterations = 0;
    HFRInc=0;
    reverseDir = false;

}

void Focus::capture()
{
    if (currentCCD == NULL)
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    double seqExpose = exposureIN->value();
    CCDFrameType ccdFrame = FRAME_LIGHT;

    if (currentCCD->isConnected() == false)
    {
        appendLogText(xi18n("Error: Lost connection to CCD."));
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
            appendLogText(xi18n("Changing filter to %1", FilterPosCombo->currentText()));
            currentFilter->runCommand(INDI_SET_FILTER, &lockedFilterPosition);
            return;
        }
    }

    if (targetChip->canBin())
        targetChip->setBinning(kcfg_focusXBin->value(), kcfg_focusXBin->value());
    targetChip->setCaptureMode(FITS_FOCUS);
    targetChip->setCaptureFilter( (FITSScale) filterCombo->currentIndex());

    if (ISOCombo->isEnabled() && ISOCombo->currentIndex() != -1 && targetChip->getISOIndex() != ISOCombo->currentIndex())
        targetChip->setISOIndex(ISOCombo->currentIndex());

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    targetChip->setFrameType(ccdFrame);

    /*if (subX >= 0 && subY >=0 && subW > 0 && subH > 0)
        targetChip->setFrame(subX, subY, subW, subH);
    else*/
    if (fw == 0 || fh == 0)
        targetChip->getFrame(&fx, &fy, &fw, &fh);

     targetChip->setFrame(fx, fy, fw, fh);

     if (fx != orig_x || fy != orig_y || fw != orig_w || fh != orig_h)
         frameModified = true;

    captureInProgress = true;

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

void Focus::FocusIn(int ms)
{
  if (currentFocuser == NULL)
       return;

  if (currentFocuser->isConnected() == false)
  {
            appendLogText(xi18n("Error: Lost connection to Focuser."));
            return;
  }

    if (ms == -1)
        ms = stepIN->value();

    if (Options::verboseLogging())
        qDebug() << "Focus in (" << ms << ")" ;

    lastFocusDirection = FOCUS_IN;

     currentFocuser->focusIn();

     if (canAbsMove)
         currentFocuser->absMoveFocuser(currentPosition-ms);
     else
       currentFocuser->moveFocuser(ms);


    appendLogText(xi18n("Focusing inward..."));
}

void Focus::FocusOut(int ms)
{
    if (currentFocuser == NULL)
         return;

    if (currentFocuser->isConnected() == false)
    {
       appendLogText(xi18n("Error: Lost connection to Focuser."));
       return;
    }

    lastFocusDirection = FOCUS_OUT;

    if (ms == -1)
        ms = stepIN->value();

    if (Options::verboseLogging())
        qDebug() << "Focus out (" << ms << ")" ;

     currentFocuser->focusOut();

    if (canAbsMove)
           currentFocuser->absMoveFocuser(currentPosition+ms);
    else
            currentFocuser->moveFocuser(ms);

    appendLogText(xi18n("Focusing outward..."));

}

void Focus::newFITS(IBLOB *bp)
{
    INDI_UNUSED(bp);
    QString HFRText;

    // Ignore guide head if there is any.
    if (!strcmp(bp->name, "CCD2"))
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    FITSView *targetImage = targetChip->getImage(FITS_FOCUS);

    if (targetImage == NULL)
    {
        appendLogText(xi18n("FITS image failed to load, aborting..."));
        stopFocus();
        return;
    }

    if (captureInProgress && inFocusLoop == false && inAutoFocus==false)
    {
            captureB->setEnabled(true);
            stopFocusB->setEnabled(false);
    }

    captureInProgress = false;

    FITSData *image_data = targetChip->getImageData();

    currentCCD->disconnect(this);

    image_data->findStars();

    currentHFR= image_data->getHFR(HFR_MAX);

    if (currentHFR == -1)
    {
        currentHFR = image_data->getHFR();
    }

    if (Options::verboseLogging())
        qDebug() << "newFITS: Current HFR " << currentHFR;

    HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    if (inFocusLoop == false && focusType == FOCUS_MANUAL && lastHFR == -1)
            appendLogText(xi18n("FITS received. No stars detected."));

    HFROut->setText(HFRText);

    if (currentHFR > 0)
    {
        if (currentHFR > maxHFR)
            maxHFR = currentHFR;

        HFRPoint *p = new HFRPoint();
        p->HFR = currentHFR;
        p->pos = HFRIterativePoints.size()+1;
        HFRIterativePoints.append(p);

        if (focusType == FOCUS_MANUAL)
            drawHFRPlot();
    }

    if (inFocusLoop)
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
                appendLogText(xi18n("Failed to automatically select a star. Please select a star manually."));
                targetImage->updateMode(FITS_GUIDE);
                targetImage->setGuideBoxSize(kcfg_focusBoxSize->value());
                if (fw == 0 || fh == 0)
                    targetChip->getFrame(&fx, &fy, &fw, &fh);
                targetImage->setGuideSquare(fw/2, fh/2);
                connect(targetImage, SIGNAL(guideStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)), Qt::UniqueConnection);
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
            }
            else
                targetChip->getFrame(&fx, &fy, &fw, &fh);

            starSelected=true;

            capture();

            return;


        }
        else if (kcfg_subFrame->isEnabled() && kcfg_subFrame->isChecked())
        {
            appendLogText(xi18n("Capture complete. Select a star to focus."));
            targetImage->updateMode(FITS_GUIDE);
            targetImage->setGuideBoxSize(kcfg_focusBoxSize->value());
            targetImage->setGuideSquare(fw/2, fh/2);
            connect(targetImage, SIGNAL(guideStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)), Qt::UniqueConnection);
            return;
        }
    }

    if (minimumRequiredHFR >= 0)
    {
        if (currentHFR == -1)
        {
            if (noStarCount++ < 3)
            {
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
           startFocus();
        }
        else
        {
            updateFocusStatus(true);
        }

        minimumRequiredHFR = -1;

        return;
    }

    if (focusType == FOCUS_MANUAL || inAutoFocus==false)
        return;

    if (canAbsMove)
        autoFocusAbs();
    else
        autoFocusRel();

}

void Focus::clearDataPoints()
{
    maxHFR=1;
    qDeleteAll(HFRIterativePoints);
    HFRIterativePoints.clear();
    qDeleteAll(HFRAbsolutePoints);
    HFRAbsolutePoints.clear();
}

void Focus::drawHFRPlot()
{
    HFRPlot->removeAllPlotObjects();

    KPlotObject * HFRObj = NULL;

    if (focusType == FOCUS_AUTO)
    {
        HFRObj = new KPlotObject( Qt::red, KPlotObject::Points, 2 );

        foreach(HFRPoint *p, HFRAbsolutePoints)
            HFRObj->addPoint(p->pos, p->HFR);

        HFRPlot->addPlotObject(HFRObj);
        HFRPlot->setLimits(minPos-pulseDuration, maxPos+pulseDuration, currentHFR/1.5, maxHFR );
        HFRPlot->axis( KPlotWidget::BottomAxis )->setLabel( xi18n("Absolute Position") );
    }
    else
    {
        HFRObj = new KPlotObject( Qt::red, KPlotObject::Lines, 2 );

        foreach(HFRPoint *p, HFRIterativePoints)
            HFRObj->addPoint(p->pos, p->HFR);

        HFRPlot->addPlotObject(HFRObj);
        HFRPlot->setLimits(1, HFRIterativePoints.size() + 1, currentHFR/1.5, maxHFR);
        HFRPlot->axis( KPlotWidget::BottomAxis )->setLabel( xi18n("Iterations") );
    }

    HFRPlot->update();
}

void Focus::autoFocusAbs()
{
    static int lastHFRPos=0, minHFRPos=0, initSlopePos=0, focusOutLimit=0, focusInLimit=0;
    static double minHFR=0, initSlopeHFR=0;
    double targetPosition=0, delta=0;

    QString deltaTxt = QString("%1").arg(fabs(currentHFR-minHFR)*100.0, 0,'g', 3);
    QString HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    if (Options::verboseLogging())
    {
        qDebug() << "########################################";
        qDebug() << "========================================";
        qDebug() << "Current HFR: " << currentHFR << " Current Position: " << currentPosition;
        qDebug() << "Last minHFR: " << minHFR << " Last MinHFR Pos: " << minHFRPos;
        qDebug() << "Delta: " << deltaTxt << "%";
        qDebug() << "========================================";
    }

    if (minHFR)
         appendLogText(xi18n("FITS received. HFR %1 @ %2. Delta (%3%)", HFRText, currentPosition, deltaTxt));
    else
        appendLogText(xi18n("FITS received. HFR %1 @ %2.", HFRText, currentPosition));

    if (++absIterations > MAXIMUM_ABS_ITERATIONS)
    {
        appendLogText(xi18n("Autofocus failed to reach proper focus. Try increasing tolerance value."));
        stopFocus();
        updateFocusStatus(false);
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        if (noStarCount++ < 3)
        {
            appendLogText(xi18n("No stars detected, capturing again..."));
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

    if (HFRAbsolutePoints.empty())
    {
        maxPos=1;
        minPos=1e6;
    }

    if (currentPosition > maxPos)
        maxPos = currentPosition;
    if (currentPosition < minPos)
        minPos = currentPosition;

    HFRPoint *p = new HFRPoint();

    p->HFR = currentHFR;
    p->pos = currentPosition;

    HFRAbsolutePoints.append(p);

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
                    appendLogText(xi18n("Change in HFR is too small. Try increasing the step size or decreasing the tolerance."));
                    stopFocus();
                    updateFocusStatus(false);
                }
                else
                {
                    appendLogText(xi18n("Autofocus complete."));
                    stopFocus();
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

                    if (Options::verboseLogging())
                        qDebug() << "Setting initial slop to " << initSlopePos << " @ HFR " << initSlopeHFR;
                }

                // Let's now limit the travel distance of the focuser
                if (lastFocusDirection == FOCUS_OUT && lastHFRPos < focusInLimit && fabs(currentHFR - lastHFR) > 0.1)
                {
                    focusInLimit = lastHFRPos;
                    if (Options::verboseLogging())
                        qDebug() << "New FocusInLimit " << focusInLimit;
                }
                else if (lastFocusDirection == FOCUS_IN && lastHFRPos > focusOutLimit && fabs(currentHFR - lastHFR) > 0.1)
                {
                    focusOutLimit = lastHFRPos;
                    if (Options::verboseLogging())
                        qDebug() << "New FocusOutLimit " << focusOutLimit;
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
                    if (Options::verboseLogging())
                        qDebug() << "Using slope to calculate target pulse...";
                }
                // Otherwise proceed iteratively
                else
                {
                     if (lastFocusDirection == FOCUS_IN)
                         targetPosition = currentPosition - pulseDuration;
                     else
                         targetPosition = currentPosition + pulseDuration;

                     if (Options::verboseLogging())
                        qDebug() << "Proceeding iteratively to next target pulse ...";
                }

                if (Options::verboseLogging())
                    qDebug() << "V-Curve Slope " << slope << " current Position " << currentPosition << " targetPosition " << targetPosition;

                lastHFR = currentHFR;

                // Let's keep track of the minimum HFR
                if (lastHFR < minHFR)
                {
                    minHFR = lastHFR;
                    minHFRPos = currentPosition;
                    if (Options::verboseLogging())
                    {
                        qDebug() << "new minHFR " << minHFR << " @ positioin " << minHFRPos;
                        qDebug() << "########################################";
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
                if (HFRInc <= 1)
                {
                    capture();
                    return;
                }
                // Looks like we're going away from optimal HFR
                else
                {

                    lastHFR = currentHFR;
                    lastHFRPos = currentPosition;
                    initSlopeHFR=0;
                    HFRInc=0;

                    if (Options::verboseLogging())
                        qDebug() << "We are going away from optimal HFR ";

                    // Let's set new limits
                    if (lastFocusDirection == FOCUS_IN)
                    {
                        focusInLimit = currentPosition;
                        if (Options::verboseLogging())
                            qDebug() << "Setting focus IN limit to " << focusInLimit;
                    }
                    else
                    {
                        focusOutLimit = currentPosition;
                        if (Options::verboseLogging())
                            qDebug() << "Setting focus OUT limit to " << focusOutLimit;
                    }

                    // Decrease pulse
                    pulseDuration = pulseDuration * 0.75;

                    // Let's get close to the minimum HFR position so far detected
                    if (lastFocusDirection == FOCUS_OUT)
                          targetPosition = minHFRPos-pulseDuration/2;
                     else
                          targetPosition = minHFRPos+pulseDuration/2;

                    if (Options::verboseLogging())
                        qDebug() << "new targetPosition " << targetPosition;

                }
            }

        // Limit target Pulse to algorithm limits
        if (focusInLimit != 0 && lastFocusDirection == FOCUS_IN && targetPosition < focusInLimit)
        {
            targetPosition = focusInLimit;
            if (Options::verboseLogging())
                qDebug() << "Limiting target pulse to focus in limit " << targetPosition;
        }
        else if (focusOutLimit != 0 && lastFocusDirection == FOCUS_OUT && targetPosition > focusOutLimit)
        {
            targetPosition = focusOutLimit;
            if (Options::verboseLogging())
                qDebug() << "Limiting target pulse to focus out limit " << targetPosition;
        }

        // Limit target pulse to focuser limits
        if (targetPosition < absMotionMin)
            targetPosition = absMotionMin;
        else if (targetPosition > absMotionMax)
            targetPosition = absMotionMax;

        // Ops, we can't go any further, we're done.
        if (targetPosition == currentPosition)
        {
            appendLogText(xi18n("Autofocus complete."));
            stopFocus();
            emit suspendGuiding(false);
            updateFocusStatus(true);
            return;
        }

        // Ops, deadlock
        if (focusOutLimit && focusOutLimit == focusInLimit)
        {
            appendLogText(xi18n("Deadlock reached. Please try again with different settings."));
            stopFocus();
            updateFocusStatus(false);
            return;
        }

        if (fabs(targetPosition - initialFocuserAbsPosition) > maxTravelIN->value())
        {
            if (Options::verboseLogging())
                qDebug() << "targetPosition (" << targetPosition << ") - initHFRAbsPos (" << initialFocuserAbsPosition << ") exceeds maxTravel distance of " << maxTravelIN->value();

            appendLogText("Maximum travel limit reached. Autofocus aborted.");
            stopFocus();
            updateFocusStatus(false);
            if (Options::verboseLogging())
                qDebug() << "Maximum travel limit reached. Autofocus aborted.";
            break;

        }

        // Get delta for next move
        delta = (targetPosition - currentPosition);

        if (Options::verboseLogging())
        {
            qDebug() << "delta (targetPosition - currentPosition) " << delta;
            qDebug() << "Focusing " << ((delta < 0) ? "IN"  : "OUT");
            qDebug() << "########################################";
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
    QString deltaTxt = QString("%1").arg(fabs(currentHFR-lastHFR)*100.0, 0,'g', 2);
    QString HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    appendLogText(xi18n("FITS received. HFR %1. Delta (%2%)", HFRText, deltaTxt));

    if (pulseDuration <= 32)
    {
        appendLogText(xi18n("Autofocus failed to reach proper focus. Try adjusting the tolerance value."));
        stopFocus();
        updateFocusStatus(false);
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        if (noStarCount++ < 3)
        {
            appendLogText(xi18n("No stars detected, capturing again..."));
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
            FocusIn(pulseDuration);
            break;

        case FOCUS_IN:
            if (fabs(currentHFR - lastHFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
            {
                appendLogText(xi18n("Autofocus complete."));                
                stopFocus();
                emit suspendGuiding(false);
                updateFocusStatus(true);
                break;
            }
            else if (currentHFR < lastHFR)
            {
                lastHFR = currentHFR;
                FocusIn(pulseDuration);
                HFRInc=0;
            }
            else
            {
                HFRInc++;


                if (HFRInc <= 1)
                {
                    capture();
                    return;
                }
                else
                {

                    lastHFR = currentHFR;

                    HFRInc=0;

                    /*if (reverseDir)
                        pulseDuration /= 2;

                    if (canAbsMove)
                    {
                        if (reverseDir)
                            FocusOut(pulseDuration*3);
                        else
                        {
                            reverseDir = true;
                            FocusOut(pulseDuration*2);
                        }
                    }
                    else
                        FocusOut(pulseDuration);
                        */
                    pulseDuration *= 0.75;
                    FocusOut(pulseDuration);
                }
            }

            break;

    case FOCUS_OUT:
        if (fabs(currentHFR - lastHFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
        {
            appendLogText(xi18n("Autofocus complete."));
            stopFocus();
            emit suspendGuiding(false);
            updateFocusStatus(true);
            break;
        }
        else if (currentHFR < lastHFR)
        {
            lastHFR = currentHFR;
            FocusOut(pulseDuration);
            HFRInc=0;
        }
        else
        {
            HFRInc++;

            if (HFRInc <= 1)
                capture();
            else
            {

                lastHFR = currentHFR;

                HFRInc=0;

                /*if (reverseDir)
                    pulseDuration /= 2;

                if (canAbsMove)
                {
                    if (reverseDir)
                        FocusIn(pulseDuration*3);
                    else
                    {
                        reverseDir = true;
                        FocusIn(pulseDuration*2);
                    }
                }
                else
                    FocusIn(pulseDuration);*/

                pulseDuration *= 0.75;
                FocusIn(pulseDuration);
            }
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
    }

    if (!strcmp(nvp->name, "ABS_FOCUS_POSITION"))
    {
       INumber *pos = IUFindNumber(nvp, "FOCUS_ABSOLUTE_POSITION");
       if (pos)
           currentPosition = pos->value;

       if (resetFocus && nvp->s == IPS_OK)
       {
           resetFocus = false;
           appendLogText(xi18n("Restarting autofocus process..."));
           startFocus();
       }

       if (canAbsMove && inAutoFocus)
       {
           if (nvp->s == IPS_OK && captureInProgress == false)
               capture();
           else if (nvp->s == IPS_ALERT)
           {
               appendLogText(xi18n("Focuser error, check INDI panel."));
               stopFocus();
               updateFocusStatus(false);
           }

       }

       return;
    }

    if (!strcmp(nvp->name, "FOCUS_TIMER"))
    {       
        if (canAbsMove == false && inAutoFocus)
        {
            if (nvp->s == IPS_OK && captureInProgress == false)
                capture();
            else if (nvp->s == IPS_ALERT)
            {
                appendLogText(xi18n("Focuser error, check INDI panel."));
                stopFocus();
                updateFocusStatus(false);
            }

        }

        return;
    }

}

void Focus::appendLogText(const QString &text)
{

    logText.insert(0, xi18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    emit newLog();
}

void Focus::clearLog()
{
    logText.clear();
    emit newLog();
}

void Focus::startFraming()
{
    inFocusLoop = true;

    resetButtons();

    appendLogText(xi18n("Starting continuous exposure..."));

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

    disconnect(this, SLOT(focusStarSelected(int,int)));

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

    FITSView *targetImage = targetChip->getImage(FITS_FOCUS);
    targetImage->updateMode(FITS_FOCUS);

    if (targetChip->canSubframe())
    {

        targetChip->getFrame(&orig_x, &orig_y, &orig_w, &orig_h);
        /*orig_x = fx;
        orig_y = fy;
        orig_w = fw;
        orig_h = fh;*/

        fx += x;
        fy += y;
        fw = w;
        fh = h;
        targetChip->setFocusFrame(fx, fy, fw, fh);
        frameModified=true;
    }

    starSelected=true;

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
   kcfg_focusXBin->setValue(binX);
   kcfg_focusYBin->setValue(binY);
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

void Focus::setAutoFocusOptions(bool selectAutoStar, bool useSubFrame)
{
    kcfg_autoSelectStar->setChecked(selectAutoStar);
    kcfg_subFrame->setChecked(useSubFrame);

}

void Focus::setAutoFocusParameters(int boxSize, int stepSize, int maxTravel, double tolerance)
{
    kcfg_focusBoxSize->setValue(boxSize);
    stepIN->setValue(stepSize);
    maxTravelIN->setValue(maxTravel);
    toleranceIN->setValue(tolerance);
}

void Focus::setFocusMode(int mode)
{
    if (mode == 0)
        manualModeR->setChecked(true);
    else
        AutoModeR->setChecked(true);

    checkCCD();
}

void Focus::updateFocusStatus(bool status)
{
    m_autoFocusSuccesful = status;

    // In case of failure, go back to last position if the focuser is absolute
    if (status == false &&  canAbsMove && currentFocuser)
    {
        currentFocuser->absMoveFocuser(initialFocuserAbsPosition);
        appendLogText(xi18n("Autofocus failed, moving back to initial focus position %1.", initialFocuserAbsPosition));

        // If we're doing in sequence focusing using an absolute focuser, let's retry focusing starting from last known good position before we give up
        if (inSequenceFocus && resetFocusIteration++ < MAXIMUM_RESET_ITERATIONS && resetFocus == false)
        {
            resetFocus = true;
            return;
        }
    }

    resetFocusIteration = 0;

    emit autoFocusFinished(status, currentHFR);

    if (Options::playFocusAlarm())
    {
        if (status)
            KSNotify::play(KSNotify::NOTIFY_OK);
        else
            KSNotify::play(KSNotify::NOTIFY_ERROR);
    }
}

}


