/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guide.h"

#include <QDateTime>

#include "guide/gmath.h"
#include "guide/guider.h"
#include "Options.h"


#include <KMessageBox>
#include <KLed>

#include "indi/driverinfo.h"
#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitsview.h"

#include "guide/rcalibration.h"

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
    useDarkFrame = false;
    rapidGuideReticleSet = false;
    isSuspended = false;
    darkExposure = 0;
    darkImage = NULL;
    AODriver= NULL;
    GuideDriver=NULL;

    tabWidget = new QTabWidget(this);

    tabLayout->addWidget(tabWidget);

    guiderStage = CALIBRATION_STAGE;

    pmath = new cgmath();

    connect(pmath, SIGNAL(newAxisDelta(double,double)), this, SIGNAL(newAxisDelta(double,double)));
    connect(pmath, SIGNAL(newAxisDelta(double,double)), this, SLOT(updateGuideDriver(double,double)));

    calibration = new rcalibration(this);
    calibration->set_math(pmath);

    guider = new rguider(this);
    guider->set_math(pmath);

    connect(guider, SIGNAL(ditherToggled(bool)), this, SIGNAL(ditherToggled(bool)));
    connect(guider, SIGNAL(autoGuidingToggled(bool,bool)), this, SIGNAL(autoGuidingToggled(bool,bool)));
    connect(guider, SIGNAL(ditherComplete()), this, SIGNAL(ditherComplete()));

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

    if (currentCCD->hasGuideHead())
        addGuideHead(newCCD);

    connect(currentCCD, SIGNAL(FITSViewerClosed()), this, SLOT(viewerClosed()));

    syncCCDInfo();

    //qDebug() << "SetCCD: ccd_pix_w " << ccd_hor_pixel << " - ccd_pix_h " << ccd_ver_pixel << " - focal length " << focal_length << " aperture " << aperture << endl;

}

void Guide::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = (ISD::Telescope*) newTelescope;

    syncTelescopeInfo();

}

void Guide::addGuideHead(ISD::GDInterface *ccd)
{
    if (currentCCD == NULL)
        currentCCD = (ISD::CCD *) ccd;

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

    if (currentCCD == NULL)
        return;

    if (useGuideHead)
        nvp = currentCCD->getBaseDevice()->getNumber("GUIDER_INFO");
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

    updateGuideParams();
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

    updateGuideParams();

}

void Guide::updateGuideParams()
{
    if (ccd_hor_pixel != -1 && ccd_ver_pixel != -1 && focal_length != -1 && aperture != -1)
    {
        pmath->set_guider_params(ccd_hor_pixel, ccd_ver_pixel, aperture, focal_length);
        int x,y,w,h;

        if (currentCCD->hasGuideHead() == false)
            useGuideHead = false;

        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

        if (targetChip == NULL)
        {
            appendLogText(i18n("Connection to the guide CCD is lost."));
            return;
        }

        guider->set_target_chip(targetChip);

        if (targetChip->getFrame(&x,&y,&w,&h))
            pmath->set_video_params(w, h);

        guider->fill_interface();

    }
}

void Guide::addST4(ISD::ST4 *newST4)
{
    foreach(ISD::ST4 *guidePort, ST4List)
    {
        if (guidePort == newST4)
            return;
    }

    ST4Combo->addItem(newST4->getDeviceName());
    ST4List.append(newST4);

    ST4Driver = ST4List.at(0);
    GuideDriver = ST4Driver;
    ST4Combo->setCurrentIndex(0);

}

void Guide::setAO(ISD::ST4 *newAO)
{
    AODriver = newAO;
    guider->set_ao(true);
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

    // Exposure changed, take a new dark
    if (useDarkFrame && darkExposure != seqExpose)
    {
        darkExposure = seqExpose;
        targetChip->setFrameType(FRAME_DARK);

        KMessageBox::information(NULL, i18n("If the guider camera if not equipped with a shutter, cover the telescope or camera in order to take a dark exposure."), i18n("Dark Exposure"), "dark_exposure_dialog_notification");

        connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
        targetChip->capture(seqExpose);

        appendLogText(i18n("Taking a dark frame. "));

        return true;
    }

    targetChip->setCaptureMode(FITS_GUIDE);
    targetChip->setCaptureFilter( (FITSScale) filterCombo->currentIndex());
    targetChip->setFrameType(ccdFrame);

    if (guider->is_guiding())
    {
         if (guider->isRapidGuide() == false)
             connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

         targetChip->capture(seqExpose);
         return true;
    }

    connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    targetChip->capture(seqExpose);

    return true;

}
void Guide::newFITS(IBLOB *bp)
{
    INDI_UNUSED(bp);

    FITSViewer *fv = currentCCD->getViewer();

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    // Received a dark calibration frame
    if (targetChip->getFrameType() == FRAME_DARK)
    {
        FITSView *targetImage = targetChip->getImage(FITS_CALIBRATE);
        if (targetImage)
        {
            darkImage = targetImage->getImageData();
            capture();
        }
        else
            appendLogText(i18n("Dark frame processing failed."));

       return;
    }

    FITSView *targetImage = targetChip->getImage(FITS_GUIDE);

    if (targetImage == NULL)
    {
        pmath->set_image(NULL);
        guider->set_image(NULL);
        calibration->set_image(NULL);
        return;
    }

    FITSImage *image_data = targetImage->getImageData();

    if (image_data == NULL)
        return;

    if (darkImage)
        image_data->subtract(darkImage->getImageBuffer());

    pmath->set_image(targetImage);
    guider->set_image(targetImage);
    calibration->set_image(targetImage);

    fv->show();

    if (isSuspended)
    {
        capture();
        return;
    }

    if (guider->is_dithering())
    {
        pmath->do_processing();
        if (guider->dither() == false)
        {
            appendLogText(i18n("Dithering failed. Autoguiding aborted."));
            guider->abort();
            emit ditherFailed();
        }
    }
    else if (guider->is_guiding())
    {
        guider->guide();

         if (guider->is_guiding())
            capture();
    }
    else if (calibration->is_calibrating())
    {
        GuideDriver = ST4Driver;
        pmath->do_processing();
        calibration->process_calibration();

         if (calibration->is_finished())
         {
             guider->set_ready(true);
             tabWidget->setTabEnabled(1, true);
             emit guideReady();
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

void Guide::setDECSwap(bool enable)
{
    if (ST4Driver == NULL)
        return;

    ST4Driver->setDECSwap(enable);
}

bool Guide::do_pulse( GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs )
{
    if (GuideDriver == NULL || (ra_dir == NO_DIR && dec_dir == NO_DIR))
        return false;

    if (calibration->is_calibrating())
        QTimer::singleShot( (ra_msecs > dec_msecs ? ra_msecs : dec_msecs) + 100, this, SLOT(capture()));

    return GuideDriver->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
}

bool Guide::do_pulse( GuideDirection dir, int msecs )
{
    if (GuideDriver == NULL || dir==NO_DIR)
        return false;

    if (calibration->is_calibrating())
        QTimer::singleShot(msecs+100, this, SLOT(capture()));

    return GuideDriver->doPulse(dir, msecs);

}

void Guide::newST4(int index)
{
    if (ST4List.empty() || index >= ST4List.count())
        return;

    ST4Driver = ST4List.at(index);
    GuideDriver = ST4Driver;

}

double Guide::getReticleAngle()
{
    return calibration->getReticleAngle();
}

void Guide::viewerClosed()
{
    pmath->set_image(NULL);
    guider->set_image(NULL);
    calibration->set_image(NULL);
}

void Guide::processRapidStarData(ISD::CCDChip *targetChip, double dx, double dy, double fit)
{
    // Check if guide star is lost
    if (dx == -1 && dy == -1 && fit == -1)
    {
        KMessageBox::error(NULL, i18n("Lost track of the guide star. Rapid guide aborted."));
        guider->abort();
        return;
    }

    FITSView *targetImage = targetChip->getImage(FITS_GUIDE);

    if (targetImage == NULL)
    {
        pmath->set_image(NULL);
        guider->set_image(NULL);
        calibration->set_image(NULL);
    }

    if (rapidGuideReticleSet == false)
    {
        // Let's set reticle parameter on first capture to those of the star, then we check if there
        // is any deviation
        double x,y,angle;
        pmath->get_reticle_params(&x, &y, &angle);
        pmath->set_reticle_params(dx, dy, angle);
        rapidGuideReticleSet = true;
    }

    pmath->setRapidStarData(dx, dy);

    if (guider->is_dithering())
    {
        pmath->do_processing();
        if (guider->dither() == false)
        {
            appendLogText(i18n("Dithering failed. Autoguiding aborted."));
            guider->abort();
            emit ditherFailed();
        }
    }
    else
    {
        guider->guide();
        capture();
    }

}

void Guide::startRapidGuide()
{
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (currentCCD->setRapidGuide(targetChip, true) == false)
    {
        appendLogText(i18n("The CCD does not support Rapid Guiding. Aborting..."));
        guider->abort();
        return;
    }

    rapidGuideReticleSet = false;

    pmath->setRapidGuide(true);
    currentCCD->configureRapidGuide(targetChip, true);
    connect(currentCCD, SIGNAL(newGuideStarData(ISD::CCDChip*,double,double,double)), this, SLOT(processRapidStarData(ISD::CCDChip*,double,double,double)));

}

void Guide::stopRapidGuide()
{
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    pmath->setRapidGuide(false);

    rapidGuideReticleSet = false;

    currentCCD->disconnect(SIGNAL(newGuideStarData(ISD::CCDChip*,double,double,double)));

    currentCCD->configureRapidGuide(targetChip, false, false, false);

    currentCCD->setRapidGuide(targetChip, false);

}


void Guide::dither()
{
   if (guider->is_dithering() == false)
        guider->dither();
}

void Guide::updateGuideDriver(double delta_ra, double delta_dec)
{
    if (guider->is_guiding() == false)
        return;

    if (guider->is_dithering())
    {
        GuideDriver = ST4Driver;
        return;
    }

    // Guide via AO only if guiding deviation is below AO limit
    if (AODriver != NULL && (fabs(delta_ra) < guider->get_ao_limit()) && (fabs(delta_dec) < guider->get_ao_limit()))
    {
        if (AODriver != GuideDriver)
                appendLogText(i18n("Using %1 to correct for guiding errors.", AODriver->getDeviceName()));
        GuideDriver = AODriver;
        return;
    }

    if (GuideDriver != ST4Driver)
        appendLogText(i18n("Using %1 to correct for guiding errors.", ST4Driver->getDeviceName()));

    GuideDriver = ST4Driver;
}


}

#include "guide.moc"
