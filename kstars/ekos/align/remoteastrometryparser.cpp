/*  Astrometry.net Parser - Remote
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "remoteastrometryparser.h"

#include "align.h"
#include "ekos_align_debug.h"
#include "Options.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "indi/guimanager.h"
#include "indi/indidevice.h"

#include <indicom.h>

#include <KMessageBox>

namespace Ekos
{
RemoteAstrometryParser::RemoteAstrometryParser() : AstrometryParser()
{
}

bool RemoteAstrometryParser::init()
{
    return remoteAstrometry != nullptr;
}

void RemoteAstrometryParser::verifyIndexFiles(double, double)
{
}

bool RemoteAstrometryParser::startSovler(const QString &filename, const QStringList &args, bool generated)
{
    INDI_UNUSED(generated);

    QFile fp(filename);
    if (fp.open(QIODevice::ReadOnly) == false)
    {
        align->appendLogText(i18n("Cannot open file %1 for reading.", filename));
        emit solverFailed();
        return false;
    }

    ISwitchVectorProperty *solverSwitch = remoteAstrometry->getBaseDevice()->getSwitch("ASTROMETRY_SOLVER");
    IBLOBVectorProperty *solverBLOB     = remoteAstrometry->getBaseDevice()->getBLOB("ASTROMETRY_DATA");

    if (solverSwitch == nullptr || solverBLOB == nullptr)
    {
        align->appendLogText(i18n("Failed to find solver properties."));
        fp.close();
        emit solverFailed();
        return false;
    }

    sendArgs(args);

    ISwitch *enableSW = IUFindSwitch(solverSwitch, "ASTROMETRY_SOLVER_ENABLE");
    if (enableSW->s == ISS_OFF)
    {
        IUResetSwitch(solverSwitch);
        enableSW->s = ISS_ON;
        remoteAstrometry->getDriverInfo()->getClientManager()->sendNewSwitch(solverSwitch);
    }

    IBLOB *bp = &(solverBLOB->bp[0]);

    bp->bloblen = bp->size = fp.size();

    bp->blob = (uint8_t *)realloc(bp->blob, bp->size);
    if (bp->blob == nullptr)
    {
        align->appendLogText(i18n("Not enough memory for file %1.", filename));
        fp.close();
        emit solverFailed();
        return false;
    }

    memcpy(bp->blob, fp.readAll().constData(), bp->size);

    solverRunning = true;

    remoteAstrometry->getDriverInfo()->getClientManager()->startBlob(solverBLOB->device, solverBLOB->name, timestamp());

#if (INDI_VERSION_MINOR >= 4 && INDI_VERSION_RELEASE >= 2)
    remoteAstrometry->getDriverInfo()->getClientManager()->sendOneBlob(bp);
#else
    remoteAstrometry->getDriverInfo()->getClientManager()->sendOneBlob(bp->name, bp->size, bp->format, bp->blob);
#endif

    remoteAstrometry->getDriverInfo()->getClientManager()->finishBlob();

    align->appendLogText(i18n("Starting remote solver..."));
    solverTimer.start();

    return true;
}

bool RemoteAstrometryParser::sendArgs(const QStringList &args)
{
    ITextVectorProperty *solverSettings = remoteAstrometry->getBaseDevice()->getText("ASTROMETRY_SETTINGS");

    if (solverSettings == nullptr)
    {
        align->appendLogText(i18n("Failed to find solver settings."));
        emit solverFailed();
        return false;
    }

    QStringList solverArgs = args;
    // Add parity option if none is give and we already know parity before
    // and is NOT a blind solve
    if (Options::astrometryDetectParity() && parity.isEmpty() == false && args.contains("parity") == false &&
        (args.contains("-3") || args.contains("-L")))
        solverArgs << "--parity" << parity;

    for (int i = 0; i < solverSettings->ntp; i++)
    {
        // 2016-10-20: Disable setting this automatically since remote device might have different
        // settings
        /*if (!strcmp(solverSettings->tp[i].name, "ASTROMETRY_SETTINGS_BINARY"))
            IUSaveText(&solverSettings->tp[i], Options::astrometrySolver().toLatin1().constData());*/
        if (!strcmp(solverSettings->tp[i].name, "ASTROMETRY_SETTINGS_OPTIONS"))
            IUSaveText(&solverSettings->tp[i], solverArgs.join(" ").toLatin1().constData());
    }

    remoteAstrometry->getDriverInfo()->getClientManager()->sendNewText(solverSettings);
    INDI_D *guiDevice = GUIManager::Instance()->findGUIDevice(remoteAstrometry->getDeviceName());
    if (guiDevice)
        guiDevice->updateTextGUI(solverSettings);

    return true;
}

void RemoteAstrometryParser::setEnabled(bool enable)
{
    ISwitchVectorProperty *solverSwitch = remoteAstrometry->getBaseDevice()->getSwitch("ASTROMETRY_SOLVER");

    if (solverSwitch == nullptr)
        return;

    ISwitch *enableSW  = IUFindSwitch(solverSwitch, "ASTROMETRY_SOLVER_ENABLE");
    ISwitch *disableSW = IUFindSwitch(solverSwitch, "ASTROMETRY_SOLVER_DISABLE");

    if (enableSW == nullptr || disableSW == nullptr)
        return;

    if (enable && enableSW->s == ISS_OFF)
    {
        IUResetSwitch(solverSwitch);
        enableSW->s = ISS_ON;
        remoteAstrometry->getDriverInfo()->getClientManager()->sendNewSwitch(solverSwitch);
        solverRunning = true;
        qCDebug(KSTARS_EKOS_ALIGN) << "Enabling remote solver...";
    }
    else if (enable == false && disableSW->s == ISS_OFF)
    {
        IUResetSwitch(solverSwitch);
        disableSW->s = ISS_ON;
        remoteAstrometry->getDriverInfo()->getClientManager()->sendNewSwitch(solverSwitch);
        solverRunning = false;
        qCDebug(KSTARS_EKOS_ALIGN) << "Disabling remote solver...";
    }
}

bool RemoteAstrometryParser::setCCD(const QString &ccd)
{
    targetCCD = ccd;

    if (remoteAstrometry == nullptr)
        return false;

    ITextVectorProperty *activeDevices = remoteAstrometry->getBaseDevice()->getText("ACTIVE_DEVICES");

    if (activeDevices == nullptr)
        return false;

    IText *activeCCD  = IUFindText(activeDevices, "ACTIVE_CCD");

    if (activeCCD == nullptr)
        return false;

    // If same device, no need to update
    if (QString(activeCCD->text) == ccd)
        return true;

    IUSaveText(activeCCD, ccd.toLatin1().data());
    remoteAstrometry->getDriverInfo()->getClientManager()->sendNewText(activeDevices);

    return true;
}

bool RemoteAstrometryParser::stopSolver()
{
    // Disable solver
    ISwitchVectorProperty *svp = remoteAstrometry->getBaseDevice()->getSwitch("ASTROMETRY_SOLVER");
    if (!svp)
        return false;

    ISwitch *disableSW = IUFindSwitch(svp, "ASTROMETRY_SOLVER_DISABLE");
    if (disableSW->s == ISS_OFF)
    {
        IUResetSwitch(svp);
        disableSW->s = ISS_ON;
        remoteAstrometry->getDriverInfo()->getClientManager()->sendNewSwitch(svp);
    }

    solverRunning = false;

    return true;
}

void RemoteAstrometryParser::setAstrometryDevice(ISD::GDInterface *device)
{
    if (device == remoteAstrometry)
        return;

    remoteAstrometry = device;

    remoteAstrometry->disconnect(this);

    connect(remoteAstrometry, SIGNAL(switchUpdated(ISwitchVectorProperty*)), this,
            SLOT(checkStatus(ISwitchVectorProperty*)));
    connect(remoteAstrometry, SIGNAL(numberUpdated(INumberVectorProperty*)), this,
            SLOT(checkResults(INumberVectorProperty*)));

    if (targetCCD.isEmpty() == false)
        setCCD(targetCCD);
}

void RemoteAstrometryParser::checkStatus(ISwitchVectorProperty *svp)
{
    if (solverRunning == false || strcmp(svp->name, "ASTROMETRY_SOLVER"))
        return;

    if (svp->s == IPS_ALERT)
    {
        stopSolver();
        align->appendLogText(i18n("Solver failed. Try again."));
        emit solverFailed();
        return;
    }
    // In case the remote solver started solving by listening to ACTIVE_CCD BLOB remotely via snooping
    // then we need to start the timer.
    else if (svp->s == IPS_BUSY)
    {
        solverTimer.restart();
    }
}

void RemoteAstrometryParser::checkResults(INumberVectorProperty *nvp)
{
    if (solverRunning == false || strcmp(nvp->name, "ASTROMETRY_RESULTS") || nvp->s != IPS_OK)
        return;

    double pixscale, ra, de, orientation;
    pixscale = ra = de = orientation = -1000;

    for (int i = 0; i < nvp->nnp; i++)
    {
        if (!strcmp(nvp->np[i].name, "ASTROMETRY_RESULTS_PIXSCALE"))
            pixscale = nvp->np[i].value;
        else if (!strcmp(nvp->np[i].name, "ASTROMETRY_RESULTS_ORIENTATION"))
            orientation = nvp->np[i].value;
        else if (!strcmp(nvp->np[i].name, "ASTROMETRY_RESULTS_RA"))
            ra = nvp->np[i].value;
        else if (!strcmp(nvp->np[i].name, "ASTROMETRY_RESULTS_DE"))
            de = nvp->np[i].value;
        else if (!strcmp(nvp->np[i].name, "ASTROMETRY_RESULTS_PARITY"))
        {
            if (nvp->np[i].value == 1)
                parity = "pos";
            else if (nvp->np[i].value == -1)
                parity = "neg";
        }
    }

    if (pixscale != -1000 && ra != -1000 && de != -1000 && orientation != -1000)
    {
        int elapsed = (int)round(solverTimer.elapsed() / 1000.0);
        align->appendLogText(i18np("Solver completed in %1 second.", "Solver completed in %1 seconds.", elapsed));
        stopSolver();
        emit solverFinished(orientation, ra, de, pixscale);
    }
}
}
