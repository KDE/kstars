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

    pulseDuration = 1000;

    filterType = FITS_NONE;

    connect(startFocusB, SIGNAL(clicked()), this, SLOT(startFocus()));
    connect(stopFocusB, SIGNAL(clicked()), this, SLOT(stopFocus()));

    connect(focusOutB, SIGNAL(clicked()), this, SLOT(FocusOut()));
    connect(focusInB, SIGNAL(clicked()), this, SLOT(FocusIn()));

    connect(captureB, SIGNAL(clicked()), this, SLOT(capture()));

    connect(AutoModeR, SIGNAL(toggled(bool)), this, SLOT(toggleAutofocus(bool)));

    connect( filterCombo, SIGNAL(activated(int)), this, SLOT(updateImageFilter(int)));

    lastFocusDirection = FOCUS_NONE;

    startFocusB->setEnabled(false);
    stopFocusB->setEnabled(false);

    focusProgress->setText(i18n("Idle."));

}

void Focus::toggleAutofocus(bool enable)
{
    if (enable)
    {
        startFocusB->setEnabled(true);
        focusInB->setEnabled(false);
        focusOutB->setEnabled(false);
        captureB->setEnabled(false);
    }
    else
    {
        startFocusB->setEnabled(false);
        stopFocusB->setEnabled(false);
        focusInB->setEnabled(true);
        focusOutB->setEnabled(true);
        captureB->setEnabled(true);
    }
}

void Focus::addFocuser(ISD::GDInterface *newFocuser)
{
    currentFocuser = static_cast<ISD::Focuser *> (newFocuser);

    if (currentFocuser->canAbsMove())
    {
        canAbsMove = true;
        getAbsFocusPosition();
    }


    connect(currentFocuser, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processFocusProperties(INumberVectorProperty*)));

}

void Focus::addCCD(ISD::GDInterface *newCCD)
{
    if (currentCCD == NULL)
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
        pulseDuration = (absMotionMax - absMotionMin) * 0.05;
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

    focusProgress->setText(i18n("Autofocus in progress..."));
}

void Focus::stopFocus()
{

    startFocusB->setEnabled(true);
    stopFocusB->setEnabled(false);

    if (manualModeR->isChecked())
    {
        captureB->setEnabled(true);
        focusOutB->setEnabled(true);
        focusInB->setEnabled(true);
    }

    absIterations = 0;

}

void Focus::capture()
{

    if (currentCCD == NULL)
        return;

    double seqExpose = 1.0;
    CCDFrameType ccdFrame = FRAME_LIGHT;
    CCDBinType   binType  = QUADRAPLE_BIN;

    if (currentCCD->isConnected() == false)
    {
        focusProgress->setText(i18n("Error: Lost connection to CCD."));
        return;
    }


    currentCCD->setCaptureMode(FITS_FOCUS);
    currentCCD->setCaptureFilter(filterType);

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

    focusProgress->setText(i18n("Capturing image..."));

}

void Focus::FocusIn(int ms)
{

    if (currentFocuser == NULL)
        return;

    if (currentFocuser->isConnected() == false)
    {
        focusProgress->setText(i18n("Error: Lost connection to Focuser."));
        return;
    }

    if (ms == -1)
        ms = stepIN->value();

    lastFocusDirection = FOCUS_IN;

    currentFocuser->focusIn();

    if (canAbsMove)
    {
        //getAbsFocusPosition();
        currentFocuser->absMoveFocuser(pulseStep+ms);
    }
    else
        currentFocuser->moveFocuser(ms);

    focusProgress->setText(i18n("Focusing inward..."));

}

void Focus::FocusOut(int ms)
{

    if (currentFocuser == NULL)
        return;

    if (currentFocuser->isConnected() == false)
    {
        focusProgress->setText(i18n("Error: Lost connection to Focuser."));
        return;
    }

    lastFocusDirection = FOCUS_OUT;

    if (ms == -1)
        ms = stepIN->value();

    currentFocuser->focusOut();

    if (canAbsMove)
    {
        //getAbsFocusPosition();
        currentFocuser->absMoveFocuser(pulseStep-ms);
    }
    else
        currentFocuser->moveFocuser(ms);

    focusProgress->setText(i18n("Focusing outward..."));

}

void Focus::newFITS(IBLOB *bp)
{
    INDI_UNUSED(bp);

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

    focusProgress->setText(i18n("FITS received. Analyzing..."));

    if (manualModeR->isChecked())
    {
        //HFR = currentHFR;
        HFROut->setText(QString("%1").arg(currentHFR, 0,'g', 3));
        return;
    }


    //qDebug() << "Pulse Duration: " << pulseDuration << endl;
    //qDebug() << "Current HFR" << currentHFR << " last HFR " << HFR << " diff is " << fabs(currentHFR - HFR) << " tolernace is " << toleranceIN->value() << endl;

    if ( (!canAbsMove && pulseDuration <= 32) || (canAbsMove && ++absIterations > MAXIMUM_ABS_ITERATIONS))
    {
        focusProgress->setText(i18n("Autofocus failed to reach proper focus."));
        stopFocus();
        return;
    }

    switch (lastFocusDirection)
    {
        case FOCUS_NONE:
            HFR = currentHFR;
            FocusIn(pulseDuration);

            //qDebug() << "In Focus NONE and will focus in now " << endl;
            break;

        case FOCUS_IN:
           //qDebug() << "Last Operation: FOCUS IN " << endl;

            if (fabs(currentHFR - HFR) < toleranceIN->value())
            {
                //qDebug() << "currentHFR is HFR, quitting  HFR " << HFR << " currentHFR " << currentHFR << endl;
                focusProgress->setText(i18n("Autofocus complete."));
                stopFocus();
                break;
            }
            else if (currentHFR < HFR)
            {
                //qDebug() << "Will continue to focus in ..." << endl;
                HFR = currentHFR;
                FocusIn(pulseDuration);
            }
            else
            {
                //qDebug() << "Will change direction to FOCUS OUT" << endl;
                HFR = currentHFR;
                pulseDuration /= 2;
                FocusOut(pulseDuration);
            }

            break;

    case FOCUS_OUT:
          //qDebug() << "Last Operation: FOCUS OUT " << endl;

        if (fabs(currentHFR - HFR) < toleranceIN->value())
        {
            //qDebug() << "currentHFR is HFR, quitting  HFR " << HFR << " currentHFR " << currentHFR << endl;
            focusProgress->setText(i18n("Autofocus complete."));
            stopFocus();
            break;
        }
        else if (currentHFR < HFR)
        {
            //qDebug() << "Will continue to focus out ..." << endl;
            HFR = currentHFR;
            FocusOut(pulseDuration);
        }
        else
        {
            //qDebug() << "Will change direction to FOCUS IN" << endl;
            HFR = currentHFR;

            pulseDuration /= 2;
            FocusIn(pulseDuration);
        }

        if (canAbsMove == false)
         capture();
        break;

    }

    //IDLog("We are reading current HFR: %g", currentHFR);

    HFROut->setText(QString::number( currentHFR, 'f', 2 ));
}

void Focus::processFocusProperties(INumberVectorProperty *nvp)
{

    if (!strcmp(nvp->name, "ABS_FOCUS_POSITION"))
    {
       INumber *pos = IUFindNumber(nvp, "FOCUS_ABSOLUTE_POSITION");
       if (pos)
           pulseStep = pos->value;

       if (canAbsMove && (AutoModeR->isChecked() == true) && (startFocusB->isEnabled() == false))
       {
           if (nvp->s == IPS_OK)
               capture();
           else if (nvp->s == IPS_ALERT)
           {
               focusProgress->setText(i18n("Focuser error, check INDI panel."));
               stopFocus();
           }

       }

       return;
    }

    if (!strcmp(nvp->name, "FOCUS_TIMER"))
    {

        if (canAbsMove == false && (AutoModeR->isChecked() == true) && (startFocusB->isEnabled() == false))
        {
            if (nvp->s == IPS_OK)
                capture();
            else if (nvp->s == IPS_ALERT)
            {
                focusProgress->setText(i18n("Focuser error, check INDI panel."));
                stopFocus();
            }
        }

        return;
    }

}

void Focus::updateImageFilter(int index)
{
    if (index == 0)
        filterType = FITS_NONE;
    else if (index == 1)
        filterType = FITS_LOW_PASS;
    else if (index == 2)
        filterType = FITS_EQUALIZE;

}

}

#include "focus.moc"
