/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    if (remoteAstrometry == nullptr)
    {
        align->appendLogText(
            i18n("Cannot set solver to remote. The Ekos equipment profile must include the astrometry Auxiliary driver."));
        return false;
    }

    return true;
}

void RemoteAstrometryParser::verifyIndexFiles(double, double)
{
}

bool RemoteAstrometryParser::startSolver(const QString &filename, const QStringList &args, bool generated)
{
    INDI_UNUSED(generated);

    QFile fp(filename);
    if (fp.open(QIODevice::ReadOnly) == false)
    {
        align->appendLogText(i18n("Cannot open file %1 for reading.", filename));
        emit solverFailed();
        return false;
    }

    auto solverSwitch = remoteAstrometry->getBaseDevice()->getSwitch("ASTROMETRY_SOLVER");
    auto solverBLOB   = remoteAstrometry->getBaseDevice()->getBLOB("ASTROMETRY_DATA");

    if (!solverSwitch || !solverBLOB)
    {
        align->appendLogText(i18n("Failed to find solver properties."));
        fp.close();
        emit solverFailed();
        return false;
    }

    sendArgs(args);

    auto enableSW = solverSwitch->findWidgetByName("ASTROMETRY_SOLVER_ENABLE");
    if (enableSW->getState() == ISS_OFF)
    {
        IUResetSwitch(solverSwitch);
        enableSW->setState(ISS_ON);
        remoteAstrometry->getDriverInfo()->getClientManager()->sendNewSwitch(solverSwitch);
    }

    // #PS: TODO
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
    auto solverSettings = remoteAstrometry->getBaseDevice()->getText("ASTROMETRY_SETTINGS");

    if (!solverSettings)
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

    //for (int i = 0; i < solverSettings->ntp; i++)
    for (auto &it : *solverSettings)
    {
        // 2016-10-20: Disable setting this automatically since remote device might have different
        // settings
        /*if (!strcmp(solverSettings->tp[i].name, "ASTROMETRY_SETTINGS_BINARY"))
            IUSaveText(&solverSettings->tp[i], Options::astrometrySolver().toLatin1().constData());*/
        if (it.isNameMatch("ASTROMETRY_SETTINGS_OPTIONS"))
            it.setText(solverArgs.join(" ").toLatin1().constData());
    }

    remoteAstrometry->getDriverInfo()->getClientManager()->sendNewText(solverSettings);
    INDI_D *guiDevice = GUIManager::Instance()->findGUIDevice(remoteAstrometry->getDeviceName());
    if (guiDevice)
        guiDevice->updateTextGUI(solverSettings);

    return true;
}

void RemoteAstrometryParser::setEnabled(bool enable)
{
    auto solverSwitch = remoteAstrometry->getBaseDevice()->getSwitch("ASTROMETRY_SOLVER");

    if (!solverSwitch)
        return;

    auto enableSW  = solverSwitch->findWidgetByName("ASTROMETRY_SOLVER_ENABLE");
    auto disableSW = solverSwitch->findWidgetByName("ASTROMETRY_SOLVER_DISABLE");

    if (!enableSW || !disableSW)
        return;

    if (enable && enableSW->getState() == ISS_OFF)
    {
        solverSwitch->reset();
        enableSW->setState(ISS_ON);
        remoteAstrometry->getDriverInfo()->getClientManager()->sendNewSwitch(solverSwitch);
        solverRunning = true;
        qCDebug(KSTARS_EKOS_ALIGN) << "Enabling remote solver...";
    }
    else if (enable == false && disableSW->s == ISS_OFF)
    {
        solverSwitch->reset();
        disableSW->setState(ISS_ON);
        remoteAstrometry->getDriverInfo()->getClientManager()->sendNewSwitch(solverSwitch);
        solverRunning = false;
        qCDebug(KSTARS_EKOS_ALIGN) << "Disabling remote solver...";
    }
}

bool RemoteAstrometryParser::setCCD(const QString &ccd)
{
    targetCCD = ccd;

    if (!remoteAstrometry)
        return false;

    auto activeDevices = remoteAstrometry->getBaseDevice()->getText("ACTIVE_DEVICES");

    if (!activeDevices)
        return false;

    auto activeCCD  = activeDevices->findWidgetByName("ACTIVE_CCD");

    if (!activeCCD)
        return false;

    // If same device, no need to update
    if (QString(activeCCD->getText()) == ccd)
        return true;

    activeCCD->setText(ccd.toLatin1().data());
    remoteAstrometry->getDriverInfo()->getClientManager()->sendNewText(activeDevices);

    return true;
}

bool RemoteAstrometryParser::stopSolver()
{
    if (solverRunning == false)
        return true;

    // Disable solver
    auto svp = remoteAstrometry->getBaseDevice()->getSwitch("ASTROMETRY_SOLVER");
    if (!svp)
        return false;

    auto disableSW = svp->findWidgetByName("ASTROMETRY_SOLVER_DISABLE");
    if (disableSW->getState() == ISS_OFF)
    {
        svp->reset();
        disableSW->setState(ISS_ON);
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
        emit solverFinished(orientation, ra, de, pixscale, parity != "pos");
    }
}
}
