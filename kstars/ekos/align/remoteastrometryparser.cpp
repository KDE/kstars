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
    if (m_RemoteAstrometry == nullptr)
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

    auto solverSwitch = m_RemoteAstrometry->getProperty("ASTROMETRY_SOLVER").getSwitch();
    auto solverBLOB   = m_RemoteAstrometry->getProperty("ASTROMETRY_DATA").getBLOB();

    if (solverSwitch->isEmpty() || solverBLOB->isEmpty())
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
        solverSwitch->reset();
        enableSW->setState(ISS_ON);
        m_RemoteAstrometry->sendNewProperty(solverSwitch);
    }

    // #PS: TODO
    auto bp = solverBLOB->at(0);

    bp->setBlobLen(fp.size());
    bp->setSize(fp.size());

    bp->setBlob(static_cast<uint8_t *>(realloc(bp->getBlob(), bp->getSize())));
    if (bp->getBlob() == nullptr)
    {
        align->appendLogText(i18n("Not enough memory for file %1.", filename));
        fp.close();
        emit solverFailed();
        return false;
    }

    memcpy(bp->blob, fp.readAll().constData(), bp->size);

    solverRunning = true;

    m_RemoteAstrometry->getDriverInfo()->getClientManager()->startBlob(solverBLOB->getDeviceName(), solverBLOB->getName(),
            timestamp());

    m_RemoteAstrometry->getDriverInfo()->getClientManager()->sendOneBlob(bp);

    m_RemoteAstrometry->getDriverInfo()->getClientManager()->finishBlob();

    align->appendLogText(i18n("Starting remote solver..."));
    solverTimer.start();

    return true;
}

bool RemoteAstrometryParser::sendArgs(const QStringList &args)
{
    auto solverSettings = m_RemoteAstrometry->getBaseDevice().getText("ASTROMETRY_SETTINGS");

    if (!solverSettings)
    {
        align->appendLogText(i18n("Failed to find solver settings."));
        emit solverFailed();
        return false;
    }

    QStringList solverArgs = args;
    // Add parity option if none is give and we already know parity before
    // and is NOT a blind solve
    if (Options::astrometryDetectParity() && args.contains("parity") == false &&
            (args.contains("-3") || args.contains("-L")))
        solverArgs << "--parity" << QString::number(parity);

    //for (int i = 0; i < solverSettings->ntp; i++)
    for (auto it : solverSettings)
    {
        // 2016-10-20: Disable setting this automatically since remote device might have different
        // settings
        /*if (!strcmp(solverSettings->tp[i].name, "ASTROMETRY_SETTINGS_BINARY"))
            IUSaveText(&solverSettings->tp[i], Options::astrometrySolver().toLatin1().constData());*/
        if (it.isNameMatch("ASTROMETRY_SETTINGS_OPTIONS"))
            it.setText(solverArgs.join(" ").toLatin1().constData());
    }

    m_RemoteAstrometry->sendNewProperty(solverSettings);
    INDI_D *guiDevice = GUIManager::Instance()->findGUIDevice(m_RemoteAstrometry->getDeviceName());
    if (guiDevice)
        guiDevice->updateTextGUI(solverSettings);

    return true;
}

void RemoteAstrometryParser::setEnabled(bool enable)
{
    auto solverSwitch = m_RemoteAstrometry->getBaseDevice().getSwitch("ASTROMETRY_SOLVER");

    if (!solverSwitch)
        return;

    auto enableSW  = solverSwitch.findWidgetByName("ASTROMETRY_SOLVER_ENABLE");
    auto disableSW = solverSwitch.findWidgetByName("ASTROMETRY_SOLVER_DISABLE");

    if (!enableSW || !disableSW)
        return;

    if (enable && enableSW->getState() == ISS_OFF)
    {
        solverSwitch.reset();
        enableSW->setState(ISS_ON);
        m_RemoteAstrometry->sendNewProperty(solverSwitch);
        solverRunning = true;
        qCDebug(KSTARS_EKOS_ALIGN) << "Enabling remote solver...";
    }
    else if (enable == false && disableSW->s == ISS_OFF)
    {
        solverSwitch.reset();
        disableSW->setState(ISS_ON);
        m_RemoteAstrometry->sendNewProperty(solverSwitch);
        solverRunning = false;
        qCDebug(KSTARS_EKOS_ALIGN) << "Disabling remote solver...";
    }
}

bool RemoteAstrometryParser::setCCD(const QString &ccd)
{
    targetCCD = ccd;

    if (!m_RemoteAstrometry)
        return false;

    auto activeDevices = m_RemoteAstrometry->getBaseDevice().getText("ACTIVE_DEVICES");

    if (!activeDevices)
        return false;

    auto activeCCD  = activeDevices.findWidgetByName("ACTIVE_CCD");

    if (!activeCCD)
        return false;

    // If same device, no need to update
    if (QString(activeCCD->getText()) == ccd)
        return true;

    activeCCD->setText(ccd.toLatin1().data());
    m_RemoteAstrometry->sendNewProperty(activeDevices);

    return true;
}

bool RemoteAstrometryParser::stopSolver()
{
    if (solverRunning == false)
        return true;

    // Disable solver
    auto svp = m_RemoteAstrometry->getProperty("ASTROMETRY_SOLVER").getSwitch();
    if (svp->isEmpty())
        return false;

    auto disableSW = svp->findWidgetByName("ASTROMETRY_SOLVER_DISABLE");
    if (disableSW->getState() == ISS_OFF)
    {
        svp->reset();
        disableSW->setState(ISS_ON);
        m_RemoteAstrometry->sendNewProperty(svp);
    }

    solverRunning = false;

    return true;
}

void RemoteAstrometryParser::setAstrometryDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (device == m_RemoteAstrometry)
        return;

    m_RemoteAstrometry = device;

    m_RemoteAstrometry->disconnect(this);

    connect(m_RemoteAstrometry.get(), &ISD::GenericDevice::propertyUpdated, this, &RemoteAstrometryParser::checkStatus);
    connect(m_RemoteAstrometry.get(), &ISD::GenericDevice::propertyUpdated, this, &RemoteAstrometryParser::checkResults);

    if (targetCCD.isEmpty() == false)
        setCCD(targetCCD);
}

void RemoteAstrometryParser::checkStatus(INDI::Property prop)
{
    if (solverRunning == false || !prop.isNameMatch("ASTROMETRY_SOLVER"))
        return;

    if (prop.getState() == IPS_ALERT)
    {
        stopSolver();
        align->appendLogText(i18n("Solver failed. Try again."));
        emit solverFailed();
        return;
    }
    // In case the remote solver started solving by listening to ACTIVE_CCD BLOB remotely via snooping
    // then we need to start the timer.
    else if (prop.getState() == IPS_BUSY)
    {
        solverTimer.restart();
    }
}

void RemoteAstrometryParser::checkResults(INDI::Property prop)
{
    if (solverRunning == false || !prop.isNameMatch("ASTROMETRY_RESULTS") || prop.getState() != IPS_OK)
        return;

    double pixscale, ra, de, orientation;
    pixscale = ra = de = orientation = -1000;

    auto nvp = prop.getNumber();

    for (int i = 0; i < nvp->count(); i++)
    {
        auto value = nvp->at(i)->getValue();
        if (nvp->at(i)->isNameMatch("ASTROMETRY_RESULTS_PIXSCALE"))
            pixscale = value;
        else if (nvp->at(i)->isNameMatch("ASTROMETRY_RESULTS_ORIENTATION"))
            orientation = value;
        else if (nvp->at(i)->isNameMatch("ASTROMETRY_RESULTS_RA"))
            ra = value;
        else if (nvp->at(i)->isNameMatch("ASTROMETRY_RESULTS_DE"))
            de = value;
        else if (nvp->at(i)->isNameMatch("ASTROMETRY_RESULTS_PARITY"))
        {
            if (value == 1)
                parity = FITSImage::POSITIVE;
            else if (value == -1)
                parity = FITSImage::NEGATIVE;
        }
    }

    if (pixscale != -1000 && ra != -1000 && de != -1000 && orientation != -1000)
    {
        int elapsed = (int)round(solverTimer.elapsed() / 1000.0);
        align->appendLogText(i18np("Solver completed in %1 second.", "Solver completed in %1 seconds.", elapsed));
        stopSolver();
        emit solverFinished(orientation, ra, de, pixscale, parity != FITSImage::POSITIVE);
    }
}
}
