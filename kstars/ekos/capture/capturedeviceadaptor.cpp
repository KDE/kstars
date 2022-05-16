/*  Ekos commands for the capture module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturedeviceadaptor.h"

#include "ksmessagebox.h"
#include "Options.h"
#include "indi/indistd.h"
#include "ekos_capture_debug.h"

namespace Ekos
{
CaptureDeviceAdaptor::CaptureDeviceAdaptor()
{

}

void CaptureDeviceAdaptor::connectDome()
{
    connect(currentSequenceJobState, &SequenceJobState::setDomeParked, this, &CaptureDeviceAdaptor::setDomeParked);
    connect(this, &CaptureDeviceAdaptor::domeStatusChanged, currentSequenceJobState,
            &SequenceJobState::domeStatusChanged);
}

void CaptureDeviceAdaptor::disconnectDome()
{
    disconnect(currentSequenceJobState, &SequenceJobState::setDomeParked, this, &CaptureDeviceAdaptor::setDomeParked);
    disconnect(this, &CaptureDeviceAdaptor::domeStatusChanged, currentSequenceJobState,
               &SequenceJobState::domeStatusChanged);
}

void CaptureDeviceAdaptor::setCurrentSequenceJobState(SequenceJobState *jobState)
{
    currentSequenceJobState = jobState;
}

void CaptureDeviceAdaptor::setLightBox(ISD::LightBox *device)
{
    m_lightBox = device;
    if (currentSequenceJobState != nullptr && !currentSequenceJobState->m_CaptureState.isNull())
        currentSequenceJobState->m_CaptureState->hasLightBox = (device != nullptr);
}

void CaptureDeviceAdaptor::setDustCap(ISD::DustCap *device)
{
    m_dustCap = device;
}

void CaptureDeviceAdaptor::connectDustCap()
{
    if (m_dustCap != nullptr)
        connect(m_dustCap, &ISD::DustCap::newStatus, this, &CaptureDeviceAdaptor::dustCapStatusChanged);

    connect(currentSequenceJobState, &SequenceJobState::askManualScopeLightCover, this,
            &CaptureDeviceAdaptor::askManualScopeLightCover);
    connect(currentSequenceJobState, &SequenceJobState::askManualScopeLightOpen, this,
            &CaptureDeviceAdaptor::askManualScopeLightOpen);
    connect(currentSequenceJobState, &SequenceJobState::setLightBoxLight, this,
            &CaptureDeviceAdaptor::setLightBoxLight);
    connect(currentSequenceJobState, &SequenceJobState::setDustCapLight, this,
            &CaptureDeviceAdaptor::setDustCapLight);
    connect(currentSequenceJobState, &SequenceJobState::parkDustCap, this, &CaptureDeviceAdaptor::parkDustCap);

    connect(this, &CaptureDeviceAdaptor::manualScopeLightCover, currentSequenceJobState,
            &SequenceJobState::manualScopeLightCover);
    connect(this, &CaptureDeviceAdaptor::lightBoxLight, currentSequenceJobState, &SequenceJobState::lightBoxLight);
    connect(this, &CaptureDeviceAdaptor::dustCapLight, currentSequenceJobState, &SequenceJobState::dustCapLight);
    connect(this, &CaptureDeviceAdaptor::dustCapStatusChanged, currentSequenceJobState,
            &SequenceJobState::dustCapStatusChanged);
}

void CaptureDeviceAdaptor::disconnectDustCap()
{
    if (m_dustCap != nullptr)
        disconnect(m_dustCap, nullptr, this, nullptr);

    disconnect(currentSequenceJobState, &SequenceJobState::askManualScopeLightCover, this,
               &CaptureDeviceAdaptor::askManualScopeLightCover);
    disconnect(currentSequenceJobState, &SequenceJobState::askManualScopeLightOpen, this,
               &CaptureDeviceAdaptor::askManualScopeLightOpen);
    disconnect(currentSequenceJobState, &SequenceJobState::setLightBoxLight, this,
               &CaptureDeviceAdaptor::setLightBoxLight);
    disconnect(currentSequenceJobState, &SequenceJobState::setDustCapLight, this,
               &CaptureDeviceAdaptor::setDustCapLight);
    disconnect(currentSequenceJobState, &SequenceJobState::parkDustCap, this, &CaptureDeviceAdaptor::parkDustCap);

    disconnect(this, &CaptureDeviceAdaptor::manualScopeLightCover, currentSequenceJobState,
               &SequenceJobState::manualScopeLightCover);
    disconnect(this, &CaptureDeviceAdaptor::lightBoxLight, currentSequenceJobState, &SequenceJobState::lightBoxLight);
    disconnect(this, &CaptureDeviceAdaptor::dustCapLight, currentSequenceJobState, &SequenceJobState::dustCapLight);
    disconnect(this, &CaptureDeviceAdaptor::dustCapStatusChanged, currentSequenceJobState,
               &SequenceJobState::dustCapStatusChanged);
}

void CaptureDeviceAdaptor::setTelescope(ISD::Telescope *device)
{
    // clean up old connections
    if (m_telescope != nullptr)
        disconnect(m_telescope, nullptr, this, nullptr);
    // connect new device
    if (device != nullptr)
    {
        connect(device, &ISD::Telescope::newStatus, this, &CaptureDeviceAdaptor::scopeStatusChanged);
        connect(device, &ISD::Telescope::newParkStatus, this, &CaptureDeviceAdaptor::scopeParkStatusChanged);
    }

    m_telescope = device;
}

void CaptureDeviceAdaptor::connectTelescope()
{
    connect(currentSequenceJobState, &SequenceJobState::slewTelescope, this, &CaptureDeviceAdaptor::slewTelescope);
    connect(currentSequenceJobState, &SequenceJobState::setScopeTracking, this,
            &CaptureDeviceAdaptor::setScopeTracking);
    connect(currentSequenceJobState, &SequenceJobState::setScopeParked, this, &CaptureDeviceAdaptor::setScopeParked);

    connect(this, &CaptureDeviceAdaptor::scopeStatusChanged, currentSequenceJobState,
            &SequenceJobState::scopeStatusChanged);
    connect(this, &CaptureDeviceAdaptor::scopeParkStatusChanged, currentSequenceJobState,
            &SequenceJobState::scopeParkStatusChanged);
}

void CaptureDeviceAdaptor::disconnectTelescope()
{
    disconnect(currentSequenceJobState, &SequenceJobState::slewTelescope, this, &CaptureDeviceAdaptor::slewTelescope);
    disconnect(currentSequenceJobState, &SequenceJobState::setScopeTracking, this,
               &CaptureDeviceAdaptor::setScopeTracking);
    disconnect(currentSequenceJobState, &SequenceJobState::setScopeParked, this, &CaptureDeviceAdaptor::setScopeParked);

    disconnect(this, &CaptureDeviceAdaptor::scopeStatusChanged, currentSequenceJobState,
               &SequenceJobState::scopeStatusChanged);
    disconnect(this, &CaptureDeviceAdaptor::scopeParkStatusChanged, currentSequenceJobState,
               &SequenceJobState::scopeParkStatusChanged);
}

void CaptureDeviceAdaptor::setDome(ISD::Dome *device)
{
    // clean up old connections
    if (m_dome != nullptr)
        disconnect(m_dome, nullptr, this, nullptr);
    // connect new device
    if (device != nullptr)
        connect(device, &ISD::Dome::newStatus, this, &CaptureDeviceAdaptor::domeStatusChanged);

    m_dome = device;
}

void CaptureDeviceAdaptor::setRotator(ISD::GDInterface *device)
{
    // clean up old connections
    if (activeRotator != nullptr)
        disconnect(activeRotator, &ISD::GDInterface::numberUpdated, this, &CaptureDeviceAdaptor::updateRotatorNumber);

    activeRotator = device;

    // connect new device
    if (activeRotator != nullptr)
        connect(activeRotator, &ISD::GDInterface::numberUpdated, this, &CaptureDeviceAdaptor::updateRotatorNumber, Qt::UniqueConnection);

}

void CaptureDeviceAdaptor::connectRotator()
{
    connect(this, &CaptureDeviceAdaptor::newRotatorAngle, currentSequenceJobState,
            &SequenceJobState::setCurrentRotatorPositionAngle, Qt::UniqueConnection);
    connect(currentSequenceJobState, &SequenceJobState::setRotatorAngle, this,
            &CaptureDeviceAdaptor::setRotatorAngle);
}

void CaptureDeviceAdaptor::disconnectRotator()
{
    disconnect(currentSequenceJobState, &SequenceJobState::setRotatorAngle, this,
               &CaptureDeviceAdaptor::setRotatorAngle);
    disconnect(this, &CaptureDeviceAdaptor::newRotatorAngle, currentSequenceJobState,
            &SequenceJobState::setCurrentRotatorPositionAngle);
}

void CaptureDeviceAdaptor::setRotatorAngle(double *rawAngle)
{
 if (activeRotator != nullptr)
     activeRotator->runCommand(INDI_SET_ROTATOR_ANGLE, rawAngle);
}

void CaptureDeviceAdaptor::readRotatorAngle()
{
    if (activeRotator != nullptr)
    {
        auto nvp = activeRotator->getBaseDevice()->getNumber("ABS_ROTATOR_ANGLE");
        emit newRotatorAngle(nvp->at(0)->getValue(), nvp->s);
    }
}

void CaptureDeviceAdaptor::updateRotatorNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "ABS_ROTATOR_ANGLE"))
        emit newRotatorAngle(nvp->np[0].value, nvp->s);
}

void CaptureDeviceAdaptor::setActiveCCD(ISD::CCD *device)
{
    // disconnect device events if the new device is not empty
    if (activeCCD != nullptr)
    {
        disconnect(activeCCD, &ISD::CCD::newTemperatureValue, this,
                   &Ekos::CaptureDeviceAdaptor::newCCDTemperatureValue);
    }

    // store the link to the new device
    activeCCD = device;

    // connect device events if the new device is not empty
    if (activeCCD != nullptr)
    {
        // publish device events
        connect(activeCCD, &ISD::CCD::newTemperatureValue, this,
                &Ekos::CaptureDeviceAdaptor::newCCDTemperatureValue, Qt::UniqueConnection);
    }
}

void CaptureDeviceAdaptor::connectActiveCCD()
{
    //connect state machine to device adaptor
    connect(currentSequenceJobState, &SequenceJobState::setCCDTemperature, this,
            &CaptureDeviceAdaptor::setCCDTemperature);
    connect(currentSequenceJobState, &SequenceJobState::setCCDBatchMode, this,
            &CaptureDeviceAdaptor::enableCCDBatchMode);
    connect(currentSequenceJobState, &SequenceJobState::queryHasShutter, this,
            &CaptureDeviceAdaptor::queryHasShutter);

    // forward own events to the state machine
    connect(this, &CaptureDeviceAdaptor::flatSyncFocusChanged, currentSequenceJobState,
            &SequenceJobState::flatSyncFocusChanged);
    connect(this, &CaptureDeviceAdaptor::hasShutter, currentSequenceJobState, &SequenceJobState::hasShutter);
    connect(this, &CaptureDeviceAdaptor::newCCDTemperatureValue, currentSequenceJobState,
            &Ekos::SequenceJobState::setCurrentCCDTemperature, Qt::UniqueConnection);
}

void CaptureDeviceAdaptor::disconnectActiveCCD()
{
    disconnect(currentSequenceJobState, &SequenceJobState::setCCDTemperature, this,
               &CaptureDeviceAdaptor::setCCDTemperature);
    disconnect(currentSequenceJobState, &SequenceJobState::setCCDBatchMode, this,
               &CaptureDeviceAdaptor::enableCCDBatchMode);
    disconnect(currentSequenceJobState, &SequenceJobState::queryHasShutter, this,
               &CaptureDeviceAdaptor::queryHasShutter);

    disconnect(this, &CaptureDeviceAdaptor::flatSyncFocusChanged, currentSequenceJobState,
               &SequenceJobState::flatSyncFocusChanged);
    disconnect(this, &CaptureDeviceAdaptor::hasShutter, currentSequenceJobState, &SequenceJobState::hasShutter);
    disconnect(this, &CaptureDeviceAdaptor::newCCDTemperatureValue, currentSequenceJobState,
               &Ekos::SequenceJobState::setCurrentCCDTemperature);
}

void CaptureDeviceAdaptor::readCurrentState(Ekos::CaptureState state)
{
    switch(state)
    {
    case CAPTURE_SETTING_TEMPERATURE:
        if (activeCCD != nullptr)
        {
            double currentTemperature;
            activeCCD->getTemperature(&currentTemperature);
            emit newCCDTemperatureValue(currentTemperature);
        }
        break;
    case CAPTURE_SETTING_ROTATOR:
        readRotatorAngle();
        break;
    case CAPTURE_GUIDER_DRIFT:
        // intentionally left empty since the guider regularly updates the drift
        break;
    default:
        // this should not happen!
        qWarning(KSTARS_EKOS_CAPTURE) << "Reading device state " << state << " not implemented!";
        break;
    }
}

void CaptureDeviceAdaptor::setCCDTemperature(double temp)
{
    if (activeCCD != nullptr)
        activeCCD->setTemperature(temp);
}

void CaptureDeviceAdaptor::enableCCDBatchMode(bool enable)
{
    if (activeChip != nullptr)
        activeChip->setBatchMode(enable);
}

void CaptureDeviceAdaptor::setActiveChip(ISD::CCDChip *device)
{
    activeChip = device;
}

void CaptureDeviceAdaptor::setFilterWheel(ISD::GDInterface *device)
{
    activeFilterWheel = device;
}

void CaptureDeviceAdaptor::askManualScopeLightCover(QString question, QString title)
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

void CaptureDeviceAdaptor::askManualScopeLightOpen()
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

void CaptureDeviceAdaptor::setLightBoxLight(bool on)
{
    m_lightBox->SetLightEnabled(on);
    emit lightBoxLight(on);
}

void CaptureDeviceAdaptor::parkDustCap(bool park)
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

void CaptureDeviceAdaptor::setDustCapLight(bool on)
{
    // If light is not on, set it.
    if (m_dustCap->hasLight())
    {
        m_dustCap->SetLightEnabled(on);
        emit dustCapLight(on);
    }
}

void CaptureDeviceAdaptor::slewTelescope(SkyPoint &target)
{
    if (m_telescope != nullptr)
    {
        m_telescope->Slew(&target);
        emit scopeStatusChanged(ISD::Telescope::MOUNT_SLEWING);
    }
}

void CaptureDeviceAdaptor::setScopeTracking(bool on)
{
    if (m_telescope != nullptr)
    {
        m_telescope->setTrackEnabled(on);
        emit scopeStatusChanged(on ? ISD::Telescope::MOUNT_TRACKING : ISD::Telescope::MOUNT_IDLE);
    }
}

void Ekos::CaptureDeviceAdaptor::setScopeParked(bool parked)
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

void Ekos::CaptureDeviceAdaptor::setDomeParked(bool parked)
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

void CaptureDeviceAdaptor::flatSyncFocus(int targetFilterID)
{
    if (filterManager->syncAbsoluteFocusPosition(targetFilterID - 1))
        emit flatSyncFocusChanged(true);
    else
        emit flatSyncFocusChanged(false);
}

void CaptureDeviceAdaptor::queryHasShutter()
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
