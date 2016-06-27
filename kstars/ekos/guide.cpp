/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guide.h"

#include <QDateTime>

#include <KMessageBox>
#include <KLed>
#include <KLocalizedString>

#include <basedevice.h>

#include "Options.h"

#include "guide/gmath.h"
#include "guide/guider.h"
#include "phd2.h"

#include "indi/driverinfo.h"
#include "indi/clientmanager.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitsview.h"

#include "guide/rcalibration.h"
#include "guideadaptor.h"

namespace Ekos
{

Guide::Guide() : QWidget()
{
    setupUi(this);

    new GuideAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Guide",  this);

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
    calibration=NULL;
    guider=NULL;

    guideDeviationRA = guideDeviationDEC = 0;

    tabWidget = new QTabWidget(this);

    tabLayout->addWidget(tabWidget);

    exposureIN->setValue(Options::guideExposure());
    connect(exposureIN, SIGNAL(editingFinished()), this, SLOT(saveDefaultGuideExposure()));

    pmath = new cgmath();

    connect(pmath, SIGNAL(newAxisDelta(double,double)), this, SIGNAL(newAxisDelta(double,double)));
    connect(pmath, SIGNAL(newAxisDelta(double,double)), this, SLOT(updateGuideDriver(double,double)));

    calibration = new rcalibration(pmath, this);

    guider = new rguider(pmath, this);

    connect(guider, SIGNAL(ditherToggled(bool)), this, SIGNAL(ditherToggled(bool)));
    connect(guider, SIGNAL(autoGuidingToggled(bool)), this, SIGNAL(autoGuidingToggled(bool)));
    connect(guider, SIGNAL(ditherComplete()), this, SIGNAL(ditherComplete()));   

    tabWidget->addTab(calibration, calibration->windowTitle());
    tabWidget->addTab(guider, guider->windowTitle());
    tabWidget->setTabEnabled(1, false);

    connect(ST4Combo, SIGNAL(currentIndexChanged(int)), this, SLOT(newST4(int)));
    connect(ST4Combo, SIGNAL(activated(QString)), this, SLOT(setDefaultST4(QString)));

    connect(guiderCombo, SIGNAL(activated(QString)), this, SLOT(setDefaultCCD(QString)));
    connect(guiderCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(checkCCD(int)));

    connect(binningCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCCDBin(int)));

    foreach(QString filter, FITSViewer::filterTypes)
        filterCombo->addItem(filter);

    phd2 = new PHD2();
    connect(phd2, SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));
    connect(phd2, SIGNAL(newAxisDelta(double,double)), this, SIGNAL(newAxisDelta(double,double)));
    connect(phd2, SIGNAL(guideReady()), this, SIGNAL(guideReady()));
    connect(phd2, SIGNAL(autoGuidingToggled(bool)), this, SIGNAL(autoGuidingToggled(bool)));
    connect(phd2, SIGNAL(autoGuidingToggled(bool)), guider, SLOT(setGuideState(bool)));
    //connect(guider, SIGNAL(ditherToggled(bool)), phd2, SLOT(setDitherEnabled(bool)));
    connect(phd2, SIGNAL(ditherComplete()), this, SIGNAL(ditherComplete()));

    if (Options::usePHD2Guider())
        phd2->connectPHD2();
}

Guide::~Guide()
{
    delete guider;
    delete calibration;
    delete pmath;
    delete phd2;
}

void Guide::setDefaultCCD(QString ccd)
{
    Options::setDefaultGuideCCD(ccd);
}

void Guide::addCCD(ISD::GDInterface *newCCD)
{
    ISD::CCD *ccd = static_cast<ISD::CCD*>(newCCD);

    if (CCDs.contains(ccd))
        return;

    CCDs.append(ccd);

    guiderCombo->addItem(ccd->getDeviceName());

    //checkCCD(CCDs.count()-1);
    //guiderCombo->setCurrentIndex(CCDs.count()-1);

    setGuiderProcess(Options::useEkosGuider() ? GUIDE_INTERNAL : GUIDE_PHD2);
}

void Guide::addGuideHead(ISD::GDInterface *newCCD)
{
    ISD::CCD *ccd = static_cast<ISD::CCD *> (newCCD);

    CCDs.append(ccd);

    QString guiderName = ccd->getDeviceName() + QString(" Guider");

    if (guiderCombo->findText(guiderName) == -1)
    {
        guiderCombo->addItem(guiderName);
        //CCDs.append(static_cast<ISD::CCD *> (newCCD));
    }

    //checkCCD(CCDs.count()-1);
    //guiderCombo->setCurrentIndex(CCDs.count()-1);

    setGuiderProcess(Options::useEkosGuider() ? GUIDE_INTERNAL : GUIDE_PHD2);

 }

void Guide::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = (ISD::Telescope*) newTelescope;

    syncTelescopeInfo();

}

bool Guide::setCCD(QString device)
{
    for (int i=0; i < guiderCombo->count(); i++)
        if (device == guiderCombo->itemText(i))
        {
            guiderCombo->setCurrentIndex(i);
            return true;
        }

    return false;
}

void Guide::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
        ccdNum = guiderCombo->currentIndex();

    if (ccdNum <= CCDs.count())
    {
        currentCCD = CCDs.at(ccdNum);

        //connect(currentCCD, SIGNAL(FITSViewerClosed()), this, SLOT(viewerClosed()), Qt::UniqueConnection);
        connect(currentCCD, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processCCDNumber(INumberVectorProperty*)), Qt::UniqueConnection);
        connect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this, SLOT(checkExposureValue(ISD::CCDChip*,double,IPState)), Qt::UniqueConnection);

        if (currentCCD->hasGuideHead() && guiderCombo->currentText().contains("Guider"))
            useGuideHead=true;
        else
            useGuideHead=false;

        syncCCDInfo();
    }
}

void Guide::setGuiderProcess(int guiderProcess)
{    
    // Don't do anything unless we have a CCD and it is online
    if (currentCCD == NULL || currentCCD->isConnected() == false)
        return;

    if (guiderProcess == GUIDE_PHD2)
    {
        // Disable calibration tab
        tabWidget->setTabEnabled(0, false);
        // Enable guide tab
        tabWidget->setTabEnabled(1, true);
        // Set current tab to guide
        tabWidget->setCurrentIndex(1);

        guider->setPHD2(phd2);

        // Do not receive BLOBs from the driver
        currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_NEVER, currentCCD->getDeviceName(), useGuideHead ? "CCD2" : "CCD1");
    }
    else
    {
        // Enable calibration tab
        tabWidget->setTabEnabled(0, true);
        // Disable guide tab?
        // TODO: Check if calibration is already complete, then no need to disable guiding tab
        tabWidget->setTabEnabled(1, false);
        // Set current tab to calibration
        tabWidget->setCurrentIndex(0);

        guider->setPHD2(NULL);

        // Receive BLOBs from the driver
        currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), useGuideHead ? "CCD2" : "CCD1");
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
        phd2->setCCDMountParams(ccd_hor_pixel, ccd_ver_pixel, focal_length);

        int x,y,w,h;

        if (currentCCD->hasGuideHead() == false)
            useGuideHead = false;

        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

        if (targetChip == NULL)
        {
            appendLogText(i18n("Connection to the guide CCD is lost."));
            return;
        }

        binningCombo->setEnabled(targetChip->canBin());
        if (targetChip->canBin())
        {
            int binX,binY, maxBinX, maxBinY;
            targetChip->getBinning(&binX, &binY);
            targetChip->getMaxBin(&maxBinX, &maxBinY);

            binningCombo->disconnect();

            binningCombo->clear();

            for (int i=1; i <= maxBinX; i++)
                binningCombo->addItem(QString("%1x%2").arg(i).arg(i));

            binningCombo->setCurrentIndex(binX-1);

            connect(binningCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCCDBin(int)));
        }

        emit guideChipUpdated(targetChip);

        guider->setTargetChip(targetChip);

        if (targetChip->getFrame(&x,&y,&w,&h))
            pmath->set_video_params(w, h);

        guider->setInterface();

    }
}

void Guide::addST4(ISD::ST4 *newST4)
{
    foreach(ISD::ST4 *guidePort, ST4List)
    {
        if (!strcmp(guidePort->getDeviceName(),newST4->getDeviceName()))
            return;
    }

    ST4List.append(newST4);

    ST4Combo->addItem(newST4->getDeviceName());        
}

bool Guide::setST4(QString device)
{
    for (int i=0; i < ST4List.count(); i++)
        if (ST4List.at(i)->getDeviceName() == device)
        {
            ST4Combo->setCurrentIndex(i);
            return true;
        }

    return false;
}

void Guide::setDefaultST4(QString st4)
{
    Options::setDefaultST4Driver(st4);
}

void Guide::newST4(int index)
{
    if (ST4List.empty() || index >= ST4List.count())
        return;

    ST4Driver = ST4List.at(index);

    GuideDriver = ST4Driver;
}

void Guide::setAO(ISD::ST4 *newAO)
{
    AODriver = newAO;
    guider->setAO(true);
}

bool Guide::capture()
{
    if (currentCCD == NULL)
        return false;

    double seqExpose = exposureIN->value();

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    CCDFrameType ccdFrame = FRAME_LIGHT;

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        return false;
    }

    //If calibrating, reset frame
    if (calibration->getCalibrationStage() == rcalibration::CAL_CAPTURE_IMAGE)
    {
        targetChip->resetFrame();
        guider->setSubFramed(false);
    }

    // Exposure changed, take a new dark
    if (useDarkFrame && darkExposure != seqExpose)
    {
        darkExposure = seqExpose;

        // Load an image from the dark library. If not found, then capture a dark frame
        if (loadDarkFrame(seqExpose) == false)
        {
            targetChip->setFrameType(FRAME_DARK);

            if (calibration->useAutoStar() == false)
                KMessageBox::information(NULL, i18n("If the guider camera if not equipped with a shutter, cover the telescope or camera in order to take a dark exposure."), i18n("Dark Exposure"), "dark_exposure_dialog_notification");

            connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
            targetChip->capture(seqExpose);

            appendLogText(i18n("Taking a dark frame. "));

            return true;
        }
    }

    targetChip->setCaptureMode(FITS_GUIDE);
    targetChip->setFrameType(ccdFrame);
    if (darkImage == NULL || calibration->useDarkFrame() == false)
        targetChip->setCaptureFilter((FITSScale) filterCombo->currentIndex());

    if (guider->isGuiding())
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

    //FITSViewer *fv = currentCCD->getViewer();

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    // Received a dark calibration frame
    if (targetChip->getFrameType() == FRAME_DARK)
    {
        FITSView *targetImage = targetChip->getImage(FITS_CALIBRATE);
        if (targetImage)
        {
            delete (darkImage);
            darkImage = targetImage->getImageData();

            // Save dark frame in the library
            saveDarkFrame();

            capture();
        }
        else
            appendLogText(i18n("Dark frame processing failed."));

       return;
    }

    FITSView *targetImage = targetChip->getImage(FITS_GUIDE);

    if (targetImage == NULL)
    {
        if (Options::guideLogging())
            qDebug() << "Guide: guide frame is missing! Capturing again...";

        capture();
        return;
    }

    /*if (targetImage == NULL)
    {
        pmath->set_image(NULL);
        guider->setImage(NULL);
        calibration->setImage(NULL);
        return;
    }*/

    if (Options::guideLogging())
        qDebug() << "Guide: recieved guide frame.";

    FITSData *image_data = targetImage->getImageData();

    Q_ASSERT(image_data);

    if (darkImage && darkImage->getImageBuffer() != image_data->getDarkFrame())
    {
        image_data->setDarkFrame(darkImage->getImageBuffer());
        image_data->applyFilter((FITSScale) filterCombo->currentIndex());
        targetImage->rescale(ZOOM_KEEP_LEVEL);
        targetImage->updateFrame();
    }

    pmath->set_image(targetImage);
    guider->setImage(targetImage);

    //fv->show();

    // It should be false in case we do not need to process the image for motion
    // which happens when we take an image for auto star selection.
    if (calibration->setImage(targetImage) == false)
        return;

    if (isSuspended)
    {
        //capture();
        if (Options::guideLogging())
            qDebug() << "Guide: Guider is suspended.";
        return;
    }

    if (guider->isDithering())
    {
        pmath->do_processing();
        if (guider->dither() == false)
        {
            appendLogText(i18n("Dithering failed. Autoguiding aborted."));
            guider->abort();
            emit ditherFailed();
        }
    }
    else if (guider->isGuiding())
    {
        guider->guide();

         if (guider->isGuiding())
            capture();
    }
    else if (calibration->isCalibrating())
    {
        GuideDriver = ST4Driver;
        pmath->do_processing();
        calibration->processCalibration();

         if (calibration->isCalibrationComplete())
         {
             guider->setReady(true);
             tabWidget->setTabEnabled(1, true);
             emit guideReady();
         }
    }

}

void Guide::appendLogText(const QString &text)
{

    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    if (Options::guideLogging())
        qDebug() << "Guide: " << text;

    emit newLog();
}

void Guide::clearLog()
{
    logText.clear();
    emit newLog();
}

void Guide::setDECSwap(bool enable)
{
    if (ST4Driver == NULL || guider == NULL)
        return;

    guider->setDECSwap(enable);
    ST4Driver->setDECSwap(enable);

}

bool Guide::sendPulse( GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs )
{
    if (GuideDriver == NULL || (ra_dir == NO_DIR && dec_dir == NO_DIR))
        return false;

    if (calibration->isCalibrating())
        QTimer::singleShot( (ra_msecs > dec_msecs ? ra_msecs : dec_msecs) + 100, this, SLOT(capture()));

    return GuideDriver->doPulse(ra_dir, ra_msecs, dec_dir, dec_msecs);
}

bool Guide::sendPulse( GuideDirection dir, int msecs )
{
    if (GuideDriver == NULL || dir==NO_DIR)
        return false;

    if (calibration->isCalibrating())
        QTimer::singleShot(msecs+100, this, SLOT(capture()));

    return GuideDriver->doPulse(dir, msecs);

}

QStringList Guide::getST4Devices()
{
    QStringList devices;

    foreach(ISD::ST4* driver, ST4List)
        devices << driver->getDeviceName();

    return devices;
}

double Guide::getReticleAngle()
{
    return calibration->getReticleAngle();
}

/*void Guide::viewerClosed()
{
    pmath->set_image(NULL);
    guider->setImage(NULL);
    calibration->setImage(NULL);
}*/

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
        guider->setImage(NULL);
        calibration->setImage(NULL);
    }

    if (rapidGuideReticleSet == false)
    {
        // Let's set reticle parameter on first capture to those of the star, then we check if there
        // is any set
        double x,y,angle;
        pmath->get_reticle_params(&x, &y, &angle);
        pmath->set_reticle_params(dx, dy, angle);
        rapidGuideReticleSet = true;
    }

    pmath->setRapidStarData(dx, dy);

    if (guider->isDithering())
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


bool Guide::isDithering()
{
    return (Options::useEkosGuider() ? guider->isDithering() : phd2->isDithering());
}

void Guide::dither()
{
   if (Options::useEkosGuider())
   {
       if (isDithering() == false)
            guider->dither();
   }
   else
   {
       if (isDithering() == false)
           phd2->dither(guider->getDitherPixels());
   }

}

void Guide::updateGuideDriver(double delta_ra, double delta_dec)
{
    guideDeviationRA  = delta_ra;
    guideDeviationDEC = delta_dec;

    // If using PHD2 or not guiding, no need to go further on
    if (Options::usePHD2Guider() || isGuiding() == false)
        return;

    if (isDithering())
    {
        GuideDriver = ST4Driver;
        return;
    }

    // Guide via AO only if guiding deviation is below AO limit
    if (AODriver != NULL && (fabs(delta_ra) < guider->getAOLimit()) && (fabs(delta_dec) < guider->getAOLimit()))
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

bool Guide::isCalibrationComplete()
{
    if (Options::useEkosGuider())
        return calibration->isCalibrationComplete();
    else
        return phd2->isCalibrationComplete();

}

bool Guide::isCalibrationSuccessful()
{
    if (Options::useEkosGuider())
        return calibration->isCalibrationSuccessful();
    else
        return phd2->isCalibrationSuccessful();
}

bool Guide::startCalibration()
{
    if (Options::useEkosGuider())
        return calibration->startCalibration();
    else
        return phd2->startGuiding();
}

bool Guide::stopCalibration()
{
    if (Options::useEkosGuider())
        return calibration->stopCalibration();
    else
        return phd2->stopGuiding();
}

bool Guide::isCalibrating()
{
    if (Options::useEkosGuider())
        return calibration->isCalibrating();
    else
        return phd2->isCalibrating();
}

bool Guide::isGuiding()
{
    if (Options::useEkosGuider())
        return guider->isGuiding();
    else
        return phd2->isGuiding();
}

bool Guide::startGuiding()
{    
    // This will handle both internal and external guiders
    return guider->start();

    /*if (Options::useEkosGuider())
        return guider->start();
    else
        return phd2->startGuiding();*/
}

bool Guide::stopGuiding()
{
    isSuspended=false;

    if (Options::useEkosGuider())
        return guider->abort(true);
    else
        // guider stop will call phd2->stopGuide() and change GUI elements accordingly
        return guider->stop();
}

void Guide::setSuspended(bool enable)
{
    if (enable == isSuspended || (enable && isGuiding() == false))
        return;

    isSuspended = enable;

    if (isSuspended)
    {
        if (Options::usePHD2Guider())
            phd2->pauseGuiding();
    }
    else
    {
        if (Options::useEkosGuider())
            capture();
        else
            phd2->resumeGuiding();
            //phd2->startGuiding();
    }

    if (isSuspended)
        appendLogText(i18n("Guiding suspended."));
    else
        appendLogText(i18n("Guiding resumed."));
}

void Guide::setExposure(double value)
{
    exposureIN->setValue(value);
}


void Guide::setImageFilter(const QString & value)
{
    for (int i=0; i < filterCombo->count(); i++)
        if (filterCombo->itemText(i) == value)
        {
            filterCombo->setCurrentIndex(i);
            break;
        }
}

void Guide::setCalibrationTwoAxis(bool enable)
{
    calibration->setCalibrationTwoAxis(enable);
}

void Guide::setCalibrationAutoStar(bool enable)
{
    calibration->setCalibrationAutoStar(enable);
}

void Guide::setCalibrationAutoSquareSize(bool enable)
{
    calibration->setCalibrationAutoSquareSize(enable);
}

void Guide::setCalibrationDarkFrame(bool enable)
{
    calibration->setCalibrationDarkFrame(enable);
}

void Guide::setCalibrationParams(int boxSize, int pulseDuration)
{
    calibration->setCalibrationParams(boxSize, pulseDuration);
}

void Guide::setGuideBoxSize(int boxSize)
{
     guider->setGuideOptions(boxSize, guider->getAlgorithm(), guider->useSubFrame(), guider->useRapidGuide());
}

void Guide::setGuideAlgorithm(const QString & algorithm)
{
     guider->setGuideOptions(guider->getBoxSize(), algorithm, guider->useSubFrame(), guider->useRapidGuide());
}

void Guide::setGuideSubFrame(bool enable)
{
     guider->setGuideOptions(guider->getBoxSize(), guider->getAlgorithm(), enable , guider->useRapidGuide());
}

void Guide::setGuideRapid(bool enable)
{
     guider->setGuideOptions(guider->getBoxSize(), guider->getAlgorithm(), guider->useSubFrame() , enable);
}

void Guide::setDither(bool enable, double value)
{
    guider->setDither(enable, value);
}

QList<double> Guide::getGuidingDeviation()
{
    QList<double> deviation;

    deviation << guideDeviationRA << guideDeviationDEC;

    return deviation;
}

void Guide::startAutoCalibrateGuiding()
{
    if (Options::useEkosGuider())
        connect(calibration, SIGNAL(calibrationCompleted(bool)), this, SLOT(checkAutoCalibrateGuiding(bool)));
    else
        connect(phd2, SIGNAL(calibrationCompleted(bool)), this, SLOT(checkAutoCalibrateGuiding(bool)));

    startCalibration();
}

void Guide::checkAutoCalibrateGuiding(bool successful)
{
    if (Options::useEkosGuider())
        disconnect(calibration, SIGNAL(calibrationCompleted(bool)), this, SLOT(checkAutoCalibrateGuiding(bool)));
    else
        disconnect(phd2, SIGNAL(calibrationCompleted(bool)), this, SLOT(checkAutoCalibrateGuiding(bool)));

    if (successful)
    {
        appendLogText(i18n("Auto calibration successful. Starting guiding..."));
        startGuiding();
    }
    else
    {
        appendLogText(i18n("Auto calibration failed."));
    }
}

void Guide::updateCCDBin(int index)
{
    if (currentCCD == NULL && Options::usePHD2Guider())
        return;

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    targetChip->setBinning(index+1, index+1);
}

void Guide::processCCDNumber(INumberVectorProperty *nvp)
{
    if (currentCCD == NULL || strcmp(nvp->device, currentCCD->getDeviceName()))
        return;

    if ( (!strcmp(nvp->name, "CCD_BINNING") && useGuideHead == false) || (!strcmp(nvp->name, "GUIDER_BINNING") && useGuideHead) )
    {
        binningCombo->disconnect();
        binningCombo->setCurrentIndex(nvp->np[0].value-1);
        connect(binningCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCCDBin(int)));
    }        
}

void Guide::checkExposureValue(ISD::CCDChip *targetChip, double exposure, IPState state)
{
    INDI_UNUSED(exposure);

    if (state == IPS_ALERT && (guider->isGuiding() || guider->isDithering() || calibration->isCalibrating()))
    {
        appendLogText(i18n("Exposure failed. Restarting exposure..."));
        targetChip->capture(exposureIN->value());
    }
}

bool Guide::loadDarkFrame(double exposure)
{
    QString filename = QString("dark-%1-%2.fits").arg(QString(currentCCD->getDeviceName()).replace(" ", "_")).arg(QString::number(exposure, 'f', 2));
    QString path     = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QDir::separator() + filename;
    if (darkImage == NULL)
        darkImage = new FITSData();

    return darkImage->loadFITS(path);
}

void Guide::saveDarkFrame()
{
    QString filename = QString("dark-%1-%2.fits").arg(QString(currentCCD->getDeviceName()).replace(" ", "_")).arg(QString::number(darkExposure, 'f', 2));
    QString path     = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QDir::separator() + filename;

    if (darkImage->saveFITS(path) == 0)
        appendLogText(i18n("Saved new dark frame %1 to library.", path));
    else
        appendLogText(i18n("Failed to save dark frame to library!"));
}

void Guide::setUseDarkFrame(bool enable)
{
    useDarkFrame = enable;

    if (enable && calibration && calibration->useAutoStar())
        appendLogText(i18n("Warning: In auto mode, you will not be asked to cover cameras unequipped with shutters in order to capture a dark frame. The dark frame capture will proceed without warning."
                           " You can capture dark frames with auto mode off and they shall be saved in the dark library for use when ever needed."));
}

void Guide::saveDefaultGuideExposure()
{
    Options::setGuideExposure(exposureIN->value());
}

}


