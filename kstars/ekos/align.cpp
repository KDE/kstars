/*  Ekos Polar Alignment Tool
    Copyright (C) 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <unistd.h>
#include <config-kstars.h>
#include <QProcess>

#include "kstars.h"
#include "kstarsdata.h"
#include "align.h"
#include "Options.h"

#ifdef HAVE_WCSLIB
#include <wcshdr.h>
#include <wcsfix.h>
#include <wcs.h>
#include <getwcstab.h>
#endif

#include <KMessageBox>

#include "QProgressIndicator.h"
#include "indi/driverinfo.h"
#include "indi/indicommon.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsview.h"

#include <basedevice.h>

namespace Ekos
{

Align::Align()
{
    setupUi(this);

    currentCCD     = NULL;
    currentTelescope = NULL;
    ccd_hor_pixel =  ccd_ver_pixel =  focal_length =  aperture = -1;
    useGuideHead = false;

    connect(solveB, SIGNAL(clicked()), this, SLOT(capture()));
    connect(stopB, SIGNAL(clicked()), this, SLOT(stopSolving()));

    connect(CCDCaptureCombo, SIGNAL(activated(int)), this, SLOT(checkCCD(int)));

    appendLogText(i18n("Idle."));

    pi = new QProgressIndicator(this);

    controlLayout->addWidget(pi, 0, 2, 1, 1);

    exposureSpin->setValue(Options::alignExposure());
}

Align::~Align()
{
    delete(pi);
}


void Align::checkCCD(int ccdNum)
{
    if (ccdNum <= CCDs.count())
        currentCCD = CCDs.at(ccdNum);

    syncCCDInfo();

}

void Align::setCCD(ISD::GDInterface *newCCD)
{
    CCDCaptureCombo->addItem(newCCD->getDeviceName());

    CCDs.append(static_cast<ISD::CCD *>(newCCD));

    checkCCD(0);

    CCDCaptureCombo->setCurrentIndex(0);
}

void Align::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = (ISD::Telescope*) newTelescope;

    connect(currentTelescope, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(updateScopeCoords(INumberVectorProperty*)));

    syncTelescopeInfo();
}

void Align::syncTelescopeInfo()
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
        calculateFOV();

    if (currentCCD && currentTelescope)
        generateArgs();
}


void Align::syncCCDInfo()
{
    INumberVectorProperty * nvp = NULL;
    int x,y;

    if (currentCCD == NULL)
        return;

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

    ISD::CCDChip *targetChip = currentCCD->getChip(useGuideHead ? ISD::CCDChip::GUIDE_CCD : ISD::CCDChip::PRIMARY_CCD);

    if (targetChip->getFrame(&x,&y,&ccd_width,&ccd_height))

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

    FOVOut->setText(QString("%1' x %2'").arg(fov_x, 0, 'g', 2).arg(fov_y, 0, 'g', 2));

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

    // let's strech the boundaries by 5%
    fov_lower = ((fov_x < fov_y) ? (fov_x *0.95) : (fov_y *0.95));
    fov_upper = ((fov_x > fov_y) ? (fov_x * 1.05) : (fov_y * 1.05));

    currentTelescope->getEqCoords(&ra, &dec);

    fov_low  = QString("%1").arg(fov_lower);
    fov_high = QString("%1").arg(fov_upper);

    getFormattedCoords(ra, dec, ra_dms, dec_dms);

    solver_args << "--no-verify" << "--no-plots" << "--no-fits2fits" << "--resort" << "--depth" << "20,30,40"
                                << "-O" << "-3" << ra_dms << "-4" << dec_dms << "-5" << "2"
                                << "-L" << fov_low << "-H" << fov_high
                                << "-u" << "aw";

    solverOptions->setText(solver_args.join(" "));
}

bool Align::capture()
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

   targetChip->setCaptureMode(FITS_WCSM);

   targetChip->setFrameType(ccdFrame);

   targetChip->capture(seqExpose);

   Options::setAlignExposure(seqExpose);

   appendLogText(i18n("Capturing image..."));

   return true;
}

void Align::newFITS(IBLOB *bp)
{
    // Ignore guide head if there is any.
    if (!strcmp(bp->name, "CCD2"))
        return;

    currentCCD->disconnect(this);

    appendLogText(i18n("Image received."));

    startSovling(QString(bp->label));
}

void Align::startSovling(const QString &filename)
{
    QStringList solverArgs;
    double ra,dec;

    fitsFile = filename;

    //qDebug() << "filename " << filename << endl;

    currentTelescope->getEqCoords(&ra, &dec);

    targetCoord.setRA(ra);
    targetCoord.setDec(dec);

    QString command = "solve-field";

    solverArgs = solverOptions->text().split(" ");
    solverArgs << "-W" << "/tmp/solution.wcs" << filename;

    qDebug() << solverArgs.join(" ") << endl;

    connect(&solver, SIGNAL(finished(int)), this, SLOT(solverComplete(int)));
    connect(&solver, SIGNAL(readyReadStandardOutput()), this, SLOT(logSolver()));

    solveB->setEnabled(false);
    stopB->setEnabled(true);

    pi->startAnimation();

    solverTimer.start();

    solver.start(command, solverArgs);

    appendLogText(i18n("Starting solver..."));

}

void Align::stopSolving()
{
    solver.terminate();

    pi->stopAnimation();

    int elapsed = (int) round(solverTimer.elapsed()/1000.0);

    appendLogText(i18np("Solver aborted after %1 second.", "Solver aborted after %1 seconds", elapsed));

}

void Align::solverComplete(int exist_status)
{
    pi->stopAnimation();
    solver.disconnect();

    stopB->setEnabled(false);
    solveB->setEnabled(true);

    if (exist_status != 0 || access("/tmp/solution.wcs", F_OK)==-1)
    {
        appendLogText(i18n("Solver failed. Try again."));
        return;
    }

    connect(&wcsinfo, SIGNAL(finished(int)), this, SLOT(wcsinfoComplete(int)));

    wcsinfo.start("wcsinfo", QStringList("/tmp/solution.wcs"));

}

void Align::wcsinfoComplete(int exist_status)
{
    if (exist_status != 0)
    {
        appendLogText(i18n("WCS header missing or corrupted. Solver failed."));
        return;
    }

    QString wcsinfo_stdout = wcsinfo.readAllStandardOutput();

    QStringList wcskeys = wcsinfo_stdout.split(QRegExp("[\n]"));

    QStringList key_value;

    foreach(QString key, wcskeys)
    {
        key_value = key.split(" ");

        if (key_value.size() > 1)
        {
            if (key_value[0] == "ra_center")
                alignCoord.setRA0(key_value[1].toDouble()/15.0);
            else if (key_value[0] == "dec_center")
                alignCoord.setDec0(key_value[1].toDouble());
            else if (key_value[0] == "orientation_center")
                RotOut->setText(key_value[1]);
        }

    }

    // Convert to JNow
    alignCoord.apparentCoord((long double) J2000, KStars::Instance()->data()->ut().djd());

    QString ra_dms, dec_dms;
    getFormattedCoords(alignCoord.ra().Hours(), alignCoord.dec().Degrees(), ra_dms, dec_dms);

    SolverRAOut->setText(ra_dms);
    SolverDecOut->setText(dec_dms);

    int elapsed = (int) round(solverTimer.elapsed()/1000.0);
    appendLogText(i18np("Solver completed in %1 second.", "Solver completed in %1 seconds.", elapsed));

    executeMode();

    // Remove files left over by the solver
    int fitsLoc = fitsFile.indexOf("fits");
    QString tmpDir  = fitsFile.left(fitsLoc);
    QString tmpFITS = fitsFile.remove(tmpDir);

    tmpFITS.replace(".tmp", "*.*");

    QDir dir(tmpDir);
    dir.setNameFilters(QStringList() << tmpFITS);
    dir.setFilter(QDir::Files);
    foreach(QString dirFile, dir.entryList())
            dir.remove(dirFile);

}

void Align::logSolver()
{
    qDebug() << solver.readAll() << endl;
}

void Align::appendLogText(const QString &text)
{
    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    emit newLog();
}

void Align::clearLog()
{
    logText.clear();
    emit newLog();
}

void Align::updateScopeCoords(INumberVectorProperty *coord)
{
    QString ra_dms, dec_dms;

    if (!strcmp(coord->name, "EQUATORIAL_EOD_COORD"))
    {        
        getFormattedCoords(coord->np[0].value, coord->np[1].value, ra_dms, dec_dms);

        ScopeRAOut->setText(ra_dms);
        ScopeDecOut->setText(dec_dms);

        generateArgs();
    }
}

void Align::executeMode()
{
    if (gotoR->isChecked())
        executeGOTO();
    else
        executePolarAlign();
}


void Align::executeGOTO()
{
    if (syncR->isChecked())
        Sync();
    else
        SlewToTarget();
}

void Align::Sync()
{
    if (currentTelescope->Sync(&alignCoord))
        appendLogText(i18n("Syncing successful."));
    else
        appendLogText(i18n("Syncing failed."));

}

void Align::SlewToTarget()
{
    Sync();

    currentTelescope->Slew(&targetCoord);

    appendLogText(i18n("Slewing to target."));
}

void Align::executePolarAlign()
{
    //TODO
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

}

#include "align.moc"
