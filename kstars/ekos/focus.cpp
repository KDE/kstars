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

#include "indi/driverinfo.h"
#include "indi/indicommon.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsimage.h"

#include <libindi/basedevice.h>

#define MAXIMUM_ABS_ITERATIONS  30

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

    lastFocusDirection = FOCUS_NONE;

    focusType = FOCUS_MANUAL;

    startFocusB->setEnabled(false);
    stopFocusB->setEnabled(false);

    appendLogText("Idle.");

    foreach(QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    exposureIN->setValue(Options::focusExposure());
    toleranceIN->setValue(Options::focusTolerance());
    stepIN->setValue(Options::focusTicks());


}

void Focus::toggleAutofocus(bool enable)
{
    if (enable)
    {
        focusType = FOCUS_AUTO;
        startFocusB->setEnabled(true);
        focusInB->setEnabled(false);
        focusOutB->setEnabled(false);
        captureB->setEnabled(false);
        startLoopB->setEnabled(false);
    }
    else
    {
        focusType = FOCUS_MANUAL;
        startFocusB->setEnabled(false);
        stopFocusB->setEnabled(false);
        focusInB->setEnabled(true);
        focusOutB->setEnabled(true);
        captureB->setEnabled(true);
        startLoopB->setEnabled(true);

    }

    if (inFocusLoop || inAutoFocus)
        stopFocus();
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

}

void Focus::setCCD(ISD::GDInterface *newCCD)
{
    currentCCD = (ISD::CCD *) newCCD;
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

    stopFocusB->setEnabled(true);

    startFocusB->setEnabled(false);
    captureB->setEnabled(false);
    focusOutB->setEnabled(false);
    focusInB->setEnabled(false);

    inAutoFocus = true;


    reverseDir = false;

    Options::setFocusTicks(stepIN->value());
    Options::setFocusTolerance(toleranceIN->value());
    Options::setFocusExposure(exposureIN->value());

    appendLogText("Autofocus in progress...");
}

void Focus::stopFocus()
{

    if (focusType == FOCUS_AUTO)
        startFocusB->setEnabled(true);

    stopFocusB->setEnabled(false);


    if (focusType == FOCUS_MANUAL)
    {
        captureB->setEnabled(true);
        focusOutB->setEnabled(true);
        focusInB->setEnabled(true);
        startLoopB->setEnabled(true);
    }

    inAutoFocus = false;
    inFocusLoop = false;

    absIterations = 0;
    HFRInc=0;
    reverseDir = false;

}

void Focus::capture()
{

    if (currentCCD == NULL)
        return;

    double seqExpose = exposureIN->value();
    CCDFrameType ccdFrame = FRAME_LIGHT;
    CCDBinType   binType  = QUADRAPLE_BIN;

    if (currentCCD->isConnected() == false)
    {
        appendLogText("Error: Lost connection to CCD.");
        return;
    }


    currentCCD->setCaptureMode(FITS_FOCUS);
    currentCCD->setCaptureFilter( (FITSScale) filterCombo->currentIndex());

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    currentCCD->setFrameType(ccdFrame);

    if (currentCCD->setBinning(binType) == false)
    {
        binType = TRIPLE_BIN;
        if (currentCCD->setBinning(binType) == false)
        {
            binType = DOUBLE_BIN;
            currentCCD->setBinning(binType);
        }
    }

    currentCCD->capture(seqExpose);

    if (inFocusLoop == false)
        appendLogText("Capturing image...");

}

void Focus::FocusIn(int ms)
{

    if (currentFocuser == NULL)
        return;

    if (currentFocuser->isConnected() == false)
    {
        appendLogText("Error: Lost connection to Focuser.");
        return;
    }

    if (ms == -1)
        ms = stepIN->value();

    lastFocusDirection = FOCUS_IN;

    currentFocuser->focusIn();

    if (canAbsMove)
    {
        currentFocuser->absMoveFocuser(pulseStep+ms);
        //qDebug() << "Focusing in to position " << pulseStep+ms << endl;
    }
    else
        currentFocuser->moveFocuser(ms);

    appendLogText("Focusing inward...");

}

void Focus::FocusOut(int ms)
{

    if (currentFocuser == NULL)
        return;

    if (currentFocuser->isConnected() == false)
    {
        appendLogText("Error: Lost connection to Focuser.");
        return;
    }

    lastFocusDirection = FOCUS_OUT;

    if (ms == -1)
        ms = stepIN->value();

    currentFocuser->focusOut();

    if (canAbsMove)
    {
        currentFocuser->absMoveFocuser(pulseStep-ms);
        //qDebug() << "Focusing out to position " << pulseStep-ms << endl;
    }
    else
        currentFocuser->moveFocuser(ms);

    appendLogText("Focusing outward...");

}

void Focus::newFITS(IBLOB *bp)
{

    INDI_UNUSED(bp);
    QString HFRText;

    FITSViewer *fv = currentCCD->getViewer();

    currentCCD->disconnect(this);

    double currentHFR=0;

    foreach(FITSTab *tab, fv->getImages())
    {
        if (tab->getUID() == currentCCD->getFocusTabID())
        {
            currentHFR = tab->getImage()->getHFR();
            //qDebug() << "Focus HFR is " << tab->getImage()->getHFR() << endl;
            break;
        }
    }

    HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    if (inFocusLoop == false && focusType == FOCUS_MANUAL && HFR == -1)
            appendLogText(QString("FITS received. No stars detected."));

    HFROut->setText(HFRText);

    if (inFocusLoop)
        capture();

    if (focusType == FOCUS_MANUAL)
        return;

    if (canAbsMove)
        autoFocusAbs(currentHFR);
    else
        autoFocusRel(currentHFR);

}


void Focus::autoFocusAbs(double currentHFR)
{
    static int initHFRPos=0, lastHFRPos=0, minHFRPos=0, initSlopePos=0, initPulseDuration=0, focusOutLimit=0, focusInLimit=0;
    static double initHFR=0, minHFR=0, initSlopeHFR=0;
    double targetPulse=0, delta=0;

    QString deltaTxt = QString("%1").arg(fabs(currentHFR-minHFR)*100.0, 0,'g', 2);
    QString HFRText = QString("%1").arg(currentHFR, 0,'g', 3);

    if (minHFR)
         appendLogText(QString("FITS received. HFR %1 @ %2. Delta (%3%)").arg(HFRText).arg(pulseStep).arg(deltaTxt));
    else
        appendLogText(QString("FITS received. HFR %1 @ %2.").arg(HFRText).arg(pulseStep));

    if (++absIterations > MAXIMUM_ABS_ITERATIONS)
    {
        appendLogText("Autofocus failed to reach proper focus. Try increasing tolerance value.");
        stopFocus();
        return;
    }

    // No stars detected, try to capture again
    if (currentHFR == -1)
    {
        appendLogText("No stars detected, capturing again...");
        capture();
        return;
    }

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
        if (focusInLimit && focusOutLimit && fabs(currentHFR - minHFR) < (toleranceIN->value()/100.0) && HFRInc == 0 )
            {
                if (absIterations <= 2)
                    appendLogText("Change in HFR is too small. Try increasing the step size or decreasing the tolerance.");
                else
                    appendLogText("Autofocus complete.");
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
                }

                // Let's now limit the travel distance of the focuser
                if (lastFocusDirection == FOCUS_OUT && lastHFRPos < focusInLimit)
                    focusInLimit = lastHFRPos;
                else if (lastFocusDirection == FOCUS_IN && lastHFRPos > focusOutLimit)
                    focusOutLimit = lastHFRPos;

                // If we have slope, get next target positon
                if (initSlopeHFR)
                {
                    slope = (currentHFR - initSlopeHFR) / (pulseStep - initSlopePos);
                    targetPulse = pulseStep + (currentHFR*0.5 - currentHFR)/slope;
                }
                // Otherwise proceed iteratively
                else
                {
                     if (lastFocusDirection == FOCUS_IN)
                         targetPulse = pulseStep + pulseDuration;
                     else
                         targetPulse = pulseStep - pulseDuration;

                }

                HFR = currentHFR;

                // Let's keep track of the minumum HFR
                if (HFR < minHFR)
                {
                    minHFR = HFR;
                    minHFRPos = pulseStep;
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

                    // Let's set new limits
                    if (lastFocusDirection == FOCUS_IN)
                        focusInLimit = pulseStep;
                    else
                        focusOutLimit = pulseStep;

                    // Decrease pulse
                    pulseDuration = pulseDuration * 0.75;

                    // Let's get close to the minimum HFR position so far detected
                    if (lastFocusDirection == FOCUS_OUT)
                          targetPulse = minHFRPos+pulseDuration/2;
                     else
                          targetPulse = minHFRPos-pulseDuration/2;

                }
            }

        // Limit target Pulse to algorithm limits
        if (focusInLimit != 0 && lastFocusDirection == FOCUS_IN && targetPulse > focusInLimit)
            targetPulse = focusInLimit;
        else if (focusOutLimit != 0 && lastFocusDirection == FOCUS_OUT && targetPulse < focusOutLimit)
            targetPulse = focusOutLimit;

        // Limit target pulse to focuser limits
        if (targetPulse < absMotionMin)
            targetPulse = absMotionMin;
        else if (targetPulse > absMotionMax)
            targetPulse = absMotionMax;

        // Ops, we can't go any further, we're done.
        if (targetPulse == pulseStep)
        {
            appendLogText("Autofocus complete.");
            stopFocus();
            return;
        }

        // Ops, deadlock
        if (focusOutLimit && focusOutLimit == focusInLimit)
        {
            appendLogText("Deadlock reached. Please try again with different settings.");
            stopFocus();
            return;
        }

        // Get delta for next move
        delta = (targetPulse - pulseStep);

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

    appendLogText(QString("FITS received. HFR %1. Delta (%2%)").arg(HFRText).arg(deltaTxt));

    if (pulseDuration <= 32)
    {
        appendLogText("Autofocus failed to reach proper focus. Try adjusting the tolerance value.");
        stopFocus();
        return;
    }

    if (currentHFR == -1)
    {
        appendLogText("No stars detected, capturing again...");
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
                appendLogText("Autofocus complete.");
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
            appendLogText("Autofocus complete.");
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
               appendLogText("Focuser error, check INDI panel.");
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
                appendLogText("Focuser error, check INDI panel.");
                stopFocus();
            }
        }

        return;
    }

}

void Focus::appendLogText(const QString &text)
{

    logText.insert(0, QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss") + " " + i18n("%1").arg(text));

    emit newLog();
}

void Focus::clearLog()
{
    logText.clear();
    emit newLog();
}

void Focus::startLooping()
{
    startFocusB->setEnabled(false);
    startLoopB->setEnabled(false);
    stopFocusB->setEnabled(true);

    captureB->setEnabled(false);
    focusOutB->setEnabled(false);
    focusInB->setEnabled(false);

    inFocusLoop = true;

    appendLogText("Starting continious exposure...");

    capture();
}


}

#include "focus.moc"
