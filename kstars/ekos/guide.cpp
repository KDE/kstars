/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QDateTime>

#include "guide/gmath.h"
#include "guide/guider.h"
#include "guide.h"
#include "Options.h"


#include <KMessageBox>
#include <KLed>

#include "indi/driverinfo.h"
#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitsimage.h"

#include "guide/rcalibration.h"
#include "guide/gmath.h"

#include <libindi/basedevice.h>

namespace Ekos
{

Guide::Guide() : QWidget()
{
    setupUi(this);

    currentCCD = NULL;
    currentTelescope = NULL;
    ccd_hor_pixel =  ccd_ver_pixel =  focal_length =  aperture = -1;

    tabWidget = new QTabWidget(this);

    filterType = FITS_NONE;

    telescopeGuide = true;

    tabLayout->addWidget(tabWidget);

    guiderStage = CALIBRATION_STAGE;

    pmath = new cgmath();

    calibration = new rcalibration(this);
    calibration->set_math(pmath);

    guider = new rguider(this);
    guider->set_math(pmath);

    tabWidget->addTab(calibration, i18n("%1").arg(calibration->windowTitle()));
    tabWidget->addTab(guider, i18n("%1").arg(guider->windowTitle()));
    tabWidget->setTabEnabled(1, false);

    connect(ST4Combo, SIGNAL(currentIndexChanged(int)), this, SLOT(newST4(int)));

    connect( filterCombo, SIGNAL(activated(int)), this, SLOT(updateImageFilter(int)));

}

Guide::~Guide()
{
    delete guider;
    delete calibration;
    delete pmath;
}

void Guide::addCCD(ISD::GDInterface *newCCD)
{
    currentCCD = (ISD::CCD *) newCCD;

    guiderCombo->addItem(currentCCD->getDeviceName());

    INumberVectorProperty * nvp = currentCCD->getBaseDevice()->getNumber("GUIDE_INFO");

    if (nvp == NULL)
       nvp = currentCCD->getBaseDevice()->getNumber("CCD_INFO");

    if (nvp)
    {
        INumber *np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_X");
        if (np)
            ccd_hor_pixel = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np)
            ccd_ver_pixel = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np)
            ccd_ver_pixel = np->value;
    }

    if (ccd_hor_pixel != -1 && ccd_ver_pixel != -1 && focal_length != -1 && aperture != -1)
    {
        pmath->set_guider_params(ccd_hor_pixel, ccd_ver_pixel, aperture, focal_length);
        int x,y,w,h;

        if (currentCCD->getFrame(&x,&y,&w,&h))
            pmath->set_video_params(w, h);

        guider->fill_interface();
    }

    if (currentCCD->canGuide())
    {
        telescopeGuide = false;
        ST4Combo->addItem(currentCCD->getDeviceName());
    }
}

void Guide::addTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = (ISD::Telescope*) newTelescope;

    if (currentTelescope->canGuide())
    {
       ST4Combo->addItem(currentTelescope->getDeviceName());
       ST4Combo->setCurrentIndex(ST4Combo->count()-1);
       telescopeGuide = true;
    }

    if (ST4Combo->count() == 0)
    {
        KMessageBox::error(NULL, i18n("The Telescope and Guider do not support pulse commands. Guiding is not possible."));
        close();
        return;
    }

    INumberVectorProperty * nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");
    if (nvp)
    {
        INumber *np = IUFindNumber(nvp, "TELESCOPE_APERTURE");
        if (np)
            aperture = np->value;

        np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
        if (np)
            focal_length = np->value;
    }

    if (ccd_hor_pixel != -1 && ccd_ver_pixel != -1 && focal_length != -1 && aperture != -1)
    {
        pmath->set_guider_params(ccd_hor_pixel, ccd_ver_pixel, aperture, focal_length);
        int x,y,w,h;

        if (currentCCD->getFrame(&x,&y,&w,&h))
            pmath->set_video_params(w, h);

        guider->fill_interface();
    }

    //qDebug() << "ccd_pix_w " << ccd_hor_pixel << " - ccd_pix_h " << ccd_ver_pixel << " - focal length " << focal_length << " aperture " << aperture << endl;
}

bool Guide::capture()
{

    if (currentCCD == NULL)
        return false;

    double seqExpose = exposureSpin->value();

    CCDFrameType ccdFrame = FRAME_LIGHT;
    CCDBinType   binType  = DOUBLE_BIN;


    if (currentCCD->isConnected() == false)
    {
        appendLogText("Error: Lost connection to CCD.");
        return false;
    }

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    currentCCD->setCaptureMode(FITS_GUIDE);
    currentCCD->setCaptureFilter(filterType);

    currentCCD->setFrameType(ccdFrame);

    currentCCD->setBinning(binType);

    currentCCD->capture(seqExpose);

    return true;

}
void Guide::newFITS(IBLOB *bp)
{
    INDI_UNUSED(bp);

    FITSViewer *fv = currentCCD->getViewer();

    currentCCD->disconnect(this);

    FITSImage *fitsImage = fv->getImage(currentCCD->getGuideTabID());

    pmath->set_image(fitsImage);

    calibration->set_image(fitsImage);

    fv->show();


    if (guider->is_guiding())
    {
        guider->guide();

         if (guider->is_guiding())
            capture();
    }
    else if (calibration->is_calibrating())
    {
        pmath->do_processing();
        calibration->process_calibration();

         if (calibration->is_calibrating())
            capture();

         if (calibration->is_finished())
             tabWidget->setTabEnabled(1, true);
    }

}


void Guide::appendLogText(const QString &text)
{
    if (enableLoggingCheck->isChecked())
    {
        guideLogOut->ensureCursorVisible();
        guideLogOut->insertPlainText(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss") + " " + i18n("%1").arg(text) + "\n");
        QTextCursor c = guideLogOut->textCursor();
        c.movePosition(QTextCursor::Start);
        guideLogOut->setTextCursor(c);
    }
}

bool Guide::do_pulse( GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs )
{
    if (telescopeGuide)
    {
        if (currentTelescope == NULL)
        return false;

       return currentTelescope->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
    }
    else
    {
        if (currentCCD == NULL)
        return false;

       return currentCCD->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);

    }

}

bool Guide::do_pulse( GuideDirection dir, int msecs )
{
    if (telescopeGuide)
    {
        if (currentTelescope == NULL)
        return false;

        return currentTelescope->doPulse(dir, msecs);
    }
    else
    {
        if (currentCCD == NULL)
        return false;

        return currentCCD->doPulse(dir, msecs);

    }

}

void Guide::newST4(int index)
{
    if (currentTelescope)
        if (ST4Combo->itemText(index) == currentTelescope->getDeviceName())
        {
            telescopeGuide = true;
            return;
        }

    if (currentCCD)
        if (ST4Combo->itemText(index) == currentCCD->getDeviceName())
            telescopeGuide = false;
}

void Guide::updateImageFilter(int index)
{
    if (index == 0)
        filterType = FITS_NONE;
    else if (index == 1)
        filterType = FITS_LOW_PASS;
    else if (index == 2)
        filterType = FITS_EQUALIZE;

}


}

#include "guide.moc"
