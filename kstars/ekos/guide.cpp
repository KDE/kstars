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

#include <basedevice.h>

namespace Ekos
{

Guide::Guide() : QWidget()
{
    setupUi(this);

    currentCCD = NULL;
    currentTelescope = NULL;
    ccd_hor_pixel =  ccd_ver_pixel =  focal_length =  aperture = -1;
    useGuideHead = false;

    tabWidget = new QTabWidget(this);

    tabLayout->addWidget(tabWidget);

    guiderStage = CALIBRATION_STAGE;

    pmath = new cgmath();

    calibration = new rcalibration(this);
    calibration->set_math(pmath);

    guider = new rguider(this);
    guider->set_math(pmath);


    tabWidget->addTab(calibration, calibration->windowTitle());
    tabWidget->addTab(guider, guider->windowTitle());
    tabWidget->setTabEnabled(1, false);

    connect(ST4Combo, SIGNAL(currentIndexChanged(int)), this, SLOT(newST4(int)));

    foreach(QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

}

Guide::~Guide()
{
    delete guider;
    delete calibration;
    delete pmath;
}

void Guide::setCCD(ISD::GDInterface *newCCD)
{
    currentCCD = (ISD::CCD *) newCCD;

    guiderCombo->addItem(currentCCD->getDeviceName());
    useGuideHead = false;

    connect(currentCCD, SIGNAL(FITSViewerClosed()), this, SLOT(viewerClosed()));

    syncCCDInfo();

    //qDebug() << "SetCCD: ccd_pix_w " << ccd_hor_pixel << " - ccd_pix_h " << ccd_ver_pixel << " - focal length " << focal_length << " aperture " << aperture << endl;

}

void Guide::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = (ISD::Telescope*) newTelescope;

    syncTelescopeInfo();


}

void Guide::addGuideHead()
{
    // Let's just make sure
    if (currentCCD->hasGuideHead())
    {
        guiderCombo->clear();
        guiderCombo->addItem(currentCCD->getDeviceName() + QString(" Guider"));
        useGuideHead = true;
        syncCCDInfo();
    }

}

void Guide::syncCCDInfo()
{
    INumberVectorProperty * nvp = NULL;

    if (useGuideHead)
        nvp = currentCCD->getBaseDevice()->getNumber("GUIDE_INFO");
    else
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

        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

        if (targetChip->getFrame(&x,&y,&w,&h))
            pmath->set_video_params(w, h);

        guider->fill_interface();
    }

    //qDebug() << "SetCCD: ccd_pix_w " << ccd_hor_pixel << " - ccd_pix_h " << ccd_ver_pixel << " - focal length " << focal_length << " aperture " << aperture << endl;
}

void Guide::syncTelescopeInfo()
{
    INumberVectorProperty * nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {
        INumber *np = IUFindNumber(nvp, "GUIDER_APERTURE");

        if (np && np->value != 0)
            aperture = np->value;
        else
        {
            np = IUFindNumber(nvp, "TELESCOPE_APERTURE");
            if (np)
                aperture = np->value;
        }

        np = IUFindNumber(nvp, "GUIDER_FOCAL_LENGTH");
        if (np && np->value != 0)
            focal_length = np->value;
        else
        {
            np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
            if (np)
                focal_length = np->value;
        }
    }

    if (ccd_hor_pixel != -1 && ccd_ver_pixel != -1 && focal_length != -1 && aperture != -1)
    {
        pmath->set_guider_params(ccd_hor_pixel, ccd_ver_pixel, aperture, focal_length);
        int x,y,w,h;

        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

        if (targetChip->getFrame(&x,&y,&w,&h))
            pmath->set_video_params(w, h);

        guider->fill_interface();

    }

    //qDebug() << "SetScope: ccd_pix_w " << ccd_hor_pixel << " - ccd_pix_h " << ccd_ver_pixel << " - focal length " << focal_length << " aperture " << aperture << endl;

}

void Guide::addST4(ISD::ST4 *newST4)
{
    ST4Combo->addItem(newST4->getDeviceName());
    ST4List.append(newST4);

    ST4Driver = ST4List.at(0);
    ST4Combo->setCurrentIndex(0);
}

bool Guide::capture()
{
    if (currentCCD == NULL)
        return false;

    double seqExpose = exposureSpin->value();

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    CCDFrameType ccdFrame = FRAME_LIGHT;

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        return false;
    }

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    targetChip->setCaptureMode(FITS_GUIDE);
    targetChip->setCaptureFilter( (FITSScale) filterCombo->currentIndex());

   targetChip->setFrameType(ccdFrame);

   targetChip->capture(seqExpose);

   return true;

}
void Guide::newFITS(IBLOB *bp)
{
    INDI_UNUSED(bp);

    FITSViewer *fv = currentCCD->getViewer();

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    FITSImage *targetImage = targetChip->getImage(FITS_GUIDE);

    targetImage->findStars();

    pmath->set_image(targetImage);
    guider->set_image(targetImage);
    calibration->set_image(targetImage);

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
         {
             guider->set_ready(true);
             tabWidget->setTabEnabled(1, true);
         }
    }

}


void Guide::appendLogText(const QString &text)
{

    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    emit newLog();
}

void Guide::clearLog()
{
    logText.clear();
    emit newLog();
}

bool Guide::do_pulse( GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs )
{
    if (ST4Driver == NULL)
        return false;

    return ST4Driver->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
}

bool Guide::do_pulse( GuideDirection dir, int msecs )
{
    if (ST4Driver == NULL)
        return false;

    return ST4Driver->doPulse(dir, msecs);

}

void Guide::newST4(int index)
{
    ST4Driver = ST4List.at(index);
}

double Guide::getReticleAngle()
{
    return calibration->getReticleAngle();
}

void Guide::viewerClosed()
{
    guider->set_image(NULL);
    calibration->set_image(NULL);
}

}

#include "guide.moc"
