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
    if (m_ActiveLightBox == device)
        return;

    m_ActiveLightBox = device;
    if (currentSequenceJobState != nullptr && !currentSequenceJobState->m_CaptureState.isNull())
        currentSequenceJobState->m_CaptureState->hasLightBox = (device != nullptr);
}

void CaptureDeviceAdaptor::setDustCap(ISD::DustCap *device)
{
    m_ActiveDustCap = device;
}

void CaptureDeviceAdaptor::connectDustCap()
{
    if (m_ActiveDustCap != nullptr)
        connect(m_ActiveDustCap, &ISD::DustCap::newStatus, this, &CaptureDeviceAdaptor::dustCapStatusChanged);

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
    if (m_ActiveDustCap != nullptr)
        disconnect(m_ActiveDustCap, nullptr, this, nullptr);

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
    if (m_ActiveTelescope == device)
        return;

    // clean up old connections
    if (m_ActiveTelescope != nullptr)
        disconnect(m_ActiveTelescope, nullptr, this, nullptr);
    // connect new device
    if (device != nullptr)
    {
        connect(device, &ISD::Telescope::newStatus, this, &CaptureDeviceAdaptor::scopeStatusChanged);
        connect(device, &ISD::Telescope::newParkStatus, this, &CaptureDeviceAdaptor::scopeParkStatusChanged);
    }

    m_ActiveTelescope = device;
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
    if (m_ActiveDome == device)
        return;

    // clean up old connections
    if (m_ActiveDome != nullptr)
        disconnect(m_ActiveDome, nullptr, this, nullptr);
    // connect new device
    if (device != nullptr)
        connect(device, &ISD::Dome::newStatus, this, &CaptureDeviceAdaptor::domeStatusChanged);

    m_ActiveDome = device;
}

void CaptureDeviceAdaptor::setRotator(ISD::GDInterface *device)
{
    if (m_ActiveRotator == device)
        return;

    // clean up old connections
    if (m_ActiveRotator != nullptr)
        disconnect(m_ActiveRotator, &ISD::GDInterface::numberUpdated, this, &CaptureDeviceAdaptor::updateRotatorNumber);

    m_ActiveRotator = device;

    // connect new device
    if (m_ActiveRotator != nullptr)
        connect(m_ActiveRotator, &ISD::GDInterface::numberUpdated, this, &CaptureDeviceAdaptor::updateRotatorNumber, Qt::UniqueConnection);

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
    if (m_ActiveRotator != nullptr)
     m_ActiveRotator->runCommand(INDI_SET_ROTATOR_ANGLE, rawAngle);
}

void CaptureDeviceAdaptor::readRotatorAngle()
{
    if (m_ActiveRotator != nullptr)
    {
        auto nvp = m_ActiveRotator->getBaseDevice()->getNumber("ABS_ROTATOR_ANGLE");
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
    if (m_ActiveCamera == device)
        return;

    // disconnect device events if the new device is not empty
    if (m_ActiveCamera != nullptr)
    {
        disconnect(m_ActiveCamera, &ISD::CCD::newTemperatureValue, this,
                   &Ekos::CaptureDeviceAdaptor::newCCDTemperatureValue);
    }

    // store the link to the new device
    m_ActiveCamera = device;

    // connect device events if the new device is not empty
    if (m_ActiveCamera != nullptr)
    {
        // publish device events
        connect(m_ActiveCamera, &ISD::CCD::newTemperatureValue, this,
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
        if (m_ActiveCamera != nullptr)
        {
            double currentTemperature;
            m_ActiveCamera->getTemperature(&currentTemperature);
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
    if (m_ActiveCamera != nullptr)
        m_ActiveCamera->setTemperature(temp);
}

void CaptureDeviceAdaptor::enableCCDBatchMode(bool enable)
{
    if (m_ActiveChip != nullptr)
        m_ActiveChip->setBatchMode(enable);
}

void CaptureDeviceAdaptor::setActiveChip(ISD::CCDChip *device)
{
    m_ActiveChip = device;
}

void CaptureDeviceAdaptor::setFilterWheel(ISD::GDInterface *device)
{
    m_ActiveFilterWheel = device;
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
    m_ActiveLightBox->SetLightEnabled(on);
    emit lightBoxLight(on);
}

void CaptureDeviceAdaptor::parkDustCap(bool park)
{
    // park
    if (park == true)
        if (m_ActiveDustCap->Park())
            emit dustCapStatusChanged(ISD::DustCap::CAP_PARKING);
        else
            emit dustCapStatusChanged(ISD::DustCap::CAP_ERROR);
    // unpark
    else
        if (m_ActiveDustCap->UnPark())
            emit dustCapStatusChanged(ISD::DustCap::CAP_UNPARKING);
        else
            emit dustCapStatusChanged(ISD::DustCap::CAP_ERROR);
}

void CaptureDeviceAdaptor::setDustCapLight(bool on)
{
    // If light is not on, set it.
    if (m_ActiveDustCap->hasLight())
    {
        m_ActiveDustCap->SetLightEnabled(on);
        emit dustCapLight(on);
    }
}

void CaptureDeviceAdaptor::slewTelescope(SkyPoint &target)
{
    if (m_ActiveTelescope != nullptr)
    {
        m_ActiveTelescope->Slew(&target);
        emit scopeStatusChanged(ISD::Telescope::MOUNT_SLEWING);
    }
}

void CaptureDeviceAdaptor::setScopeTracking(bool on)
{
    if (m_ActiveTelescope != nullptr)
    {
        m_ActiveTelescope->setTrackEnabled(on);
        emit scopeStatusChanged(on ? ISD::Telescope::MOUNT_TRACKING : ISD::Telescope::MOUNT_IDLE);
    }
}

void Ekos::CaptureDeviceAdaptor::setScopeParked(bool parked)
{
    if (m_ActiveTelescope != nullptr)
    {
        if (parked == true)
        {
            if (m_ActiveTelescope->Park())
                emit scopeStatusChanged(ISD::Telescope::MOUNT_PARKING);
            else
                emit scopeStatusChanged(ISD::Telescope::MOUNT_ERROR);
        }
        else
        {
            if (m_ActiveTelescope->UnPark() == false)
                emit scopeStatusChanged(ISD::Telescope::MOUNT_ERROR);
        }
    }
}

void Ekos::CaptureDeviceAdaptor::setDomeParked(bool parked)
{
    if (m_ActiveDome != nullptr)
    {
        if (parked == true)
        {
            if (m_ActiveDome->Park())
                emit scopeStatusChanged(ISD::Telescope::MOUNT_PARKING);
            else
                emit scopeStatusChanged(ISD::Telescope::MOUNT_ERROR);
        }
        else
        {
            if (m_ActiveTelescope->UnPark() == false)
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
    if (m_ActiveCamera == nullptr)
    {
        emit hasShutter(false);
        return;
    }
    QStringList shutterfulCCDs  = Options::shutterfulCCDs();
    QStringList shutterlessCCDs = Options::shutterlessCCDs();
    QString deviceName = m_ActiveCamera->getDeviceName();

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
        QString deviceName = m_ActiveCamera->getDeviceName();
        // Yes, has shutter
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
        {
            KSMessageBox::Instance()->disconnect(this);
            QStringList shutterfulCCDs  = Options::shutterfulCCDs();
            shutterfulCCDs.append(m_ActiveCamera->getDeviceName());
            Options::setShutterfulCCDs(shutterfulCCDs);
            emit hasShutter(true);
        });
        // No, has no shutter
        connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
        {
            KSMessageBox::Instance()->disconnect(this);
            QStringList shutterlessCCDs = Options::shutterlessCCDs();
            shutterlessCCDs.append(m_ActiveCamera->getDeviceName());
            Options::setShutterlessCCDs(shutterlessCCDs);
            emit hasShutter(false);
        });

        KSMessageBox::Instance()->questionYesNo(i18n("Does %1 have a shutter?", deviceName),
                                                i18n("Dark Exposure"));
    }
}

} // namespace
