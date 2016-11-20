/*  Ekos Alignment Module
    Copyright (C) 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QProcess>

#include "kstars.h"
#include "kstarsdata.h"
#include "align.h"
#include "dms.h"
#include "fov.h"
#include "ekos/auxiliary/darklibrary.h"

#include "Options.h"

#include <QFileDialog>
#include <KMessageBox>
#include <KNotifications/KNotification>

#include "ekos/auxiliary/QProgressIndicator.h"
#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "alignadaptor.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsview.h"

#include "ekos/ekosmanager.h"

#include "onlineastrometryparser.h"
#include "offlineastrometryparser.h"
#include "remoteastrometryparser.h"

#include <basedevice.h>

#define MAXIMUM_SOLVER_ITERATIONS   10

namespace Ekos
{

// 30 arcmiutes RA movement
const double Align::RAMotion = 0.5;
// Sidereal rate, degrees/s
const float Align::SIDRATE  = 0.004178;

Align::Align()
{
    setupUi(this);
    new AlignAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Align",  this);

    dirPath = QDir::homePath();

    state = ALIGN_IDLE;
    focusState = FOCUS_IDLE;

    currentCCD     = NULL;
    currentTelescope = NULL;
    currentFilter = NULL;
    useGuideHead = false;
    canSync = false;
    //loadSlewMode = false;
    loadSlewState=IPS_IDLE;
    //m_isSolverComplete = false;
    //m_isSolverSuccessful = false;
    //m_slewToTargetSelected=false;
    m_wcsSynced=false;
    //isFocusBusy=false;
    ccd_hor_pixel =  ccd_ver_pixel =  focal_length =  aperture = sOrientation = sRA = sDEC = -1;
    decDeviation = azDeviation = altDeviation = 0;

    rememberUploadMode = ISD::CCD::UPLOAD_CLIENT;
    currentFilter = NULL;
    filterPositionPending = false;
    lockedFilterIndex = currentFilterIndex = -1;
    retries=0;
    targetDiff=1e6;
    solverIterations=0;
    fov_x=fov_y=0;

    parser = NULL;
    solverFOV = new FOV();
    solverFOV->setColor(KStars::Instance()->data()->colorScheme()->colorNamed( "SolverFOVColor" ).name());
    onlineParser = NULL;
    offlineParser = NULL;
    remoteParser = NULL;

    connect(solveB, SIGNAL(clicked()), this, SLOT(captureAndSolve()));
    connect(stopB, SIGNAL(clicked()), this, SLOT(abort()));
    connect(measureAltB, SIGNAL(clicked()), this, SLOT(measureAltError()));
    connect(measureAzB, SIGNAL(clicked()), this, SLOT(measureAzError()));
    connect(raBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
    connect(decBox, SIGNAL(textChanged( const QString & ) ), this, SLOT( checkLineEdits() ) );
    connect(syncBoxesB, SIGNAL(clicked()), this, SLOT(copyCoordsToBoxes()));
    connect(clearBoxesB, SIGNAL(clicked()), this, SLOT(clearCoordBoxes()));

    connect(CCDCaptureCombo, SIGNAL(activated(QString)), this, SLOT(setDefaultCCD(QString)));
    connect(CCDCaptureCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(checkCCD(int)));

    connect(correctAltB, SIGNAL(clicked()), this, SLOT(correctAltError()));
    connect(correctAzB, SIGNAL(clicked()), this, SLOT(correctAzError()));
    connect(loadSlewB, SIGNAL(clicked()), this, SLOT(loadAndSlew()));

    gotoModeButtonGroup->setId(syncR, GOTO_SYNC);
    gotoModeButtonGroup->setId(slewR, GOTO_SLEW);
    gotoModeButtonGroup->setId(nothingR, GOTO_NOTHING);

    connect(gotoModeButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, [=](int id){ this->currentGotoMode = static_cast<GotoMode>(id); });

    // TODO make this configurable, 3 minutes timeout
    alignTimer.setInterval(180*1000);
    connect(&alignTimer, SIGNAL(timeout()), this, SLOT(checkAlignmentTimeout()));

    currentGotoMode = static_cast<GotoMode>(Options::solverGotoOption());
    gotoModeButtonGroup->button(currentGotoMode)->setChecked(true);

    syncBoxesB->setIcon(QIcon::fromTheme("edit-copy", QIcon(":/icons/breeze/default/edit-copy.svg")));
    clearBoxesB->setIcon(QIcon::fromTheme("edit-clear", QIcon(":/icons/breeze/default/edit-clear.svg")));

    raBox->setDegType(false); //RA box should be HMS-style

    appendLogText(i18n("Idle."));

    pi = new QProgressIndicator(this);

    controlBox->layout()->addWidget(pi);

    exposureIN->setValue(Options::alignExposure());

    altStage = ALT_INIT;
    azStage  = AZ_INIT;

    // Online/Offline/Remote solver check
    solverTypeGroup->setId(onlineSolverR, SOLVER_ONLINE);
    solverTypeGroup->setId(offlineSolverR, SOLVER_OFFLINE);
    solverTypeGroup->setId(remoteSolverR, SOLVER_REMOTE);
    solverTypeGroup->button(Options::solverType())->setChecked(true);
    connect(solverTypeGroup, SIGNAL(buttonClicked(int)), SLOT(setSolverType(int)));

    switch(solverTypeGroup->checkedId())
    {
    case SOLVER_ONLINE:
        onlineParser = new Ekos::OnlineAstrometryParser();
        parser = onlineParser;
        break;

    case SOLVER_OFFLINE:
        offlineParser = new OfflineAstrometryParser();
        parser = offlineParser;
        break;

    case SOLVER_REMOTE:
        remoteParser = new RemoteAstrometryParser();
        parser = remoteParser;
        break;
    }

    parser->setAlign(this);
    if (parser->init() == false)
        setEnabled(false);
    else
    {
        connect(parser, SIGNAL(solverFinished(double,double,double, double)), this, SLOT(solverFinished(double,double,double, double)), Qt::UniqueConnection);
        connect(parser, SIGNAL(solverFailed()), this, SLOT(solverFailed()), Qt::UniqueConnection);
    }

    solverOptions->setText(Options::solverOptions());

    // Which telescope info to use for FOV calculations
    kcfg_solverOTA->setChecked(Options::solverOTA());
    connect(kcfg_solverOTA, SIGNAL(toggled(bool)), this, SLOT(syncTelescopeInfo()));

    accuracySpin->setValue(Options::solverAccuracyThreshold());
    alignDarkFrameCheck->setChecked(Options::alignDarkFrame());

    delaySpin->setValue(Options::settlingTime());
    connect(delaySpin, SIGNAL(editingFinished()), this, SLOT(saveSettleTime()));

    connect(binningCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setBinningIndex(int)));
}

Align::~Align()
{
    delete(pi);
    delete(solverFOV);
    delete(parser);
}

bool Align::isParserOK()
{
    bool rc = parser->init();

    if (rc)
    {
        connect(parser, SIGNAL(solverFinished(double,double,double,double)), this, SLOT(solverFinished(double,double,double,double)), Qt::UniqueConnection);
        connect(parser, SIGNAL(solverFailed()), this, SLOT(solverFailed()), Qt::UniqueConnection);
    }

    return rc;
}

void Align::checkAlignmentTimeout()
{
    if (loadSlewState != IPS_IDLE || ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
        abort();
    else if (loadSlewState == IPS_IDLE)
    {
        appendLogText(i18n("Solver timed out"));
        captureAndSolve();
    }
    // TODO must also account for loadAndSlew. Retain file name
}

void Align::setSolverType(int type)
{
    switch(type)
    {
    case SOLVER_ONLINE:
        if (onlineParser != NULL)
        {
            parser = onlineParser;
            return;
        }

        onlineParser = new Ekos::OnlineAstrometryParser();
        parser = onlineParser;
        break;

    case SOLVER_OFFLINE:
        if (offlineParser != NULL)
        {
            parser = offlineParser;

            return;
        }

        offlineParser = new Ekos::OfflineAstrometryParser();
        parser = offlineParser;
        break;

    case SOLVER_REMOTE:
        if (remoteParser != NULL)
        {
            parser = remoteParser;
            (dynamic_cast<RemoteAstrometryParser*>(parser))->setCCD(currentCCD);
            return;
        }

        remoteParser = new Ekos::RemoteAstrometryParser();
        parser = remoteParser;
        (dynamic_cast<RemoteAstrometryParser*>(parser))->setCCD(currentCCD);
        break;
    }

    parser->setAlign(this);
    if (parser->init())
    {
        connect(parser, SIGNAL(solverFinished(double,double,double, double)), this, SLOT(solverFinished(double,double,double, double)), Qt::UniqueConnection);
        connect(parser, SIGNAL(solverFailed()), this, SLOT(solverFailed()), Qt::UniqueConnection);
    }
    else
        parser->disconnect();

}

bool Align::setCCD(QString device)
{
    for (int i=0; i < CCDCaptureCombo->count(); i++)
        if (device == CCDCaptureCombo->itemText(i))
        {
            CCDCaptureCombo->setCurrentIndex(i);
            return true;
        }

    return false;
}

void Align::setDefaultCCD(QString ccd)
{
    Options::setDefaultAlignCCD(ccd);
}

void Align::checkCCD(int ccdNum)
{
    if (ccdNum == -1)
    {
        ccdNum = CCDCaptureCombo->currentIndex();

        if (ccdNum == -1)
            return;
    }

    if (ccdNum <= CCDs.count())
    {
        currentCCD = CCDs.at(ccdNum);

        if (solverTypeGroup->checkedId() == SOLVER_REMOTE)
            (dynamic_cast<RemoteAstrometryParser*>(parser))->setCCD(currentCCD);
    }

    syncCCDInfo();

}

void Align::addCCD(ISD::GDInterface *newCCD)
{
    if (CCDs.contains(static_cast<ISD::CCD *>(newCCD)))
    {
       syncCCDInfo();
       return;
    }

    CCDs.append(static_cast<ISD::CCD *>(newCCD));

    CCDCaptureCombo->addItem(newCCD->getDeviceName());
}

void Align::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = static_cast<ISD::Telescope*> (newTelescope);

    connect(currentTelescope, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processTelescopeNumber(INumberVectorProperty*)));

    syncTelescopeInfo();
}

void Align::syncTelescopeInfo()
{
    INumberVectorProperty * nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {
        INumber *np = NULL;

        if (kcfg_solverOTA->isChecked())
            np = IUFindNumber(nvp, "GUIDER_APERTURE");
        else
            np = IUFindNumber(nvp, "TELESCOPE_APERTURE");

        if (np && np->value > 0)
            aperture = np->value;

        if (kcfg_solverOTA->isChecked())
            np = IUFindNumber(nvp, "GUIDER_FOCAL_LENGTH");
        else
            np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");

        if (np && np->value > 0)
            focal_length = np->value;
    }

    if (focal_length == -1 || aperture == -1)
        return;

    if (ccd_hor_pixel != -1 && ccd_ver_pixel != -1 && focal_length != -1 && aperture != -1)
        calculateFOV();

    if (currentCCD && currentTelescope)
        generateArgs();

    if (syncR->isEnabled() && (canSync = currentTelescope->canSync()) == false)
    {
        syncR->setEnabled(false);
        slewR->setChecked(true);
        appendLogText(i18n("Telescope does not support syncing."));
    }
}


void Align::syncCCDInfo()
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
        if (np && np->value >0)
            ccd_hor_pixel = ccd_ver_pixel = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np && np->value >0)
            ccd_ver_pixel = np->value;

        np = IUFindNumber(nvp, "CCD_PIXEL_SIZE_Y");
        if (np && np->value >0)
            ccd_ver_pixel = np->value;
    }

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    ISwitchVectorProperty *svp = currentCCD->getBaseDevice()->getSwitch("WCS_CONTROL");
    if (svp)
        setWCSEnabled(Options::solverWCS());

    targetChip->getFrameMinMax(NULL, NULL, NULL, NULL, NULL, &ccd_width, NULL, &ccd_height);
    //targetChip->getFrame(&x,&y,&ccd_width,&ccd_height);
    binningCombo->setEnabled(targetChip->canBin());
    if (targetChip->canBin())
    {
        binningCombo->blockSignals(true);

        int binx=1,biny=1;
        targetChip->getMaxBin(&binx, &biny);
        binningCombo->clear();

        for (int i=0; i < binx; i++)
            binningCombo->addItem(QString("%1x%2").arg(i+1).arg(i+1));

        binningCombo->setCurrentIndex(Options::solverBinningIndex());

        binningCombo->blockSignals(false);
    }

    if (ccd_hor_pixel == -1 || ccd_ver_pixel == -1)
        return;

    if (ccd_hor_pixel != -1 && ccd_ver_pixel != -1 && focal_length != -1 && aperture != -1)
        calculateFOV();

    if (currentCCD && currentTelescope)
        generateArgs();

}


void Align::calculateFOV()
{
    // Calculate FOV
    fov_x = 206264.8062470963552 * ccd_width * ccd_hor_pixel / 1000.0 / focal_length;
    fov_y = 206264.8062470963552 * ccd_height * ccd_ver_pixel / 1000.0 / focal_length;

    fov_x /= 60.0;
    fov_y /= 60.0;

    solverFOV->setSize(fov_x, fov_y);

    FOVOut->setText(QString("%1' x %2'").arg(fov_x, 0, 'g', 3).arg(fov_y, 0, 'g', 3));

}

void Align::generateArgs()
{
    // -O overwrite
    // -3 Expected RA
    // -4 Expected DEC
    // -5 Radius (deg)
    // -L lower scale of image in arcminutes
    // -H upper scale of image in arcmiutes
    // -u aw set scale to be in arcminutes
    // -W solution.wcs name of solution file
    // apog1.jpg name of target file to analyze
    //solve-field -O -3 06:40:51 -4 +09:49:53 -5 1 -L 40 -H 100 -u aw -W solution.wcs apod1.jpg

    double ra=0,dec=0, fov_lower, fov_upper;
    QString ra_dms, dec_dms;
    QString fov_low,fov_high;
    QStringList solver_args;

    // let's stretch the boundaries by 5%
    fov_lower = ((fov_x < fov_y) ? (fov_x *0.95) : (fov_y *0.95));
    fov_upper = ((fov_x > fov_y) ? (fov_x * 1.05) : (fov_y * 1.05));

    currentTelescope->getEqCoords(&ra, &dec);

    fov_low  = QString("%1").arg(fov_lower);
    fov_high = QString("%1").arg(fov_upper);

    getFormattedCoords(ra, dec, ra_dms, dec_dms);

    if (solverOptions->text().isEmpty())
    {
        solver_args << "--no-verify" << "--no-plots" << "--no-fits2fits" << "--resort"
                    << "--downsample" << "2" << "-O" << "-L" << fov_low << "-H" << fov_high << "-u" << "aw";
    }
    else
    {
        solver_args = solverOptions->text().split(" ");
        int fov_low_index = solver_args.indexOf("-L");
        if (fov_low_index != -1)
            solver_args.replace(fov_low_index+1, fov_low);
        int fov_high_index = solver_args.indexOf("-H");
        if (fov_high_index != -1)
            solver_args.replace(fov_high_index+1, fov_high);
    }

    if (raBox->isEmpty() == false && decBox->isEmpty() == false)
    {
        bool raOk(false), decOk(false), radiusOk(false);
        dms ra( raBox->createDms( false, &raOk ) ); //false means expressed in hours
        dms dec( decBox->createDms( true, &decOk ) );
        int radius = 30;
        QString message;

        if ( raOk && decOk )
        {
            //make sure values are in valid range
            if ( ra.Hours() < 0.0 || ra.Hours() > 24.0 )
                message = i18n( "The Right Ascension value must be between 0.0 and 24.0." );
            if ( dec.Degrees() < -90.0 || dec.Degrees() > 90.0 )
                message += '\n' + i18n( "The Declination value must be between -90.0 and 90.0." );
            if ( ! message.isEmpty() )
            {
                KMessageBox::sorry( 0, message, i18n( "Invalid Coordinate Data" ) );
                return;
            }
        }

        if (radiusBox->text().isEmpty() == false)
            radius = radiusBox->text().toInt(&radiusOk);

        if (radiusOk == false)
        {
            KMessageBox::sorry( 0, message, i18n( "Invalid radius value" ) );
            return;
        }

        int ra_index = solver_args.indexOf("-3");
        if (ra_index == -1)
            solver_args << "-3" << QString().setNum(ra.Degrees());
        else
            solver_args.replace(ra_index+1, QString().setNum(ra.Degrees()));

        int de_index = solver_args.indexOf("-4");
        if (de_index == -1)
            solver_args << "-4" << QString().setNum(dec.Degrees());
        else
            solver_args.replace(de_index+1, QString().setNum(dec.Degrees()));

        int rad_index = solver_args.indexOf("-5");
        if (rad_index == -1)
            solver_args << "-5" << QString().setNum(radius);
        else
            solver_args.replace(rad_index+1, QString().setNum(radius));

     }

    solverOptions->setText(solver_args.join(" "));
}

void Align::checkLineEdits()
{
   bool raOk(false), decOk(false);
   raBox->createDms( false, &raOk );
   decBox->createDms( true, &decOk );
   if ( raOk && decOk )
            generateArgs();
}

void Align::copyCoordsToBoxes()
{
    raBox->setText(ScopeRAOut->text());
    decBox->setText(ScopeDecOut->text());

    checkLineEdits();
}

void Align::clearCoordBoxes()
{
    raBox->clear();
    decBox->clear();

    generateArgs();
}

bool Align::captureAndSolve()
{
    //m_isSolverComplete = false;

    if (currentCCD == NULL)
        return false;

    if (parser->init() == false)
        return false;

    if (focal_length == -1 || aperture == -1)
    {
        KMessageBox::error(0, i18n("Telescope aperture and focal length are missing. Please check your driver settings and try again."));
        return false;
    }

    if (ccd_hor_pixel == -1 || ccd_ver_pixel == -1)
    {
        KMessageBox::error(0, i18n("CCD pixel size is missing. Please check your driver settings and try again."));
        return false;
    }

    if (currentFilter != NULL && lockedFilterIndex != -1)
    {
        if (lockedFilterIndex != currentFilterIndex)
        {
            int lockedFilterPosition = lockedFilterIndex + 1;
            filterPositionPending = true;
            currentFilter->runCommand(INDI_SET_FILTER, &lockedFilterPosition);
            return true;
        }
    }

    if (currentCCD->getDriverInfo()->getClientManager()->getBLOBMode(currentCCD->getDeviceName(), "CCD1") == B_NEVER)
    {
        if (KMessageBox::questionYesNo(0, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) == KMessageBox::Yes)
        {
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD1");
            currentCCD->getDriverInfo()->getClientManager()->setBLOBMode(B_ALSO, currentCCD->getDeviceName(), "CCD2");
        }
        else
        {
            return false;
        }
    }

    double seqExpose = exposureIN->value();

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (focusState >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot capture while focus module is busy! Retrying..."));
        QTimer::singleShot(5000, this, SLOT(captureAndSolve()));
        return false;
    }

    if (targetChip->isCapturing())
    {
        appendLogText(i18n("Cannot capture while CCD exposure is in progress! Retrying..."));
        QTimer::singleShot(1000, this, SLOT(captureAndSolve()));
        return false;
    }

    if (currentCCD->isConnected() == false)
    {
        appendLogText(i18n("Error: Lost connection to CCD."));
        KNotification::event( QLatin1String( "AlignFailed"), i18n("Astrometry alignment failed") );
        return false;
    }

   connect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
   connect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this, SLOT(checkCCDExposureProgress(ISD::CCDChip*,double,IPState)));

   // In case of remote solver, we set mode to UPLOAD_BOTH
   if (solverTypeGroup->checkedId() == SOLVER_REMOTE)
   {
       rememberUploadMode = currentCCD->getUploadMode();
       currentCCD->setUploadMode(ISD::CCD::UPLOAD_BOTH);

       // For solver remote we need to start solver BEFORE capture
       startSolving(QString());
   }
   else
   {
       if (currentCCD->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
       {
           rememberUploadMode = ISD::CCD::UPLOAD_LOCAL;
           currentCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
       }
   }

   targetChip->resetFrame();
   targetChip->setBatchMode(false);
   targetChip->setCaptureMode(FITS_ALIGN);
   targetChip->setFrameType(FRAME_LIGHT);

   int bin = Options::solverBinningIndex()+1;
   targetChip->setBinning(bin, bin);

   targetChip->capture(seqExpose);

   Options::setAlignExposure(seqExpose);

   solveB->setEnabled(false);
   stopB->setEnabled(true);
   pi->startAnimation();

   state = ALIGN_PROGRESS;
   emit newStatus(state);

   appendLogText(i18n("Capturing image..."));

   return true;
}

void Align::newFITS(IBLOB *bp)
{
    // Ignore guide head if there is any.
    if (!strcmp(bp->name, "CCD2"))
        return;

    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    disconnect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this, SLOT(checkCCDExposureProgress(ISD::CCDChip*,double,IPState)));

    appendLogText(i18n("Image received."));

    blobType = *(static_cast<ISD::CCD::BlobType*>(bp->aux1));
    blobFileName = QString(static_cast<char*>(bp->aux2));

    if (solverTypeGroup->checkedId() != SOLVER_REMOTE)
    {
        if (blobType == ISD::CCD::BLOB_FITS)
        {
            ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

            if (alignDarkFrameCheck->isChecked())
            {
                int x,y,w,h,binx=1,biny=1;
                targetChip->getFrame(&x,&y,&w,&h);
                targetChip->getBinning(&binx, &biny);

                FITSView *currentImage   = targetChip->getImageView(FITS_ALIGN);
                FITSData *darkData       = NULL;

                uint16_t offsetX = x / binx;
                uint16_t offsetY = y / biny;

                darkData = DarkLibrary::Instance()->getDarkFrame(targetChip, exposureIN->value());

                connect(DarkLibrary::Instance(), SIGNAL(darkFrameCompleted(bool)), this, SLOT(setCaptureComplete()));
                connect(DarkLibrary::Instance(), SIGNAL(newLog(QString)), this, SLOT(appendLogText(QString)));

                if (darkData)
                    DarkLibrary::Instance()->subtract(darkData, currentImage, FITS_NONE, offsetX, offsetY);
                else
                {
                    bool rc = DarkLibrary::Instance()->captureAndSubtract(targetChip, currentImage, exposureIN->value(), offsetX, offsetY);
                    alignDarkFrameCheck->setChecked(rc);
                }

                return;
            }
        }

        setCaptureComplete();
    }
}

void Align::setCaptureComplete()
{
    DarkLibrary::Instance()->disconnect(this);

    if (solverTypeGroup->checkedId() == SOLVER_ONLINE && Options::astrometryUseJPEG())
    {
        ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
        if (targetChip)
        {
            FITSView *view = targetChip->getImageView(FITS_ALIGN);
            if (view)
            {
                QString jpegFile = blobFileName + ".jpg";
                bool rc = view->getDisplayImage()->save(jpegFile, "JPG");
                if (rc)
                    blobFileName = jpegFile;
            }
        }
    }

    startSolving(blobFileName);
}

void Align::setGOTOMode(int mode)
{
    gotoModeButtonGroup->button(mode)->setChecked(true);
    currentGotoMode = static_cast<GotoMode>(mode);
}

void Align::startSolving(const QString &filename, bool isGenerated)
{
    QStringList solverArgs;
    double ra,dec;

    if (isGenerated)
        solverArgs = solverOptions->text().split(" ");
    else if (filename.endsWith("fits") || filename.endsWith("fit"))
    {
        solverArgs = getSolverOptionsFromFITS(filename);
        appendLogText(i18n("Using solver options: %1", solverArgs.join(" ")));
    }
    else
    {
        KGuiItem blindItem(i18n("Blind solver"), QString(), i18n("Blind solver takes a very long time to solve but can reliably solve any image any where in the sky given enough time."));
        KGuiItem existingItem(i18n("Use existing settings"), QString(), i18n("Mount must be pointing close to the target location and current field of view must match the image's field of view."));
        int rc = KMessageBox::questionYesNoCancel(0, i18n("No metadata is available in this image. Do you want to use the blind solver or the existing solver settings?"), i18n("Astrometry solver"),
                                         blindItem, existingItem, KStandardGuiItem::cancel(), "blind_solver_or_existing_solver_option");
        if (rc == KMessageBox::Yes)
            solverArgs << "--no-verify" << "--no-plots" << "--no-fits2fits" << "--resort"  << "--downsample" << "2" << "-O";
        else if (rc == KMessageBox::No)
            solverArgs = solverOptions->text().split(" ");
        else
        {
            abort();
            return;
        }
    }

    currentTelescope->getEqCoords(&ra, &dec);

    if (solverIterations == 0)
    {
        targetCoord.setRA(ra);
        targetCoord.setDec(dec);
    }

    Options::setSolverType(solverTypeGroup->checkedId());
    Options::setSolverOptions(solverOptions->text());
    Options::setSolverOTA(kcfg_solverOTA->isChecked());
    Options::setSolverAccuracyThreshold(accuracySpin->value());
    Options::setAlignDarkFrame(alignDarkFrameCheck->isChecked());
    Options::setSolverGotoOption(currentGotoMode);

    //m_isSolverComplete = false;
    //m_isSolverSuccessful = false;

    parser->verifyIndexFiles(fov_x, fov_y);

    solverTimer.start();

    alignTimer.start();

    if (currentGotoMode == GOTO_SLEW)
        appendLogText(i18n("Solver iteration #%1", solverIterations+1));

    state = ALIGN_PROGRESS;
    emit newStatus(state);

    parser->startSovler(filename, solverArgs, isGenerated);
}

void Align::solverFinished(double orientation, double ra, double dec, double pixscale)
{
    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);

    sOrientation = orientation;
    sRA  = ra;
    sDEC = dec;

    alignTimer.stop();

    int binx, biny;
    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);
    targetChip->getBinning(&binx, &biny);

    if (Options::solverVerbose())
        appendLogText(i18n("Solver RA (%1) DEC (%2) Orientation (%3) Pixel Scale (%4)", QString::number(ra, 'g' , 5), QString::number(dec, 'g' , 5),
                            QString::number(orientation, 'g' , 5), QString::number(pixscale, 'g' , 5)));

    if (pixscale > 0 && loadSlewState == IPS_IDLE)
    {
        double solver_focal_length = (206.264 * ccd_hor_pixel) / pixscale * binx;
        if (fabs(focal_length - solver_focal_length) > 1)
            appendLogText(i18n("Current focal length is %1 mm while computed focal length from the solver is %2 mm. Please update the mount focal length to obtain accurate results.",
                                QString::number(focal_length, 'g' , 5), QString::number(solver_focal_length, 'g' , 5)));
    }

     alignCoord.setRA0(ra/15.0);
     alignCoord.setDec0(dec);
     RotOut->setText(QString::number(orientation, 'g', 5));

     // Convert to JNow
     alignCoord.apparentCoord((long double) J2000, KStars::Instance()->data()->ut().djd());
     // Get horizontal coords
     alignCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

     double raDiff = (alignCoord.ra().Degrees()-targetCoord.ra().Degrees()) * 3600;
     double deDiff = (alignCoord.dec().Degrees()-targetCoord.dec().Degrees()) * 3600;

     emit newSolutionDeviation(raDiff, deDiff);

     targetDiff    = sqrt(raDiff*raDiff + deDiff*deDiff);

     solverFOV->setCenter(alignCoord);
     solverFOV->setRotation(sOrientation);
     solverFOV->setImageDisplay(Options::solverOverlay());

     QString ra_dms, dec_dms;
     getFormattedCoords(alignCoord.ra().Hours(), alignCoord.dec().Degrees(), ra_dms, dec_dms);

     SolverRAOut->setText(ra_dms);
     SolverDecOut->setText(dec_dms);

     if (Options::solverWCS())
     {
         INumberVectorProperty *ccdRotation = currentCCD->getBaseDevice()->getNumber("CCD_ROTATION");
         if (ccdRotation)
         {
             INumber *rotation = IUFindNumber(ccdRotation, "CCD_ROTATION_VALUE");
             if (rotation)
             {
                 ClientManager *clientManager = currentCCD->getDriverInfo()->getClientManager();
                 rotation->value = orientation;
                 clientManager->sendNewNumber(ccdRotation);

                 if (m_wcsSynced == false)
                 {
                     appendLogText(i18n("WCS information updated. Images captured from this point forward shall have valid WCS."));

                     // Just send telescope info in case the CCD driver did not pick up before.
                     INumberVectorProperty *telescopeInfo = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");
                     if (telescopeInfo)
                        clientManager->sendNewNumber(telescopeInfo);

                     m_wcsSynced=true;
                 }
             }
         }
     }

     retries=0;

     appendLogText(i18n("Solution coordinates: RA (%1) DEC (%2) Telescope Coordinates: RA (%3) DEC (%4)", alignCoord.ra().toHMSString(), alignCoord.dec().toDMSString(), telescopeCoord.ra().toHMSString(), telescopeCoord.dec().toDMSString()));
     if (loadSlewState == IPS_IDLE && currentGotoMode == GOTO_SLEW)
     {
        dms diffDeg(targetDiff/3600.0);
        appendLogText(i18n("Target is within %1 degrees of solution coordinates.", diffDeg.toDMSString()));
     }

     if (rememberUploadMode != currentCCD->getUploadMode())
         currentCCD->setUploadMode(rememberUploadMode);

     //if (syncR->isChecked() || nothingR->isChecked() || targetDiff <= accuracySpin->value())
     // CONTINUE HERE

     switch (currentGotoMode)
     {
        case GOTO_SYNC:
         executeGOTO();
         return;
         break;

        case GOTO_SLEW:
         if (loadSlewState == IPS_BUSY || targetDiff > accuracySpin->value())
         {
             if (loadSlewState == IPS_IDLE && ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
             {
                 appendLogText(i18n("Maximum number of iterations reached. Solver failed."));
                 solverFailed();
                 return;
             }

             //executeMode();
             executeGOTO();
             return;
         }

         appendLogText(i18n("Target is within acceptable range. Astrometric solver is successful."));
         break;

        case GOTO_NOTHING:
         break;

     }

     KNotification::event( QLatin1String( "AlignSuccessful"), i18n("Astrometry alignment completed successfully") );
     state = ALIGN_COMPLETE;
     emit newStatus(state);
     solverIterations=0;

     if (azStage > AZ_INIT || altStage > ALT_INIT)
         executePolarAlign();
}

void Align::solverFailed()
{
    KNotification::event( QLatin1String( "AlignFailed"), i18n("Astrometry alignment failed with errors") );

    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);

    alignTimer.stop();

    azStage  = AZ_INIT;
    altStage = ALT_INIT;

    //loadSlewMode = false;
    loadSlewState=IPS_IDLE;
    //m_isSolverComplete = true;
    //m_isSolverSuccessful = false;
    //m_slewToTargetSelected=false;
    solverIterations=0;
    retries=0;

    //emit solverComplete(false);

    state = ALIGN_FAILED;
    emit newStatus(state);
}

void Align::abort()
{
    parser->stopSolver();
    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);

    azStage  = AZ_INIT;
    altStage = ALT_INIT;

    //loadSlewMode = false;
    loadSlewState=IPS_IDLE;
    //m_isSolverComplete = false;
    //m_isSolverSuccessful = false;
    //m_slewToTargetSelected=false;
    solverIterations=0;
    retries=0;
    alignTimer.stop();

    //currentCCD->disconnect(this);
    disconnect(currentCCD, SIGNAL(BLOBUpdated(IBLOB*)), this, SLOT(newFITS(IBLOB*)));
    disconnect(currentCCD, SIGNAL(newExposureValue(ISD::CCDChip*,double,IPState)), this, SLOT(checkCCDExposureProgress(ISD::CCDChip*,double,IPState)));

    if (rememberUploadMode != currentCCD->getUploadMode())
        currentCCD->setUploadMode(rememberUploadMode);

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    // If capture is still in progress, let's stop that.
    if (targetChip->isCapturing())
    {
        targetChip->abortExposure();
        appendLogText(i18n("Capture aborted."));
    }
    else
    {
        int elapsed = (int) round(solverTimer.elapsed()/1000.0);
        appendLogText(i18np("Solver aborted after %1 second.", "Solver aborted after %1 seconds", elapsed));
    }

    state = ALIGN_ABORTED;
    emit newStatus(state);
}

QList<double> Align::getSolutionResult()
{
    QList<double> result;

    result << sOrientation << sRA << sDEC;

    return result;
}

void Align::appendLogText(const QString &text)
{
    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    if (Options::alignmentLogging())
        qDebug() << "Alignment: " << text;

    emit newLog();
}

void Align::clearLog()
{
    logText.clear();
    emit newLog();
}

void Align::processTelescopeNumber(INumberVectorProperty *coord)
{
    QString ra_dms, dec_dms;
    static bool slew_dirty=false;

    if (!strcmp(coord->name, "EQUATORIAL_EOD_COORD"))
    {
        getFormattedCoords(coord->np[0].value, coord->np[1].value, ra_dms, dec_dms);

        telescopeCoord.setRA(coord->np[0].value);
        telescopeCoord.setDec(coord->np[1].value);
        telescopeCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

        ScopeRAOut->setText(ra_dms);
        ScopeDecOut->setText(dec_dms);

        switch (coord->s)
        {
        case IPS_OK:
        {
            // Update the boxes as the mount just finished slewing
            if (slew_dirty && Options::solverUpdateCoords())
                copyCoordsToBoxes();

            switch (state)
            {
            case ALIGN_PROGRESS:
                break;

            case ALIGN_SYNCING:
            {
                slew_dirty = false;
                if (currentGotoMode == GOTO_SLEW)
                {
                    Slew();
                    return;
                }
                else
                {
                    appendLogText(i18n("Mount is synced to solution coordinates. Astrometric solver is successful."));
                    KNotification::event( QLatin1String( "AlignSuccessful"), i18n("Astrometry alignment completed successfully") );
                    state = ALIGN_COMPLETE;
                    emit newStatus(state);
                }
            }
            break;

            case ALIGN_SLEWING:
                if (slew_dirty == false)
                    break;

                slew_dirty = false;
                if (loadSlewState == IPS_BUSY)
                {
                    loadSlewState = IPS_IDLE;

                    if (Options::alignmentLogging())
                        qDebug() << "Alignment: loadSlewState is IDLE.";

                    state = ALIGN_PROGRESS;
                    emit newStatus(state);

                    QTimer::singleShot(delaySpin->value(), this, SLOT(captureAndSolve()));
                    return;
                }
                else if(currentGotoMode == GOTO_SLEW)
                {
                    appendLogText(i18n("Slew complete. Target accuracy is not met, running solver again..."));

                    state = ALIGN_PROGRESS;
                    emit newStatus(state);

                    QTimer::singleShot(delaySpin->value(), this, SLOT(captureAndSolve()));
                    return;
                }
                break;

            default:
            {
                slew_dirty = false;
            }
            break;
            }
        }
        break;

        case IPS_BUSY:
        {
            slew_dirty = true;

        }
        break;

        case IPS_ALERT:
        {
            if (state == ALIGN_SYNCING || state == ALIGN_SLEWING)
            {
                if (state == ALIGN_SYNCING)
                    appendLogText(i18n("Syncing failed!"));
                else
                    appendLogText(i18n("Slewing failed!"));

                if (++retries == 3)
                {
                    abort();
                    return;
                }
                else
                {
                    if (currentGotoMode == GOTO_SLEW)
                        Slew();
                    else
                        Sync();
                }
            }

            return;
        }
        break;

        default:
        break;
        }


        /*if (Options::alignmentLogging())
            qDebug() << "Alignment: State is " << Ekos::getAlignStatusString(state) << " isSlewing? " << currentTelescope->isSlewing() << " slew Dirty? " << slew_dirty
                     << " Current GOTO Mode? " << currentGotoMode << " LoadSlewState? " << pstateStr(loadSlewState);*/

        switch (azStage)
        {
        case AZ_SYNCING:
            if (currentTelescope->isSlewing())
                azStage=AZ_SLEWING;
            break;

        case AZ_SLEWING:
            if (currentTelescope->isSlewing() == false)
            {
                azStage = AZ_SECOND_TARGET;
                measureAzError();
            }
            break;

        case AZ_CORRECTING:
            if (currentTelescope->isSlewing() == false)
            {
                appendLogText(i18n("Slew complete. Please adjust azimuth knob until the target is in the center of the view."));
                azStage = AZ_INIT;
            }
            break;

        default:
            break;
        }

        switch (altStage)
        {
        case ALT_SYNCING:
            if (currentTelescope->isSlewing())
                altStage = ALT_SLEWING;
            break;

        case ALT_SLEWING:
            if (currentTelescope->isSlewing() == false)
            {
                altStage = ALT_SECOND_TARGET;
                measureAltError();
            }
            break;

        case ALT_CORRECTING:
            if (currentTelescope->isSlewing() == false)
            {
                appendLogText(i18n("Slew complete. Please adjust altitude knob until the target is in the center of the view."));
                altStage = ALT_INIT;
            }
            break;
        default:
            break;
        }

    }


    if (!strcmp(coord->name, "TELESCOPE_INFO"))
        syncTelescopeInfo();

}

void Align::executeGOTO()
{
    if (loadSlewState == IPS_BUSY)
    {
        //if (loadSlewIterations == loadSlewIterationsSpin->value())
            //loadSlewCoord = alignCoord;

        //targetCoord = loadSlewCoord;
        targetCoord = alignCoord;
        SlewToTarget();
    }
    else if (currentGotoMode == GOTO_SYNC)
        Sync();
    else if (currentGotoMode == GOTO_SLEW)
        SlewToTarget();
}

void Align::Sync()
{
    state = ALIGN_SYNCING;

    if (currentTelescope->Sync(&alignCoord))    
    {
        emit newStatus(state);
        appendLogText(i18n("Syncing to RA (%1) DEC (%2)", alignCoord.ra().toHMSString(), alignCoord.dec().toDMSString()));
    }
    else
    {
        state = ALIGN_IDLE;
        emit newStatus(state);
        appendLogText(i18n("Syncing failed."));
    }
}

void Align::Slew()
{
    state = ALIGN_SLEWING;
    emit newStatus(state);

    currentTelescope->Slew(&targetCoord);

    appendLogText(i18n("Slewing to target coordinates: RA (%1) DEC (%2).", targetCoord.ra().toHMSString(), targetCoord.dec().toDMSString()));
}

void Align::SlewToTarget()
{
    if (canSync && loadSlewState == IPS_IDLE)
    {
        Sync();
        return;
    }

    Slew();
}

void Align::executePolarAlign()
{
    appendLogText(i18n("Processing solution for polar alignment..."));

    switch (azStage)
    {
        case AZ_FIRST_TARGET:
        case AZ_FINISHED:
            measureAzError();
            break;

        default:
            break;
    }

    switch (altStage)
    {
        case ALT_FIRST_TARGET:
        case ALT_FINISHED:
            measureAltError();
            break;

        default:
            break;
    }
}


void Align::measureAzError()
{
    static double initRA=0, initDEC=0, finalRA=0, finalDEC=0, initAz=0;
    int hemisphere = KStarsData::Instance()->geo()->lat()->Degrees() > 0 ? 0 : 1;

    if (Options::alignmentLogging())
        qDebug() << "Polar Alignment: Measureing Azimuth Error...";

    switch (azStage)
    {
        case AZ_INIT:

        // Display message box confirming user point scope near meridian and south

        if (KMessageBox::warningContinueCancel( 0, hemisphere == 0
                                                   ? i18n("Point the telescope at the southern meridian. Press continue when ready.")
                                                   : i18n("Point the telescope at the northern meridian. Press continue when ready.")
                                                , i18n("Polar Alignment Measurement"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                                                "ekos_measure_az_error")!=KMessageBox::Continue)
            return;

        appendLogText(i18n("Solving first frame near the meridian."));
        azStage = AZ_FIRST_TARGET;
        solveB->click();
        break;

      case AZ_FIRST_TARGET:
        // start solving there, find RA/DEC
        initRA   = alignCoord.ra().Degrees();
        initDEC  = alignCoord.dec().Degrees();
        initAz   = alignCoord.az().Degrees();

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: initRA " << alignCoord.ra().toHMSString() << " initDEC " << alignCoord.dec().toDMSString() <<
                        " initlAz " << alignCoord.az().toDMSString() << " initAlt " << alignCoord.alt().toDMSString();

        // Now move 30 arcminutes in RA
        if (canSync)
        {
            azStage = AZ_SYNCING;
            currentTelescope->Sync(initRA/15.0, initDEC);
            currentTelescope->Slew((initRA - RAMotion)/15.0, initDEC);
        }
        // If telescope doesn't sync, we slew relative to its current coordinates
        else
        {
            azStage = AZ_SLEWING;
            currentTelescope->Slew(telescopeCoord.ra().Hours() - RAMotion/15.0, telescopeCoord.dec().Degrees());
        }

        appendLogText(i18n("Slewing 30 arcminutes in RA..."));
        break;

      case AZ_SECOND_TARGET:
        // We reached second target now
        // Let now solver for RA/DEC
        appendLogText(i18n("Solving second frame near the meridian."));
        azStage = AZ_FINISHED;
        solveB->click();
        break;


      case AZ_FINISHED:
        // Measure deviation in DEC
        // Call function to report error
        // set stage to AZ_FIRST_TARGET again
        appendLogText(i18n("Calculating azimuth alignment error..."));
        finalRA   = alignCoord.ra().Degrees();
        finalDEC  = alignCoord.dec().Degrees();

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: finalRA " << alignCoord.ra().toHMSString() << " finalDEC " << alignCoord.dec().toDMSString() <<
                        " finalAz " << alignCoord.az().toDMSString() << " finalAlt " << alignCoord.alt().toDMSString();

        // Slew back to original position
        if (canSync)
            currentTelescope->Slew(initRA/15.0, initDEC);
        else
        {
            currentTelescope->Slew(telescopeCoord.ra().Hours() + RAMotion/15.0, telescopeCoord.dec().Degrees());
        }

        appendLogText(i18n("Slewing back to original position..."));

        calculatePolarError(initRA, initDEC, finalRA, finalDEC, initAz);

        azStage = AZ_INIT;
        break;

    default:
        break;

    }

}

void Align::measureAltError()
{
    static double initRA=0, initDEC=0, finalRA=0, finalDEC=0, initAz=0;

    if (Options::alignmentLogging())
        qDebug() << "Polar Alignment: Measureing Altitude Error...";

    switch (altStage)
    {
        case ALT_INIT:

        // Display message box confirming user point scope near meridian and south

        if (KMessageBox::warningContinueCancel( 0, i18n("Point the telescope to the eastern or western horizon with a minimum altitude of 20 degrees. Press continue when ready.")
                                                , i18n("Polar Alignment Measurement"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                                                "ekos_measure_alt_error")!=KMessageBox::Continue)
            return;

        appendLogText(i18n("Solving first frame."));
        altStage = ALT_FIRST_TARGET;
        solveB->click();
        break;

      case ALT_FIRST_TARGET:
        // start solving there, find RA/DEC
        initRA   = alignCoord.ra().Degrees();
        initDEC  = alignCoord.dec().Degrees();
        initAz   = alignCoord.az().Degrees();

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: initRA " << alignCoord.ra().toHMSString() << " initDEC " << alignCoord.dec().toDMSString() <<
                        " initlAz " << alignCoord.az().toDMSString() << " initAlt " << alignCoord.alt().toDMSString();

        // Now move 30 arcminutes in RA
        if (canSync)
        {
            altStage = ALT_SYNCING;
            currentTelescope->Sync(initRA/15.0, initDEC);
            currentTelescope->Slew((initRA - RAMotion)/15.0, initDEC);

        }
        // If telescope doesn't sync, we slew relative to its current coordinates
        else
        {
            altStage = ALT_SLEWING;
            currentTelescope->Slew(telescopeCoord.ra().Hours() - RAMotion/15.0, telescopeCoord.dec().Degrees());
        }


        appendLogText(i18n("Slewing 30 arcminutes in RA..."));
        break;

      case ALT_SECOND_TARGET:
        // We reached second target now
        // Let now solver for RA/DEC
        appendLogText(i18n("Solving second frame."));
        altStage = ALT_FINISHED;
        solveB->click();
        break;


      case ALT_FINISHED:
        // Measure deviation in DEC
        // Call function to report error
        appendLogText(i18n("Calculating altitude alignment error..."));
        finalRA   = alignCoord.ra().Degrees();
        finalDEC  = alignCoord.dec().Degrees();

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: finalRA " << alignCoord.ra().toHMSString() << " finalDEC " << alignCoord.dec().toDMSString() <<
                        " finalAz " << alignCoord.az().toDMSString() << " finalAlt " << alignCoord.alt().toDMSString();

        // Slew back to original position
        if (canSync)
            currentTelescope->Slew(initRA/15.0, initDEC);
        // If telescope doesn't sync, we slew relative to its current coordinates
        else
        {
            currentTelescope->Slew(telescopeCoord.ra().Hours() + RAMotion/15.0, telescopeCoord.dec().Degrees());
        }

        appendLogText(i18n("Slewing back to original position..."));

        calculatePolarError(initRA, initDEC, finalRA, finalDEC, initAz);

        altStage = ALT_INIT;
        break;

    default:
        break;

    }

}

void Align::calculatePolarError(double initRA, double initDEC, double finalRA, double finalDEC, double initAz)
{
    double raMotion = finalRA - initRA;
    decDeviation = finalDEC - initDEC;

    // Northern/Southern hemisphere
    int hemisphere = KStarsData::Instance()->geo()->lat()->Degrees() > 0 ? 0 : 1;
    // East/West of meridian
    int horizon    = (initAz > 0 && initAz <= 180) ? 0 : 1;

    // How much time passed siderrally form initRA to finalRA?
    //double RATime = fabs(raMotion / SIDRATE) / 60.0;

    // 2016-03-30: Diff in RA is sufficient for time difference
    // raMotion in degrees. RATime in minutes.
    double RATime = fabs(raMotion) * 60.0;

    // Equation by Frank Berret (Measuring Polar Axis Alignment Error, page 4)
    // In degrees
    double deviation = (3.81 * (decDeviation * 3600) ) / ( RATime * cos(initDEC * dms::DegToRad)) / 60.0;
    dms devDMS(fabs(deviation));

    KLocalizedString deviationDirection;

    switch (hemisphere)
    {
        // Northern hemisphere
        case 0:
        if (azStage == AZ_FINISHED)
        {
            if (decDeviation > 0)
                deviationDirection = ki18n("%1 too far east");
            else
                deviationDirection = ki18n("%1 too far west");
        }
        else if (altStage == ALT_FINISHED)
        {
            switch (horizon)
            {
                // East
                case 0:
                if (decDeviation > 0)
                    deviationDirection = ki18n("%1 too far high");
                else
                    deviationDirection = ki18n("%1 too far low");

                break;

                // West
                case 1:
                if (decDeviation > 0)
                    deviationDirection = ki18n("%1 too far low");
                else
                    deviationDirection = ki18n("%1 too far high");
                break;

                default:
                break;
            }
        }
        break;

        // Southern hemisphere
        case 1:
        if (azStage == AZ_FINISHED)
        {
            if (decDeviation > 0)
                deviationDirection = ki18n("%1 too far west");
            else
                deviationDirection = ki18n("%1 too far east");
        }
        else if (altStage == ALT_FINISHED)
        {
            switch (horizon)
            {
                // East
                case 0:
                if (decDeviation > 0)
                    deviationDirection = ki18n("%1 too far low");
                else
                    deviationDirection = ki18n("%1 too far high");
                break;

                // West
                case 1:
                if (decDeviation > 0)
                    deviationDirection = ki18n("%1 too far high");
                else
                    deviationDirection = ki18n("%1 too far low");
                break;

                default:
                break;
            }
        }
        break;

       default:
        break;

    }

    if (Options::alignmentLogging())
    {
        qDebug() << "Polar Alignment: Hemisphere is " << ((hemisphere == 0) ? "North" : "South") << " --- initAz " << initAz;
        qDebug() << "Polar Alignment: initRA " << initRA << " initDEC " << initDEC << " finalRA " << finalRA << " finalDEC " << finalDEC;
        qDebug() << "Polar Alignment: decDeviation " << decDeviation*3600 << " arcsec " << " RATime " << RATime << " minutes";
        qDebug() << "Polar Alignment: Raw Deviaiton " << deviation << " degrees.";
    }

    if (azStage == AZ_FINISHED)
    {
        azError->setText(deviationDirection.subs(QString("%1").arg(devDMS.toDMSString())).toString());
        //azError->setText(deviationDirection.subs(QString("%1")azDMS.toDMSString());
        azDeviation = deviation * (decDeviation > 0 ? 1 : -1);

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: Azimuth Deviation " << azDeviation << " degrees.";

        correctAzB->setEnabled(true);
    }
    if (altStage == ALT_FINISHED)
    {
        //altError->setText(deviationDirection.subs(QString("%1").arg(fabs(deviation), 0, 'g', 3)).toString());
        altError->setText(deviationDirection.subs(QString("%1").arg(devDMS.toDMSString())).toString());
        altDeviation = deviation * (decDeviation > 0 ? 1 : -1);

        if (Options::alignmentLogging())
            qDebug() << "Polar Alignment: Altitude Deviation " << altDeviation << " degrees.";

        correctAltB->setEnabled(true);
    }
}

void Align::correctAltError()
{
    double newRA, newDEC;

    SkyPoint currentCoord (telescopeCoord);
    dms      targetLat;

    if (Options::alignmentLogging())
    {
        qDebug() << "Polar Alignment: Correcting Altitude Error...";
        qDebug() << "Polar Alignment: Current Mount RA " << currentCoord.ra().toHMSString() << " DEC " << currentCoord.dec().toDMSString() <<
                    "Az " << currentCoord.az().toDMSString() << " Alt " << currentCoord.alt().toDMSString();
    }

    // An error in polar alignment altitude reflects a deviation in the latitude of the mount from actual latitude of the site
    // Calculating the latitude accounting for the altitude deviation. This is the latitude at which the altitude deviation should be zero.
    targetLat.setD(KStars::Instance()->data()->geo()->lat()->Degrees() + altDeviation);

    // Calculate the Az/Alt of the mount if it were located at the corrected latitude
    currentCoord.EquatorialToHorizontal(KStars::Instance()->data()->lst(), &targetLat );

    // Convert corrected Az/Alt to RA/DEC given the local sideral time and current (not corrected) latitude
    currentCoord.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

    // New RA/DEC should reflect the position in the sky at which the polar alignment altitude error is minimal.
    newRA  = currentCoord.ra().Hours();
    newDEC = currentCoord.dec().Degrees();

    altStage = ALT_CORRECTING;

    if (Options::alignmentLogging())
    {
        qDebug() << "Polar Alignment: Target Latitude = Latitude " << KStars::Instance()->data()->geo()->lat()->Degrees() << " + Altitude Deviation " << altDeviation << " = " << targetLat.Degrees();
        qDebug() << "Polar Alignment: Slewing to calibration position...";
    }

    currentTelescope->Slew(newRA, newDEC);

    appendLogText(i18n("Slewing to calibration position, please wait until telescope completes slewing."));
}

void Align::correctAzError()
{
    double newRA, newDEC, currentAlt, currentAz;

    SkyPoint currentCoord (telescopeCoord);

    if (Options::alignmentLogging())
    {
        qDebug() << "Polar Alignment: Correcting Azimuth Error...";
        qDebug() << "Polar Alignment: Current Mount RA " << currentCoord.ra().toHMSString() << " DEC " << currentCoord.dec().toDMSString() <<
                    "Az " << currentCoord.az().toDMSString() << " Alt " << currentCoord.alt().toDMSString();
        qDebug() << "Polar Alignment: Target Azimuth = Current Azimuth " << currentCoord.az().Degrees() << " + Azimuth Deviation " << azDeviation << " = " << currentCoord.az().Degrees() + azDeviation;
    }

    // Get current horizontal coordinates of the mount
    currentCoord.EquatorialToHorizontal(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

    // Keep Altitude as it is and change Azimuth to account for the azimuth deviation
    // The new sky position should be where the polar alignment azimuth error is minimal
    currentAlt = currentCoord.alt().Degrees();
    currentAz  = currentCoord.az().Degrees() + azDeviation;

    // Update current Alt and Azimuth to new values
    currentCoord.setAlt(currentAlt);
    currentCoord.setAz(currentAz);

    // Convert Alt/Az back to equatorial coordinates
    currentCoord.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());

    // Get new RA and DEC
    newRA  = currentCoord.ra().Hours();
    newDEC = currentCoord.dec().Degrees();

    azStage = AZ_CORRECTING;

    if (Options::alignmentLogging())
        qDebug() << "Polar Alignment: Slewing to calibration position...";

    currentTelescope->Slew(newRA, newDEC);

    appendLogText(i18n("Slewing to calibration position, please wait until telescope completes slewing."));

}

void Align::getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str)
{
    dms ra_s,dec_s;
    ra_s.setH(ra);
    dec_s.setD(dec);

    ra_str = QString("%1:%2:%3").arg(ra_s.hour(), 2, 10, QChar('0')).arg(ra_s.minute(), 2, 10, QChar('0')).arg(ra_s.second(), 2, 10, QChar('0'));
    if (dec_s.Degrees() < 0)
        dec_str = QString("-%1:%2:%3").arg(abs(dec_s.degree()), 2, 10, QChar('0')).arg(abs(dec_s.arcmin()), 2, 10, QChar('0')).arg(dec_s.arcsec(), 2, 10, QChar('0'));
    else
        dec_str = QString("%1:%2:%3").arg(dec_s.degree(), 2, 10, QChar('0')).arg(dec_s.arcmin(), 2, 10, QChar('0')).arg(dec_s.arcsec(), 2, 10, QChar('0'));
}

void Align::loadAndSlew(QString fileURL)
{
    if (fileURL.isEmpty())
        fileURL = QFileDialog::getOpenFileName(KStars::Instance(), i18n("Load Image"), dirPath, "Images (*.fits *.fit *.jpg *.jpeg)");

    if (fileURL.isEmpty())
        return;

    QFileInfo fileInfo(fileURL);
    dirPath = fileInfo.absolutePath();

    //loadSlewMode = true;
    loadSlewState=IPS_BUSY;

    slewR->setChecked(true);
    currentGotoMode = GOTO_SLEW;

    solveB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    startSolving(fileURL, false);
}

void Align::setExposure(double value)
{
    exposureIN->setValue(value);
}

void Align::setBinningIndex(int binIndex)
{
   Options::setSolverBinningIndex(binIndex);

   // If sender is not our combo box, then we need to update the combobox itself
   if ( dynamic_cast<QComboBox*>(sender()) != binningCombo)
   {
       binningCombo->blockSignals(true);
       binningCombo->setCurrentIndex(binIndex);
       binningCombo->blockSignals(false);
   }
}

void Align::setSolverArguments(const QString & value)
{
    solverOptions->setText(value);
}

void Align::setSolverSearchOptions(double ra, double dec, double radius)
{
    dms RA, DEC;
    RA.setH(ra);
    DEC.setD(dec);

    raBox->setText(RA.toHMSString());
    decBox->setText(DEC.toDMSString());
    radiusBox->setText(QString::number(radius));
}

void Align::setUseOAGT(bool enabled)
{
    kcfg_solverOTA->setChecked(enabled);
}

FOV* Align::fov()
{
    if (sOrientation == -1)
        return NULL;
    else
        return solverFOV;

}

void Align::setLockedFilter(ISD::GDInterface *filter, int lockedPosition)
{
    currentFilter = filter;
    if (currentFilter)
    {
        lockedFilterIndex = lockedPosition;

        INumberVectorProperty *filterSlot = filter->getBaseDevice()->getNumber("FILTER_SLOT");
        if (filterSlot)
            currentFilterIndex = filterSlot->np[0].value-1;

        connect(currentFilter, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processFilterNumber(INumberVectorProperty*)), Qt::UniqueConnection);
    }
}

void Align::processFilterNumber(INumberVectorProperty *nvp)
{
    if (currentFilter && !strcmp(nvp->name, "FILTER_SLOT") && !strcmp(nvp->device, currentFilter->getDeviceName()))
    {
        currentFilterIndex = nvp->np[0].value - 1;

        if (filterPositionPending)
        {
            if (currentFilterIndex == lockedFilterIndex)
            {
                filterPositionPending = false;
                captureAndSolve();
            }
        }
    }
}

void Align::setWCSEnabled(bool enable)
{
    if (currentCCD == NULL)
        return;

    ISwitchVectorProperty *wcsControl = currentCCD->getBaseDevice()->getSwitch("WCS_CONTROL");

    ISwitch *wcs_enable  = IUFindSwitch(wcsControl, "WCS_ENABLE");
    ISwitch *wcs_disable = IUFindSwitch(wcsControl, "WCS_DISABLE");

    if (!wcs_enable || !wcs_disable)
        return;

    if ( (wcs_enable->s == ISS_ON && enable) || (wcs_disable->s == ISS_ON && !enable) )
        return;

    IUResetSwitch(wcsControl);
    if (enable)
    {
        appendLogText(i18n("World Coordinate System (WCS) is enabled. CCD rotation must be set either manually in the CCD driver or by solving an image before proceeding to capture any further images, otherwise the WCS information may be invalid."));
        wcs_enable->s  = ISS_ON;
    }
    else
    {
        wcs_disable->s = ISS_ON;
        m_wcsSynced=false;
        appendLogText(i18n("World Coordinate System (WCS) is disabled."));
    }

    ClientManager *clientManager = currentCCD->getDriverInfo()->getClientManager();

    clientManager->sendNewSwitch(wcsControl);
}

void Align::checkCCDExposureProgress(ISD::CCDChip *targetChip, double remaining, IPState state)
{
    INDI_UNUSED(targetChip);
    INDI_UNUSED(remaining);

    if (state == IPS_ALERT)
    {
        if (++retries == 3)
        {
            appendLogText(i18n("Capture error! Aborting..."));

            abort();
            return;
        }

        appendLogText(i18n("Restarting capture attempt #%1", retries));
        captureAndSolve();
    }
}

void Align::setFocusStatus(Ekos::FocusState state)
{
    focusState = state;
}

QStringList Align::getSolverOptionsFromFITS(const QString &filename)
{
    int status=0, fits_ccd_width, fits_ccd_height, fits_focal_length=-1, fits_binx=1, fits_biny=1;
    char comment[128], error_status[512];
    fitsfile * fptr = NULL;
    double ra=0,dec=0, fits_fov_x, fits_fov_y, fov_lower, fov_upper, fits_ccd_hor_pixel=-1, fits_ccd_ver_pixel=-1;
    QString fov_low,fov_high;
    QStringList solver_args;

    // Default arguments
    solver_args << "--no-verify" << "--no-plots" << "--no-fits2fits" << "--resort"  << "--downsample" << "2" << "-O";

    if (fits_open_image(&fptr, filename.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        qWarning() << "Could not open file " << filename << "  Error: " << QString::fromUtf8(error_status);
        return solver_args;
    }

    if (fits_read_key(fptr, TINT, "NAXIS1", &fits_ccd_width, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: Cannot find NAXIS1."));
        return solver_args;
    }

    if (fits_read_key(fptr, TINT, "NAXIS2", &fits_ccd_height, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: Cannot find NAXIS2."));
        return solver_args;
    }

    bool coord_ok = true;

    if (fits_read_key(fptr, TDOUBLE, "OBJCTRA", &ra, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        coord_ok=false;
        appendLogText(i18n("FITS header: Cannot find OBJCTRA. Using current mount coordinates."));
        //return solver_args;
    }

    if (coord_ok && fits_read_key(fptr, TDOUBLE, "OBJCTDEC", &dec, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        coord_ok=false;
        appendLogText(i18n("FITS header: Cannot find OBJCTDEC. Using current mount coordinates."));
        //return solver_args;
    }

    if (coord_ok == false)
    {
        ra  = telescopeCoord.ra0().Hours();
        dec = telescopeCoord.dec0().Degrees();
    }

    solver_args << "-3" << QString::number(ra*15.0) << "-4" << QString::number(dec) << "-5 15";

    if (fits_read_key(fptr, TINT, "FOCALLEN", &fits_focal_length, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: Cannot find FOCALLEN."));
        return solver_args;
    }

    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE1", &fits_ccd_hor_pixel, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: Cannot find PIXSIZE1."));
        return solver_args;
    }

    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE2", &fits_ccd_ver_pixel, comment, &status ))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        appendLogText(i18n("FITS header: Cannot find PIXSIZE2."));
        return solver_args;
    }

    fits_read_key(fptr, TINT, "XBINNING", &fits_binx, comment, &status );
    fits_read_key(fptr, TINT, "YBINNING", &fits_biny, comment, &status );

    // Calculate FOV
    fits_fov_x = 206264.8062470963552 * fits_ccd_width * fits_ccd_hor_pixel / 1000.0 / fits_focal_length * fits_binx;
    fits_fov_y = 206264.8062470963552 * fits_ccd_height * fits_ccd_ver_pixel / 1000.0 / fits_focal_length* fits_biny;

    fits_fov_x /= 60.0;
    fits_fov_y /= 60.0;

    // let's stretch the boundaries by 5%
    fov_lower = ((fits_fov_x < fits_fov_y) ? (fits_fov_x *0.95) : (fits_fov_y *0.95));
    fov_upper = ((fits_fov_x > fits_fov_y) ? (fits_fov_x * 1.05) : (fits_fov_y * 1.05));

    fov_low  = QString::number(fov_lower);
    fov_high = QString::number(fov_upper);

    solver_args << "-L" << fov_low << "-H" << fov_high << "-u" << "aw";


    return solver_args;
}

void Align::saveSettleTime()
{
    Options::setSettlingTime(delaySpin->value());
}

void Align::setCaptureStatus(CaptureState newState)
{
    switch (newState)
    {
    case CAPTURE_ALIGNING:
        QTimer::singleShot(Options::settlingTime(), this, SLOT(captureAndSolve()));
        break;

    default:
        break;
    }
}

}


