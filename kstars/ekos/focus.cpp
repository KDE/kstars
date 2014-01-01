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

    canAbsMove     = false;
    inAutoFocus    = false;
    inFocusLoop    = false;

    HFRInc =0;
    reverseDir = false;

    pulseDuration = 1000;

    connect(startFocusB, SIGNAL(clicked()), this, SLOT(startFocus()));
    connect(stopFocusB, SIGNAL(clicked()), this, SLOT(stopFocus()));

    connect(focusOutB, SIGNAL(clicked()), this, SLOT(FocusOut()));
    connect(focusInB, SIGNAL(clicked()), this, SLOT(FocusIn()));

    connect(captureB, SIGNAL(clicked()), this, SLOT(capture()));

    connect(AutoModeR, SIGNAL(toggled(bool)), this, SLOT(toggleAutofocus(bool)));

    connect(startLoopB, SIGNAL(clicked()), this, SLOT(startLooping()));

    connect(CCDCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkCCD(int)));

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

void Focus::checkCCD(int ccdNum)
{
    if (ccdNum <= CCDs.count())
        currentCCD = CCDs.at(ccdNum);

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);
    if (targetChip)
    {
        kcfg_focusXBin->setEnabled(targetChip->canBin());
        kcfg_focusYBin->setEnabled(targetChip->canBin());
        kcfg_subFrame->setEnabled(targetChip->canSubframe());
    }

}

void Focus::setFocuser(ISD::GDInterface *newFocuser)
{
    currentFocuser = static_cast<ISD::Focuser *> (newFocuser);

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
        CCDCaptureCombo->setCurrentIndex(0);

}

void Focus::getAbsFocusPosition()
{
    if (canAbsMove)
    {
        INumberVectorProperty *absMove = currentFocuser->getBaseDevice()->getNumber("ABS_FOCUS_POSITION");
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
        //pulseDuration = (absMotionMax - absMotionMin) * 0.05;
        pulseDuration = stepIN->value();
    }
    else
      /* Start 1000 ms */
      pulseDuration=1000;

    capture();

    inAutoFocus = true;

    resetButtons();

    reverseDir = false;
    starSelected= false;

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

    #ifdef FOCUS_DEBUG
    qDebug() << "Starting focus with pulseDuration " << pulseDuration << endl;
    #endif

    if (kcfg_autoSelectStar->isChecked())
        appendLogText(i18n("Autofocus in progress..."));
    else
        appendLogText(i18n("Image capture in progress, please select a star to focus."));

}

void Focus::stopFocus()
{

    #ifdef FOCUS_DEBUG
    qDebug() << "Stopppig Focus" << endl;
    #endif

    ISD::CCDChip *targetChip = currentCCD->getChip(ISD::CCDChip::PRIMARY_CCD);

    inAutoFocus = false;
    inFocusLoop = false;
    starSelected= false;

    currentCCD->disconnect(this);

    targetChip->abortExposure();
    if (targetChip->canSubframe())
        targetChip->setFrame(fx, fy, fw, fh);

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

    if (targetChip->canBin())
        targetChip->setBinning(kcfg_focusXBin->value(), kcfg_focusXBin->value());
    targetChip->setCaptureMode(FITS_FOCUS);
    targetChip->setCaptureFilter( (FITSScale) filterCombo->currentIndex());

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    targetChip->setFrameType(ccdFrame);

    targetChip->capture(seqExpose);

    if (inFocusLoop == false)
        appendLogText(i18n("Capturing image..."));

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
    {
        currentFocuser->absMoveFocuser(pulseStep+ms);
        //qDebug() << "Focusing in to position " << pulseStep+ms << endl;
    }
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
    {
        currentFocuser->absMoveFocuser(pulseStep-ms);
        //qDebug() << "Focusing out to position " << pulseStep-ms << endl;
    }
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

    FITSImage *image_data = targetImage->getImageData();

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

    if (inFocusLoop)
    {
        capture();
        return;
    }

    if (focusType == FOCUS_MANUAL || inAutoFocus==false)
        return;

    if (starSelected == false)
    {
        int binx, biny;
        targetChip->getBinning(&binx, &biny);

        if (kcfg_autoSelectStar->isChecked())
        {
            Edge *maxStar = image_data->getMaxHFRStar();
            if (maxStar == NULL)
            {
                appendLogText(i18n("Failed to automatically select a star. Please select a star manually."));
                targetChip->getFrame(&fx, &fy, &fw, &fh);
                targetImage->updateMode(FITS_GUIDE);
                targetImage->setGuideBoxSize(kcfg_focusBoxSize->value());
                targetImage->setGuideSquare(fw/2, fh/2);
                connect(targetImage, SIGNAL(guideStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)));
                //stopFocus();
                return;
            }

            if (kcfg_subFrame->isEnabled() && kcfg_subFrame->isChecked())
            {
                targetChip->getFrame(&fx, &fy, &fw, &fh);
                int x=maxStar->x - maxStar->width * 2;
                int y=maxStar->y - maxStar->width * 2;
                int w=maxStar->width*4*binx;
                int h=maxStar->width*4*biny;

                if (x<0)
                    x=0;
                if (y<0)
                    y=0;
                if (w>fw)
                    w=fw;
                if (h>fh)
                    h=fh;

                if (w <= 0)
                    w = DEFAULT_SUBFRAME_DIM;
                if (h <= 0)
                    h = DEFAULT_SUBFRAME_DIM;


                targetChip->setFrame(x, y, w, h);
            }

            starSelected=true;

            capture();

            return;


        }
        else
        {
            targetChip->getFrame(&fx, &fy, &fw, &fh);
            targetImage->updateMode(FITS_GUIDE);
            targetImage->setGuideBoxSize(kcfg_focusBoxSize->value());
            targetImage->setGuideSquare(fw/2, fh/2);
            connect(targetImage, SIGNAL(guideStarSelected(int,int)), this, SLOT(focusStarSelected(int, int)));
            return;
        }
    }

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
    qDebug() << "minHFR: " << minHFR << " MinHFR Pos: " << minHFRPos << endl;
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
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        appendLogText(i18n("No stars detected, capturing again..."));
        capture();
        return;
    }

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
            FocusIn(pulseDuration);
            break;

        case FOCUS_IN:
        case FOCUS_OUT:
        if (reverseDir && focusInLimit && focusOutLimit && fabs(currentHFR - minHFR) < (toleranceIN->value()/100.0) && HFRInc == 0 )
            {
                if (absIterations <= 2)
                    appendLogText(i18n("Change in HFR is too small. Try increasing the step size or decreasing the tolerance."));
                else
                    appendLogText(i18n("Autofocus complete."));
                stopFocus();
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
                if (initSlopeHFR)
                {
                    slope = (currentHFR - initSlopeHFR) / (pulseStep - initSlopePos);
                    targetPulse = pulseStep + (currentHFR*0.5 - currentHFR)/slope;
                    #ifdef FOCUS_DEBUG
                    qDebug() << "Using slope to calculate target pulse..." << endl;
                    #endif
                }
                // Otherwise proceed iteratively
                else
                {
                     if (lastFocusDirection == FOCUS_IN)
                         targetPulse = pulseStep + pulseDuration;
                     else
                         targetPulse = pulseStep - pulseDuration;

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
                          targetPulse = minHFRPos+pulseDuration/2;
                     else
                          targetPulse = minHFRPos-pulseDuration/2;

                    #ifdef FOCUS_DEBUG
                    qDebug() << "new targetPulse " << targetPulse << endl;
                    #endif


                }
            }

        // Limit target Pulse to algorithm limits
        if (focusInLimit != 0 && lastFocusDirection == FOCUS_IN && targetPulse > focusInLimit)
        {
            targetPulse = focusInLimit;
            #ifdef FOCUS_DEBUG
            qDebug() << "Limiting target pulse to focus in limit " << targetPulse << endl;
            #endif
        }
        else if (focusOutLimit != 0 && lastFocusDirection == FOCUS_OUT && targetPulse < focusOutLimit)
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
            return;
        }

        // Ops, deadlock
        if (focusOutLimit && focusOutLimit == focusInLimit)
        {
            appendLogText(i18n("Deadlock reached. Please try again with different settings."));
            stopFocus();
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
            #ifdef FOCUS_DEBUG
            qDebug() << "Maximum travel limit reached. Autofocus aborted." << endl;
            #endif
            break;

        }

        // Get delta for next move
        delta = (targetPulse - pulseStep);

        #ifdef FOCUS_DEBUG
        qDebug() << "delta (targetPulse - pulseStep) " << delta << endl;
        qDebug() << "Focusing " << ((delta > 0) ? "IN"  : "OUT") << endl;
        qDebug() << "########################################" << endl;
        #endif

        // Now cross your fingers and wait
       if (delta > 0)
            FocusIn(delta);
        else
          FocusOut(fabs(delta));
        break;

    }

}


void Focus::autoFocusRel(double currentHFR)
{
    QString deltaTxt = QString("%1").arg(fabs(currentHFR-HFR)*100.0, 0,'g', 2);
    QString HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    appendLogText(i18n("FITS received. HFR %1. Delta (%2%)", HFRText, deltaTxt));

    if (pulseDuration <= 32)
    {
        appendLogText(i18n("Autofocus failed to reach proper focus. Try adjusting the tolerance value."));
        stopFocus();
        return;
    }

    if (currentHFR == -1)
    {
        appendLogText(i18n("No stars detected, capturing again..."));
        capture();
        return;
    }

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
           if (nvp->s == IPS_OK)
               capture();
           else if (nvp->s == IPS_ALERT)
           {
               appendLogText(i18n("Focuser error, check INDI panel."));
               stopFocus();
           }

       }

       return;
    }

    if (!strcmp(nvp->name, "FOCUS_TIMER"))
    {

        if (canAbsMove == false && inAutoFocus)
        {
            if (nvp->s == IPS_OK)
                capture();
            else if (nvp->s == IPS_ALERT)
            {
                appendLogText(i18n("Focuser error, check INDI panel."));
                stopFocus();
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

    x -= offset*2 ;
    y -= offset*2;
    int w=offset*4*binx;
    int h=offset*4*biny;
    FITSView *targetImage = targetChip->getImage(FITS_FOCUS);
    targetImage->updateMode(FITS_FOCUS);

    if (targetChip->canSubframe())
        targetChip->setFrame(x, y, w, h);

    starSelected=true;

    capture();
}

}

#include "focus.moc"
