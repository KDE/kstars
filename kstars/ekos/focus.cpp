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

namespace Ekos
{

Focus::Focus()
{
    setupUi(this);

    currentFocuser = NULL;
    currentCCD     = NULL;

    connect(startFocusB, SIGNAL(clicked()), this, SLOT(startFocus()));
    connect(stopFocusB, SIGNAL(clicked()), this, SLOT(stopFocus()));

    connect(focusOutB, SIGNAL(clicked()), this, SLOT(FocusOut()));
    connect(focusInB, SIGNAL(clicked()), this, SLOT(FocusIn()));

    connect(captureB, SIGNAL(clicked()), this, SLOT(capture()));

    connect(AutoModeR, SIGNAL(toggled(bool)), this, SLOT(toggleAutofocus(bool)));

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
}

void Focus::addCCD(ISD::GDInterface *newCCD)
{
    if (currentCCD == NULL)
        currentCCD = (ISD::CCD *) newCCD;



}

void Focus::startFocus()
{
    lastFocusDirection = FOCUS_NONE;

    HFR = 0;
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

    currentCCD->setFocusMode(false);
}

void Focus::capture()
{

    if (currentCCD == NULL)
        return;

    double seqExpose = 1.0;
    CCDFrameType ccdFrame = FRAME_LIGHT;
    CCDBinType   binType  = QUADRAPLE_BIN;


    // TODO: Set bining to 3x3
    // TODO: Make sure FRAME is LIGHT

   // qDebug() << "Issuing CCD capture command" << endl;

    if (currentCCD->isConnected() == false)
    {
        focusProgress->setText(i18n("Error: Lost connection to CCD."));
        return;
    }

    currentCCD->setFocusMode(true);

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    currentCCD->runCommand(INDI_CCD_FRAME, &ccdFrame);

    if (currentCCD->runCommand(INDI_CCD_BINNING, &binType) == false)
    {
        binType = TRIPLE_BIN;
        if (currentCCD->runCommand(INDI_CCD_BINNING, &binType) == false)
        {
            binType = DOUBLE_BIN;
            currentCCD->runCommand(INDI_CCD_BINNING, &binType);
        }
    }

    currentCCD->runCommand(INDI_CAPTURE, &seqExpose);

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

    lastFocusDirection = FOCUS_IN;

    currentFocuser->runCommand(INDI_FOCUS_IN);

    currentFocuser->runCommand(INDI_FOCUS_MOVE, &ms);

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

    currentFocuser->runCommand(INDI_FOCUS_OUT);

    currentFocuser->runCommand(INDI_FOCUS_MOVE, &ms);

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

    //qDebug() << "Iteration #" << counter ++ << endl;
    //qDebug() << "Pulse Duration: " << pulseDuration << endl;
    //qDebug() << "Current HFR" << currentHFR << " last HFR " << HFR << " diff is " << fabs(currentHFR - HFR) << " tolernace is " << toleranceIN->value() << endl;

    if (pulseDuration <= 32)
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
            sleep(delayIN->value());
            capture();
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

            sleep(delayIN->value());
            capture();

            break;

    case FOCUS_OUT:
          //qDebug() << "Last Operation: FOCUS OUT " << endl;

        if (fabs(currentHFR - HFR) < toleranceIN->value())
        {
            //qDebug() << "currentHFR is HFR, quitting " << endl;
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

         sleep(delayIN->value());
         capture();
        break;

    }

    //qDebug() << "########################################" << endl;
    //IDLog("We are reading current HFR: %g", currentHFR);

    HFROut->setText(QString("%1").arg(currentHFR, 0,'g', 3));
}

}

#include "focus.moc"
