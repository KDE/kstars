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
#include <KPlotWidget>
#include <KPlotObject>
#include <KPlotAxis>

#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "indi/indifilter.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsview.h"

#include <basedevice.h>

#define MAXIMUM_ABS_ITERATIONS  30
#define DEFAULT_SUBFRAME_DIM    128

//#define FOCUS_DEBUG

namespace Ekos
{

Focus::Focus()
{
    setupUi(this);

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

    HFRInc =0;
    noStarCount=0;
    reverseDir = false;

    pulseDuration = 1000;

    fx=fy=fw=fh=0;

    deltaHFR = 0;

    connect(startFocusB, SIGNAL(clicked()), this, SLOT(startFocus()));
    connect(stopFocusB, SIGNAL(clicked()), this, SLOT(checkStopFocus()));

    connect(focusOutB, SIGNAL(clicked()), this, SLOT(FocusOut()));
    connect(focusInB, SIGNAL(clicked()), this, SLOT(FocusIn()));

    connect(captureB, SIGNAL(clicked()), this, SLOT(capture()));

    connect(AutoModeR, SIGNAL(toggled(bool)), this, SLOT(toggleAutofocus(bool)));

    connect(startLoopB, SIGNAL(clicked()), this, SLOT(startLooping()));

    connect(kcfg_subFrame, SIGNAL(toggled(bool)), this, SLOT(subframeUpdated(bool)));

    connect(CCDCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkCCD(int)));
    connect(focuserCombo, SIGNAL(activated(int)), this, SLOT(checkFocuser(int)));
    connect(FilterCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkFilter(int)));
    connect(FilterPosCombo, SIGNAL(activated(int)), this, SLOT(updateFilterPos(int)));
    connect(lockFilterCheck, SIGNAL(toggled(bool)), this, SLOT(filterLockToggled(bool)));

    lastFocusDirection = FOCUS_NONE;

    focusType = FOCUS_MANUAL;

    HFRPlot->axis( KPlotWidget::LeftAxis )->setLabel( i18nc("Half Flux Radius", "HFR") );
    HFRPlot->axis( KPlotWidget::BottomAxis )->setLabel( i18n("Absolute Position") );

    resetButtons();

    appendLogText(i18n("Idle."));

    foreach(QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    exposureIN->setValue(Options::focusExposure());
    toleranceIN->setValue(Options::focusTolerance());
    stepIN->setValue(Options::focusTicks());
    kcfg_autoSelectStar->setChecked(Options::autoSelectStar());
    kcfg_focusBoxSize->setValue(Options::focusBoxSize());
    kcfg_focusXBin->setValue(Options::focusXBin());
    kcfg_focusYBin->setValue(Options::focusYBin());
    maxTravel->setValue(Options::focusMaxTravel());
    kcfg_subFrame->setChecked(Options::focusSubFrame());
    suspendGuideCheck->setChecked(Options::suspendGuiding());
    lockFilterCheck->setChecked(Options::lockFocusFilter());

}

Focus::~Focus()
{
    qDeleteAll(HFRPoints);
    HFRPoints.clear();
}

void Focus::toggleAutofocus(bool enable)
{
    if (enable)
        focusType = FOCUS_AUTO;
    else
        focusType = FOCUS_MANUAL;

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

        if (targetChip && !inAutoFocus && !inFocusLoop && !captureInProgress && !inSequenceFocus)
        {
            targetChip->resetFrame();
        }
    }
}

void Focus::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
        ccdNum = CCDCaptureCombo->currentIndex();

    if (ccdNum <= CCDs.count())
        currentCCD = CCDs.at(ccdNum);

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    if (targetChip)
    {
        kcfg_focusXBin->setEnabled(targetChip->canBin());
        kcfg_focusYBin->setEnabled(targetChip->canBin());
        kcfg_subFrame->setEnabled(targetChip->canSubframe());
        if (targetChip->canBin())
        {
            int binx=1,biny=1;
            targetChip->getMaxBin(&binx, &biny);
            kcfg_focusXBin->setMaximum(binx);
            kcfg_focusYBin->setMaximum(biny);
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

void Focus::checkFilter(int filterNum)
{
    if (filterNum == -1)
        filterNum = FilterCaptureCombo->currentIndex();

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
        FilterPosCombo->setCurrentIndex( (int) filterSlot->np[0].value-1);
    else
        FilterPosCombo->setCurrentIndex(lastLockFilterPos);

}

void Focus::filterLockToggled(bool enable)
{
    if (enable)
        lastLockFilterPos = FilterPosCombo->currentIndex();
    else if (filterSlot != NULL)
        FilterPosCombo->setCurrentIndex(filterSlot->np[0].value-1);
}

void Focus::updateFilterPos(int index)
{
    lastLockFilterPos = index;
}

void Focus::addFocuser(ISD::GDInterface *newFocuser)
{
    focuserCombo->addItem(newFocuser->getDeviceName());

    Focusers.append(static_cast<ISD::Focuser*>(newFocuser));

    currentFocuser = static_cast<ISD::Focuser *> (newFocuser);

    checkFocuser();
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

    connect(currentFocuser, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processFocusProperties(INumberVectorProperty*)));

    AutoModeR->setEnabled(true);

    resetButtons();

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
           pulseStep = absMove->np[0].value;
           absMotionMax  = absMove->np[0].max;
           absMotionMin  = absMove->np[0].min;
        }
    }

}

void Focus::startFocus()
{
    lastFocusDirection = FOCUS_NONE;

    HFR = 0;

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

    resetButtons();

    reverseDir = false;

    /*if (fw > 0 && fh > 0)
        starSelected= true;
    else
        starSelected= false;*/

    qDeleteAll(HFRPoints);
    HFRPoints.clear();

    Options::setFocusTicks(stepIN->value());
    Options::setFocusTolerance(toleranceIN->value());
    Options::setFocusExposure(exposureIN->value());
    Options::setFocusXBin(kcfg_focusXBin->value());
    Options::setFocusYBin(kcfg_focusYBin->value());
    Options::setFocusMaxTravel(maxTravel->value());

    Options::setFocusSubFrame(kcfg_subFrame->isChecked());
    Options::setAutoSelectStar(kcfg_autoSelectStar->isChecked());
    Options::setSuspendGuiding(suspendGuideCheck->isChecked());
    Options::setLockFocusFilter(lockFilterCheck->isChecked());

    #ifdef FOCUS_DEBUG
    qDebug() << "Starting focus with pulseDuration " << pulseDuration << endl;
    #endif

    if (kcfg_autoSelectStar->isChecked())
        appendLogText(i18n("Autofocus in progress..."));
    else
        appendLogText(i18n("Please wait until image capture is complete..."));

    if (suspendGuideCheck->isChecked())
         emit suspendGuiding(true);

    capture();
}

void Focus::checkStopFocus()
{
    if (inSequenceFocus == true)
    {
        inSequenceFocus = false;
        emit autoFocusFinished(false);
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

    #ifdef FOCUS_DEBUG
    qDebug() << "Stopppig Focus" << endl;
    #endif

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    inAutoFocus = false;
    inFocusLoop = false;
    //starSelected= false;
    deltaHFR    = 0;
    noStarCount = 0;

    currentCCD->disconnect(this);

    targetChip->abortExposure();
    if (targetChip->canSubframe())
        targetChip->resetFrame();

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
        appendLogText(i18n("Error: Lost connection to CCD."));
        return;
    }

    if (currentFilter != NULL)
    {
        if (currentFilter->isConnected() == false)
        {
            appendLogText(i18n("Error: Lost connection to filter wheel."));
            return;
        }

        int filterPos = FilterPosCombo->currentIndex() + 1;
        currentFilter->runCommand(INDI_SET_FILTER, &filterPos);
    }

    if (targetChip->canBin())
        targetChip->setBinning(kcfg_focusXBin->value(), kcfg_focusXBin->value());
    targetChip->setCaptureMode(FITS_FOCUS);
    targetChip->setCaptureFilter( (FITSScale) filterCombo->currentIndex());

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    targetChip->setFrameType(ccdFrame);

    /*if (subX >= 0 && subY >=0 && subW > 0 && subH > 0)
        targetChip->setFrame(subX, subY, subW, subH);
    else*/
    if (fw == 0 || fh == 0)
        targetChip->getFrame(&fx, &fy, &fw, &fh);

     targetChip->setFrame(fx, fy, fw, fh);

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
            appendLogText(i18n("Error: Lost connection to Focuser."));
            return;
  }

    if (ms == -1)
        ms = stepIN->value();

    #ifdef FOCUS_DEBUG
    qDebug() << "Focus in (" << ms << ")"  << endl;
    #endif

    lastFocusDirection = FOCUS_IN;

     currentFocuser->focusIn();

     if (canAbsMove)
         currentFocuser->absMoveFocuser(pulseStep-ms);
     else
       currentFocuser->moveFocuser(ms);


    appendLogText(i18n("Focusing inward..."));
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

    #ifdef FOCUS_DEBUG
    qDebug() << "Focus out (" << ms << ")"  << endl;
    #endif

     currentFocuser->focusOut();

    if (canAbsMove)
           currentFocuser->absMoveFocuser(pulseStep+ms);
    else
            currentFocuser->moveFocuser(ms);

    appendLogText(i18n("Focusing outward..."));

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
        appendLogText(i18n("FITS image failed to load, aborting..."));
        stopFocus();
        return;
    }

    if (captureInProgress && inFocusLoop == false && inAutoFocus==false)
    {
            captureB->setEnabled(true);
            stopFocusB->setEnabled(false);
    }

    captureInProgress = false;

    FITSImage *image_data = targetChip->getImageData();

    currentCCD->disconnect(this);

    image_data->findStars();

    double currentHFR= image_data->getHFR(HFR_MAX);

    if (currentHFR == -1)
    {
        currentHFR = image_data->getHFR();
    }

    #ifdef FOCUS_DEBUG
    qDebug() << "newFITS: Current HFR " << currentHFR << endl;
    #endif

    HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    if (inFocusLoop == false && focusType == FOCUS_MANUAL && HFR == -1)
            appendLogText(i18n("FITS received. No stars detected."));

    HFROut->setText(HFRText);

    if (deltaHFR > 0)
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
                emit autoFocusFinished(false);
            }
        }
        else if (currentHFR > deltaHFR)
        {
           inSequenceFocus = true;
           AutoModeR->setChecked(true);
           startFocus();
        }
        else
            emit autoFocusFinished(true);

        deltaHFR = 0;

        return;
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

        if (kcfg_autoSelectStar->isChecked() && focusType == FOCUS_AUTO)
        {
            Edge *maxStar = image_data->getMaxHFRStar();
            if (maxStar == NULL)
            {
                appendLogText(i18n("Failed to automatically select a star. Please select a star manually."));
                targetImage->updateMode(FITS_GUIDE);
                targetImage->setGuideBoxSize(kcfg_focusBoxSize->value());
                if (fw == 0 || fh == 0)
                    targetChip->getFrame(&fx, &fy, &fw, &fh);
                targetImage->setGuideSquare(fw/2, fh/2);
                connect(targetImage, SIGNAL(guideStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)));
                return;
            }

            if (kcfg_subFrame->isEnabled() && kcfg_subFrame->isChecked())
            {
                int offset = kcfg_focusBoxSize->value();
                int subX=(maxStar->x - offset) * subBinX;
                int subY=(maxStar->y - offset) * subBinY;
                int subW=offset*2*subBinX;
                int subH=offset*2*subBinY;

                if (subX<0)
                    subX=0;
                if (subY<0)
                    subY=0;
                if ((subW+subX)>fw)
                    subW=fw-subX;
                if ((subH+subY)>fh)
                    subH=fh-subY;

                if (subW <= 0)
                    subW = DEFAULT_SUBFRAME_DIM;
                if (subH <= 0)
                    subH = DEFAULT_SUBFRAME_DIM;


                targetChip->setFocusFrame(subX, subY, subW, subH);
                fx=subX;
                fy=subY;
                fw=subW;
                fh=subH;
            }
            else
                targetChip->getFrame(&fx, &fy, &fw, &fh);

            starSelected=true;

            capture();

            return;


        }
        else
        {
            appendLogText(i18n("Capture complete. Select a star to focus."));
            targetImage->updateMode(FITS_GUIDE);
            targetImage->setGuideBoxSize(kcfg_focusBoxSize->value());
            targetImage->setGuideSquare(fw/2, fh/2);
            connect(targetImage, SIGNAL(guideStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)));
            return;
        }
    }

    if (focusType == FOCUS_MANUAL || inAutoFocus==false)
        return;

    if (canAbsMove)
        autoFocusAbs(currentHFR);
    else
        autoFocusRel(currentHFR);

}


void Focus::autoFocusAbs(double currentHFR)
{
    static int initHFRPos=0, lastHFRPos=0, minHFRPos=0, initSlopePos=0, initPulseDuration=0, focusOutLimit=0, focusInLimit=0, minPos=1e6, maxPos=0;
    static double initHFR=0, minHFR=0, maxHFR=1,initSlopeHFR=0;
    double targetPulse=0, delta=0;

    QString deltaTxt = QString("%1").arg(fabs(currentHFR-minHFR)*100.0, 0,'g', 3);
    QString HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    #ifdef FOCUS_DEBUG
    qDebug() << "########################################" << endl;
    qDebug() << "========================================" << endl;
    qDebug() << "Current HFR: " << currentHFR << " Current Position: " << pulseStep << endl;
    qDebug() << "Last minHFR: " << minHFR << " Last MinHFR Pos: " << minHFRPos << endl;
    qDebug() << "Delta: " << deltaTxt << "%" << endl;
    qDebug() << "========================================" << endl;
    #endif

    if (minHFR)
         appendLogText(i18n("FITS received. HFR %1 @ %2. Delta (%3%)", HFRText, pulseStep, deltaTxt));
    else
        appendLogText(i18n("FITS received. HFR %1 @ %2.", HFRText, pulseStep));

    if (++absIterations > MAXIMUM_ABS_ITERATIONS)
    {
        appendLogText(i18n("Autofocus failed to reach proper focus. Try increasing tolerance value."));
        stopFocus();
        emit autoFocusFinished(false);
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        if (noStarCount++ < 3)
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

    if (currentHFR > maxHFR || HFRPoints.empty())
    {
        maxHFR = currentHFR;

        if (HFRPoints.empty())
        {
            maxPos=1;
            minPos=1e6;
        }
    }

    if (pulseStep > maxPos)
        maxPos = pulseStep;
    if (pulseStep < minPos)
        minPos = pulseStep;

    HFRPoint *p = new HFRPoint();

    p->HFR = currentHFR;
    p->pos = pulseStep;

    HFRPoints.append(p);

    HFRPlot->removeAllPlotObjects();

    HFRPlot->setLimits(minPos-pulseDuration, maxPos+pulseDuration, currentHFR/1.5, maxHFR );

    KPlotObject *hfrObj = new KPlotObject( Qt::red, KPlotObject::Points, 2 );

    foreach(HFRPoint *p, HFRPoints)
        hfrObj->addPoint(p->pos, p->HFR);

    HFRPlot->addPlotObject(hfrObj);

    HFRPlot->update();

    switch (lastFocusDirection)
    {
        case FOCUS_NONE:
            HFR = currentHFR;
            initHFRPos = pulseStep;
            initHFR=HFR;
            minHFR=currentHFR;
            minHFRPos=pulseStep;
            HFRDec=0;
            HFRInc=0;
            focusOutLimit=0;
            focusInLimit=0;
            initPulseDuration=pulseDuration;
            FocusOut(pulseDuration);
            break;

        case FOCUS_IN:
        case FOCUS_OUT:
        if (reverseDir && focusInLimit && focusOutLimit && fabs(currentHFR - minHFR) < (toleranceIN->value()/100.0) && HFRInc == 0 )
            {
                if (absIterations <= 2)
                {
                    appendLogText(i18n("Change in HFR is too small. Try increasing the step size or decreasing the tolerance."));
                    stopFocus();
                    emit autoFocusFinished(false);
                }
                else
                {
                    appendLogText(i18n("Autofocus complete."));
                    stopFocus();
                    emit suspendGuiding(false);
                    emit autoFocusFinished(true);                    
                }
                break;
            }
            else if (currentHFR < HFR)
            {
                double slope=0;

                // Let's try to calculate slope of the V curve.
                if (initSlopeHFR == 0 && HFRInc == 0 && HFRDec >= 1)
                {
                    initSlopeHFR = HFR;
                    initSlopePos = lastHFRPos;

                    #ifdef FOCUS_DEBUG
                    qDebug() << "Setting initial slop to " << initSlopePos << " @ HFR " << initSlopeHFR << endl;
                    #endif

                }

                // Let's now limit the travel distance of the focuser
                if (lastFocusDirection == FOCUS_OUT && lastHFRPos < focusInLimit && fabs(currentHFR - HFR) > 0.1)
                {
                    focusInLimit = lastHFRPos;
                    #ifdef FOCUS_DEBUG
                    qDebug() << "New FocusInLimit " << focusInLimit << endl;
                    #endif
                }
                else if (lastFocusDirection == FOCUS_IN && lastHFRPos > focusOutLimit && fabs(currentHFR - HFR) > 0.1)
                {
                    focusOutLimit = lastHFRPos;
                    #ifdef FOCUS_DEBUG
                    qDebug() << "New FocusOutLimit " << focusOutLimit << endl;
                    #endif
                }

                // If we have slope, get next target position
                if (initSlopeHFR && absMotionMax > 50)
                {
                    double factor=0.5;
                    slope = (currentHFR - initSlopeHFR) / (pulseStep - initSlopePos);
                    if (fabs(currentHFR-minHFR)*100.0 < 0.5)
                        factor = 1 - fabs(currentHFR-minHFR)*10;
                    targetPulse = pulseStep + (currentHFR*factor - currentHFR)/slope;
                    if (targetPulse < 0)
                    {
                        factor = 1;
                        while (targetPulse < 0)
                        {
                           factor -= 0.005;
                            targetPulse = pulseStep + (currentHFR*factor - currentHFR)/slope;
                        }
                    }
                    #ifdef FOCUS_DEBUG
                    qDebug() << "Using slope to calculate target pulse..." << endl;
                    #endif
                }
                // Otherwise proceed iteratively
                else
                {
                     if (lastFocusDirection == FOCUS_IN)
                         targetPulse = pulseStep - pulseDuration;
                     else
                         targetPulse = pulseStep + pulseDuration;

                     #ifdef FOCUS_DEBUG
                     qDebug() << "Proceeding iteratively to next target pulse ..." << endl;
                     #endif
                }

                #ifdef FOCUS_DEBUG
                qDebug() << "V-Curve Slope " << slope << " pulseStep " << pulseStep << " targetPulse " << targetPulse << endl;
                #endif

                HFR = currentHFR;

                // Let's keep track of the minimum HFR
                if (HFR < minHFR)
                {
                    minHFR = HFR;
                    minHFRPos = pulseStep;
                    #ifdef FOCUS_DEBUG
                    qDebug() << "new minHFR " << minHFR << " @ positioin " << minHFRPos << endl;
                    qDebug() << "########################################" << endl;
                    #endif
                }

                lastHFRPos = pulseStep;

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

                    HFR = currentHFR;
                    lastHFRPos = pulseStep;
                    initSlopeHFR=0;
                    HFRInc=0;

                    #ifdef FOCUS_DEBUG
                    qDebug() << "We are going away from optimal HFR " << endl;
                    #endif

                    // Let's set new limits
                    if (lastFocusDirection == FOCUS_IN)
                    {
                        focusInLimit = pulseStep;
                        #ifdef FOCUS_DEBUG
                        qDebug() << "Setting focus IN limit to " << focusInLimit << endl;
                        #endif

                    }
                    else
                    {
                        focusOutLimit = pulseStep;
                        #ifdef FOCUS_DEBUG
                        qDebug() << "Setting focus OUT limit to " << focusOutLimit << endl;
                        #endif
                    }



                    // Decrease pulse
                    pulseDuration = pulseDuration * 0.75;

                    // Let's get close to the minimum HFR position so far detected
                    if (lastFocusDirection == FOCUS_OUT)
                          targetPulse = minHFRPos-pulseDuration/2;
                     else
                          targetPulse = minHFRPos+pulseDuration/2;

                    #ifdef FOCUS_DEBUG
                    qDebug() << "new targetPulse " << targetPulse << endl;
                    #endif


                }
            }

        // Limit target Pulse to algorithm limits
        if (focusInLimit != 0 && lastFocusDirection == FOCUS_IN && targetPulse < focusInLimit)
        {
            targetPulse = focusInLimit;
            #ifdef FOCUS_DEBUG
            qDebug() << "Limiting target pulse to focus in limit " << targetPulse << endl;
            #endif
        }
        else if (focusOutLimit != 0 && lastFocusDirection == FOCUS_OUT && targetPulse > focusOutLimit)
        {
            targetPulse = focusOutLimit;
            #ifdef FOCUS_DEBUG
            qDebug() << "Limiting target pulse to focus out limit " << targetPulse << endl;
            #endif
        }

        // Limit target pulse to focuser limits
        if (targetPulse < absMotionMin)
            targetPulse = absMotionMin;
        else if (targetPulse > absMotionMax)
            targetPulse = absMotionMax;

        // Ops, we can't go any further, we're done.
        if (targetPulse == pulseStep)
        {
            appendLogText(i18n("Autofocus complete."));
            stopFocus();
            emit suspendGuiding(false);
            emit autoFocusFinished(true);
            return;
        }

        // Ops, deadlock
        if (focusOutLimit && focusOutLimit == focusInLimit)
        {
            appendLogText(i18n("Deadlock reached. Please try again with different settings."));
            stopFocus();
            emit autoFocusFinished(false);
            return;
        }

        if (fabs(targetPulse - initHFRPos) > maxTravel->value())
        {
            #ifdef FOCUS_DEBUG
            qDebug() << "targetPulse (" << targetPulse << ") - initHFRPos (" << initHFRPos << ") exceeds maxTravel distance of " <<
            maxTravel->value() << endl;
            #endif

            appendLogText("Maximum travel limit reached. Autofocus aborted.");
            stopFocus();
            emit autoFocusFinished(false);
            #ifdef FOCUS_DEBUG
            qDebug() << "Maximum travel limit reached. Autofocus aborted." << endl;
            #endif
            break;

        }

        // Get delta for next move
        delta = (targetPulse - pulseStep);

        #ifdef FOCUS_DEBUG
        qDebug() << "delta (targetPulse - pulseStep) " << delta << endl;
        qDebug() << "Focusing " << ((delta < 0) ? "IN"  : "OUT") << endl;
        qDebug() << "########################################" << endl;
        #endif

        // Now cross your fingers and wait
       if (delta > 0)
            FocusOut(delta);
        else
          FocusIn(fabs(delta));
        break;

    }

}


void Focus::autoFocusRel(double currentHFR)
{
    static int noStarCount=0;
    QString deltaTxt = QString("%1").arg(fabs(currentHFR-HFR)*100.0, 0,'g', 2);
    QString HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    appendLogText(i18n("FITS received. HFR %1. Delta (%2%)", HFRText, deltaTxt));

    if (pulseDuration <= 32)
    {
        appendLogText(i18n("Autofocus failed to reach proper focus. Try adjusting the tolerance value."));
        stopFocus();
        emit autoFocusFinished(false);
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        if (noStarCount++ < 3)
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
            HFR = currentHFR;
            FocusIn(pulseDuration);
            break;

        case FOCUS_IN:
            if (fabs(currentHFR - HFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
            {
                appendLogText(i18n("Autofocus complete."));                
                stopFocus();
                emit suspendGuiding(false);
                emit autoFocusFinished(true);
                break;
            }
            else if (currentHFR < HFR)
            {
                HFR = currentHFR;
                FocusIn(pulseDuration);
                HFRInc=0;
            }
            else
            {
                HFRInc++;


                if (HFRInc < 1)
                {
                    capture();
                    return;
                }
                else
                {

                    HFR = currentHFR;

                    HFRInc=0;

                    if (reverseDir)
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
                }
            }

            break;

    case FOCUS_OUT:
        if (fabs(currentHFR - HFR) < (toleranceIN->value()/100.0) && HFRInc == 0)
        {
            appendLogText(i18n("Autofocus complete."));
            emit suspendGuiding(false);
            emit autoFocusFinished(true);
            stopFocus();
            break;
        }
        else if (currentHFR < HFR)
        {
            HFR = currentHFR;
            FocusOut(pulseDuration);
            HFRInc=0;
        }
        else
        {
            HFRInc++;

            if (HFRInc < 1)
                capture();
            else
            {

                HFR = currentHFR;

                HFRInc=0;

                if (reverseDir)
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
                    FocusIn(pulseDuration);
            }
        }

        break;

    }

}

void Focus::processFocusProperties(INumberVectorProperty *nvp)
{

    if (!strcmp(nvp->name, "ABS_FOCUS_POSITION"))
    {
       INumber *pos = IUFindNumber(nvp, "FOCUS_ABSOLUTE_POSITION");
       if (pos)
           pulseStep = pos->value;

       if (canAbsMove && inAutoFocus)
       {
           if (nvp->s == IPS_OK && captureInProgress == false)
               capture();
           else if (nvp->s == IPS_ALERT)
           {
               appendLogText(i18n("Focuser error, check INDI panel."));
               emit autoFocusFinished(false);
               stopFocus();
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
                appendLogText(i18n("Focuser error, check INDI panel."));
                stopFocus();
                emit autoFocusFinished(false);
            }

        }

        return;
    }

}

void Focus::appendLogText(const QString &text)
{

    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    emit newLog();
}

void Focus::clearLog()
{
    logText.clear();
    emit newLog();
}

void Focus::startLooping()
{
    inFocusLoop = true;

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
        focusOutB->setEnabled(false);
        focusInB->setEnabled(false);

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

        captureB->setEnabled(true);
        startLoopB->setEnabled(true);

    }
    else
    {
        focusOutB->setEnabled(false);
        focusInB->setEnabled(false);
    }
}

void Focus::focusStarSelected(int x, int y)
{
    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    int offset = kcfg_focusBoxSize->value();
    int binx, biny;

    targetChip->getBinning(&binx, &biny);

    x = (x - offset) * binx;
    y = (y - offset) * biny;
    int w=offset*2*binx;
    int h=offset*2*biny;

    if (x<0)
        x=0;
    if (y<0)
        y=0;
    if ((x+w)>fw)
        w=fw-x;
    if ((y+h)>fh)
        h=fh-y;

    if (w <= 0)
        w = DEFAULT_SUBFRAME_DIM;
    if (h <= 0)
        h = DEFAULT_SUBFRAME_DIM;

    FITSView *targetImage = targetChip->getImage(FITS_FOCUS);
    targetImage->updateMode(FITS_FOCUS);

    if (targetChip->canSubframe())
    {
        targetChip->setFocusFrame(x, y, w, h);
        fx = x;
        fy = y;
        fw = w;
        fh = h;
    }

    starSelected=true;

    capture();
}

void Focus::checkFocus(double delta)
{
    deltaHFR = delta;

    capture();
}

void Focus::subframeUpdated(bool enable)
{
    if (enable == false)
    {
        fx=fy=fw=fh=0;
        ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
        targetChip->setFocusFrame(0,0,0,0);
        targetChip->resetFrame();
        starSelected = false;
    }
}

void Focus::setInSequenceFocus(bool autoFocusComplete)
{
    inSequenceFocus = !autoFocusComplete;
}

}

#include "focus.moc"
