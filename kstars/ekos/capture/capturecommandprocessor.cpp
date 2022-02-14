/*  Ekos commands for the capture module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturecommandprocessor.h"

#include "ksmessagebox.h"
#include "Options.h"
#include "indi/indistd.h"

namespace Ekos
{
CaptureCommandProcessor::CaptureCommandProcessor()
{

}

void CaptureCommandProcessor::setLightBox(ISD::LightBox *device)
{
    m_lightBox = device;
}

void CaptureCommandProcessor::setDustCap(ISD::DustCap *device)
{
    // clean up old connections
    if (m_dustCap != nullptr)
        disconnect(m_dustCap, nullptr, this, nullptr);

    // connect new device
    if (device != nullptr)
        connect(device, &ISD::DustCap::newStatus, this, &CaptureCommandProcessor::dustCapStatusChanged);

    m_dustCap = device;
}

void CaptureCommandProcessor::setTelescope(ISD::Telescope *device)
{
    // clean up old connections
    if (m_telescope != nullptr)
        disconnect(m_telescope, nullptr, this, nullptr);
    // connect new device
    if (device != nullptr)
    {
        connect(device, &ISD::Telescope::newStatus, this, &CaptureCommandProcessor::scopeStatusChanged);
        connect(device, &ISD::Telescope::newParkStatus, this, &CaptureCommandProcessor::scopeParkStatusChanged);
    }

    m_telescope = device;
}

void CaptureCommandProcessor::setDome(ISD::Dome *device)
{
    // clean up old connections
    if (m_dome != nullptr)
        disconnect(m_dome, nullptr, this, nullptr);
    // connect new device
    if (device != nullptr)
        connect(device, &ISD::Dome::newStatus, this, &CaptureCommandProcessor::domeStatusChanged);

    m_dome = device;
}

void CaptureCommandProcessor::setRotatorAngle(double *rawAngle)
{
 if (activeRotator != nullptr)
     activeRotator->runCommand(INDI_SET_ROTATOR_ANGLE, rawAngle);
}

void CaptureCommandProcessor::setCCDTemperature(double temp)
{
    if (activeCCD != nullptr)
        activeCCD->setTemperature(temp);
}

void CaptureCommandProcessor::enableCCDBatchMode(bool enable)
{
    if (activeChip != nullptr)
        activeChip->setBatchMode(enable);
}

void CaptureCommandProcessor::askManualScopeLightCover(QString question, QString title)
{
    // do not ask again
    if (m_ManualCoveringAsked == true)
    {
        emit manualScopeLightCover(true, true);
        return;
    }

    // Continue
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
    {
        emit manualScopeLightCover(true, true);
        KSMessageBox::Instance()->disconnect(this);
        m_ManualCoveringAsked = true;
    });

    // Cancel
    connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
    {
        emit manualScopeLightCover(true, false);
        KSMessageBox::Instance()->disconnect(this);
    });

    KSMessageBox::Instance()->warningContinueCancel(question, title, Options::manualCoverTimeout());

}

void CaptureCommandProcessor::askManualScopeLightOpen()
{
    // Continue
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
    {
        emit manualScopeLightCover(false, true);
        KSMessageBox::Instance()->disconnect(this);
    });

    // Cancel
    connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
    {
        emit manualScopeLightCover(false, false);
        KSMessageBox::Instance()->disconnect(this);
    });

    KSMessageBox::Instance()->warningContinueCancel(i18n("Remove cover from the telescope in order to continue."),
            i18n("Telescope Covered"), Options::manualCoverTimeout());

}

void CaptureCommandProcessor::setLightBoxLight(bool on)
{
    m_lightBox->SetLightEnabled(on);
    emit lightBoxLight(on);
}

void CaptureCommandProcessor::parkDustCap(bool park)
{
    // park
    if (park == true)
        if (m_dustCap->Park())
            emit dustCapStatusChanged(ISD::DustCap::CAP_PARKING);
        else
            emit dustCapStatusChanged(ISD::DustCap::CAP_ERROR);
    // unpark
    else
        if (m_dustCap->UnPark())
            emit dustCapStatusChanged(ISD::DustCap::CAP_UNPARKING);
        else
            emit dustCapStatusChanged(ISD::DustCap::CAP_ERROR);
}

void CaptureCommandProcessor::setDustCapLight(bool on)
{
    // If light is not on, set it.
    if (m_dustCap->hasLight())
    {
        m_dustCap->SetLightEnabled(on);
        emit dustCapLight(on);
    }
}

void CaptureCommandProcessor::slewTelescope(SkyPoint &target)
{
    if (m_telescope != nullptr)
    {
        m_telescope->Slew(&target);
        emit scopeStatusChanged(ISD::Telescope::MOUNT_SLEWING);
    }
}

void CaptureCommandProcessor::setScopeTracking(bool on)
{
    if (m_telescope != nullptr)
    {
        m_telescope->setTrackEnabled(on);
        emit scopeStatusChanged(on ? ISD::Telescope::MOUNT_TRACKING : ISD::Telescope::MOUNT_IDLE);
    }
}

void Ekos::CaptureCommandProcessor::setScopeParked(bool parked)
{
    if (m_telescope != nullptr)
    {
        if (parked == true)
        {
            if (m_telescope->Park())
                emit scopeStatusChanged(ISD::Telescope::MOUNT_PARKING);
            else
                emit scopeStatusChanged(ISD::Telescope::MOUNT_ERROR);
        }
        else
        {
            if (m_telescope->UnPark() == false)
                emit scopeStatusChanged(ISD::Telescope::MOUNT_ERROR);
        }
    }
}

void Ekos::CaptureCommandProcessor::setDomeParked(bool parked)
{
    if (m_dome != nullptr)
    {
        if (parked == true)
        {
            if (m_dome->Park())
                emit scopeStatusChanged(ISD::Telescope::MOUNT_PARKING);
            else
                emit scopeStatusChanged(ISD::Telescope::MOUNT_ERROR);
        }
        else
        {
            if (m_telescope->UnPark() == false)
                emit scopeStatusChanged(ISD::Telescope::MOUNT_ERROR);
        }
    }

}

void CaptureCommandProcessor::flatSyncFocus(int targetFilterID)
{
    if (filterManager->syncAbsoluteFocusPosition(targetFilterID - 1))
        emit flatSyncFocusChanged(true);
    else
        emit flatSyncFocusChanged(false);
}

void CaptureCommandProcessor::queryHasShutter()
{
    if (activeCCD == nullptr)
    {
        emit hasShutter(false);
        return;
    }
    QStringList shutterfulCCDs  = Options::shutterfulCCDs();
    QStringList shutterlessCCDs = Options::shutterlessCCDs();
    QString deviceName = activeCCD->getDeviceName();

    bool shutterFound   = shutterfulCCDs.contains(deviceName);
    // FIXME: what about || (captureISOS && captureISOS->count() > 0?
    bool noShutterFound = shutterlessCCDs.contains(deviceName);

    if (shutterFound == true)
        emit hasShutter(true);
    else if (noShutterFound == true)
        emit hasShutter(false);
    else
    {
        // If we have no information, we ask before we proceed.
        QString deviceName = activeCCD->getDeviceName();
        // Yes, has shutter
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
        {
            KSMessageBox::Instance()->disconnect(this);
            QStringList shutterfulCCDs  = Options::shutterfulCCDs();
            shutterfulCCDs.append(activeCCD->getDeviceName());
            Options::setShutterfulCCDs(shutterfulCCDs);
            emit hasShutter(true);
        });
        // No, has no shutter
        connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
        {
            KSMessageBox::Instance()->disconnect(this);
            QStringList shutterlessCCDs = Options::shutterlessCCDs();
            shutterlessCCDs.append(activeCCD->getDeviceName());
            Options::setShutterlessCCDs(shutterlessCCDs);
            emit hasShutter(false);
        });

        KSMessageBox::Instance()->questionYesNo(i18n("Does %1 have a shutter?", deviceName),
                                                i18n("Dark Exposure"));
    }
}

} // namespace
