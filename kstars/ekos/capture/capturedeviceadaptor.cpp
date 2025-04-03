/*  Ekos commands for the capture module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturedeviceadaptor.h"

#include "ksmessagebox.h"
#include "Options.h"
#include "indi/indistd.h"
#include "ekos_capture_debug.h"
#include "sequencejobstate.h"

#include "indi/indicamera.h"
#include "indi/indidustcap.h"
#include "indi/indidome.h"
#include "indi/indilightbox.h"
#include "indi/indimount.h"
#include "indi/indirotator.h"
#include "ekos/auxiliary/rotatorutils.h"
#include "ksnotification.h"

namespace Ekos
{


void CaptureDeviceAdaptor::connectDome(SequenceJobState *state)
{
    if (state == nullptr)
        return;

    connect(state, &SequenceJobState::setDomeParked, this, &CaptureDeviceAdaptor::setDomeParked);
    connect(this, &CaptureDeviceAdaptor::domeStatusChanged, state, &SequenceJobState::domeStatusChanged);
}

void CaptureDeviceAdaptor::disconnectDome(SequenceJobState *state)
{
    if (state == nullptr)
        return;

    disconnect(state, &SequenceJobState::setDomeParked, this, &CaptureDeviceAdaptor::setDomeParked);
    disconnect(this, &CaptureDeviceAdaptor::domeStatusChanged, state, &SequenceJobState::domeStatusChanged);
}

void CaptureDeviceAdaptor::setCurrentSequenceJobState(QSharedPointer<SequenceJobState> jobState)
{
    // clear old connections
    disconnectDevices(currentSequenceJobState.data());
    // add new connections
    connectRotator(jobState.data());
    connectFilterManager(jobState.data());
    connectActiveCamera(jobState.data());
    connectMount(jobState.data());
    connectDome(jobState.data());
    connectDustCap(jobState.data());
    currentSequenceJobState = jobState;

}

void CaptureDeviceAdaptor::setDustCap(ISD::DustCap *device)
{
    disconnectDustCap(currentSequenceJobState.data());
    m_ActiveDustCap = device;
    connectDustCap(currentSequenceJobState.data());
}

void CaptureDeviceAdaptor::disconnectDevices(SequenceJobState *state)
{
    disconnectRotator(state);
    disconnectFilterManager(state);
    disconnectActiveCamera(state);
    disconnectMount(state);
    disconnectDome(state);
    disconnectDustCap(state);
}





void CaptureDeviceAdaptor::connectDustCap(SequenceJobState *state)
{
    if (m_ActiveDustCap != nullptr)
        connect(m_ActiveDustCap, &ISD::DustCap::newStatus, this, &CaptureDeviceAdaptor::dustCapStatusChanged);

    if (state == nullptr)
        return;

    connect(state, &SequenceJobState::askManualScopeCover, this, &CaptureDeviceAdaptor::askManualScopeCover);
    connect(state, &SequenceJobState::askManualScopeOpen, this, &CaptureDeviceAdaptor::askManualScopeOpen);
    connect(state, &SequenceJobState::setLightBoxLight, this, &CaptureDeviceAdaptor::setLightBoxLight);
    connect(state, &SequenceJobState::parkDustCap, this, &CaptureDeviceAdaptor::parkDustCap);

    connect(this, &CaptureDeviceAdaptor::manualScopeCoverUpdated, state, &SequenceJobState::updateManualScopeCover);
    connect(this, &CaptureDeviceAdaptor::lightBoxLight, state, &SequenceJobState::lightBoxLight);
    connect(this, &CaptureDeviceAdaptor::dustCapStatusChanged, state, &SequenceJobState::dustCapStateChanged);
}

void CaptureDeviceAdaptor::disconnectDustCap(SequenceJobState *state)
{
    if (m_ActiveDustCap != nullptr)
        disconnect(m_ActiveDustCap, nullptr, this, nullptr);

    if (state == nullptr)
        return;

    disconnect(state, &SequenceJobState::askManualScopeCover, this, &CaptureDeviceAdaptor::askManualScopeCover);
    disconnect(state, &SequenceJobState::askManualScopeOpen, this, &CaptureDeviceAdaptor::askManualScopeOpen);
    disconnect(state, &SequenceJobState::setLightBoxLight, this, &CaptureDeviceAdaptor::setLightBoxLight);
    disconnect(state, &SequenceJobState::parkDustCap, this, &CaptureDeviceAdaptor::parkDustCap);

    disconnect(this, &CaptureDeviceAdaptor::manualScopeCoverUpdated, state, &SequenceJobState::updateManualScopeCover);
    disconnect(this, &CaptureDeviceAdaptor::lightBoxLight, state, &SequenceJobState::lightBoxLight);
    disconnect(this, &CaptureDeviceAdaptor::dustCapStatusChanged, state, &SequenceJobState::dustCapStateChanged);
}

void CaptureDeviceAdaptor::setMount(ISD::Mount *device)
{
    if (m_ActiveMount == device)
        return;

    // clean up old connections
    if (m_ActiveMount != nullptr)
    {
        disconnect(m_ActiveMount, nullptr, this, nullptr);
        disconnectMount(currentSequenceJobState.data());
    }
    // connect new device
    if (device != nullptr)
    {
        connect(device, &ISD::Mount::newStatus, this, &CaptureDeviceAdaptor::scopeStatusChanged);
        connect(device, &ISD::Mount::pierSideChanged, this, &CaptureDeviceAdaptor::pierSideChanged);
        connect(device, &ISD::Mount::newParkStatus, this, &CaptureDeviceAdaptor::scopeParkStatusChanged);
        connectMount(currentSequenceJobState.data());

        // update mount states
        emit pierSideChanged(device->pierSide());
        emit scopeParkStatusChanged(device->parkStatus());
    }

    m_ActiveMount = device;
}

void CaptureDeviceAdaptor::connectMount(SequenceJobState *state)
{
    if (state == nullptr)
        return;

    connect(state, &SequenceJobState::slewTelescope, this, &CaptureDeviceAdaptor::slewTelescope);
    connect(state, &SequenceJobState::setScopeTracking, this, &CaptureDeviceAdaptor::setScopeTracking);
    connect(state, &SequenceJobState::readCurrentMountParkState, this, &CaptureDeviceAdaptor::readCurrentMountParkState);
    connect(state, &SequenceJobState::setScopeParked, this, &CaptureDeviceAdaptor::setScopeParked);

    connect(this, &CaptureDeviceAdaptor::scopeStatusChanged, state, &SequenceJobState::scopeStatusChanged);
    connect(this, &CaptureDeviceAdaptor::scopeParkStatusChanged, state, &SequenceJobState::scopeParkStatusChanged);
}

void CaptureDeviceAdaptor::disconnectMount(SequenceJobState *state)
{
    if (state == nullptr)
        return;

    disconnect(state, &SequenceJobState::slewTelescope, this, &CaptureDeviceAdaptor::slewTelescope);
    disconnect(state, &SequenceJobState::setScopeTracking, this, &CaptureDeviceAdaptor::setScopeTracking);
    disconnect(state, &SequenceJobState::setScopeParked, this, &CaptureDeviceAdaptor::setScopeParked);

    disconnect(this, &CaptureDeviceAdaptor::scopeStatusChanged, state, &SequenceJobState::scopeStatusChanged);
    disconnect(this, &CaptureDeviceAdaptor::scopeParkStatusChanged, state, &SequenceJobState::scopeParkStatusChanged);
}

void CaptureDeviceAdaptor::setDome(ISD::Dome *device)
{
    if (m_ActiveDome == device)
        return;

    // clean up old connections
    if (m_ActiveDome != nullptr)
    {
        disconnect(m_ActiveDome, nullptr, this, nullptr);
        disconnectDome(currentSequenceJobState.data());
    }
    // connect new device
    if (device != nullptr)
    {
        connect(device, &ISD::Dome::newStatus, this, &CaptureDeviceAdaptor::domeStatusChanged);
        connectDome(currentSequenceJobState.data());
    }

    m_ActiveDome = device;
}

void CaptureDeviceAdaptor::setRotator(ISD::Rotator *device)
{
    // do nothing if *real* rotator is already connected
    if ((m_ActiveRotator == device) && (device != nullptr))
        return;

    // clean up old connections
    if (m_ActiveRotator != nullptr)
    {
        m_ActiveRotator->disconnect(this);
        disconnectRotator(currentSequenceJobState.data());
    }

    m_ActiveRotator = device;

    // connect new device
    if (m_ActiveRotator != nullptr)
    {
        connect(m_ActiveRotator, &ISD::Rotator::newAbsoluteAngle, this, &CaptureDeviceAdaptor::newRotatorAngle,
                Qt::UniqueConnection);
        connect(m_ActiveRotator, &ISD::Rotator::reverseToggled, this, &CaptureDeviceAdaptor::rotatorReverseToggled,
                Qt::UniqueConnection);
        connectRotator(currentSequenceJobState.data());

        emit newRotator(device->getDeviceName());
    }
    else
        emit newRotator(""); // no real rotator present, so check if user wants to use "manual rotator"

}

void CaptureDeviceAdaptor::connectRotator(SequenceJobState *state)
{
    if (state == nullptr)
        return;

    connect(this, &CaptureDeviceAdaptor::newRotatorAngle, state, &SequenceJobState::setCurrentRotatorPositionAngle,
            Qt::UniqueConnection);
    connect(state, &SequenceJobState::setRotatorAngle, this, &CaptureDeviceAdaptor::setRotatorAngle);
}

void CaptureDeviceAdaptor::disconnectRotator(SequenceJobState *state)
{
    if (state == nullptr)
        return;

    disconnect(state, &SequenceJobState::setRotatorAngle, this, &CaptureDeviceAdaptor::setRotatorAngle);
    disconnect(this, &CaptureDeviceAdaptor::newRotatorAngle, state, &SequenceJobState::setCurrentRotatorPositionAngle);
}

void CaptureDeviceAdaptor::setRotatorAngle(double rawAngle)
{
    if (m_ActiveRotator != nullptr && m_ActiveRotator->setAbsoluteAngle(rawAngle))
    {
        RotatorUtils::Instance()->initTimeFrame(rawAngle);
        qCInfo(KSTARS_EKOS_CAPTURE) << "Setting rotator angle to" << rawAngle << "degrees.";
    }
    else
        qCWarning(KSTARS_EKOS_CAPTURE) << "Setting rotator angle to " << rawAngle
                                       << "failed due to missing or unresponsive rotator.";
}

double CaptureDeviceAdaptor::getRotatorAngle()
{
    if (m_ActiveRotator != nullptr)
        return m_ActiveRotator->absoluteAngle();
    else
        return 0;
}

IPState CaptureDeviceAdaptor::getRotatorAngleState()
{
    if (m_ActiveRotator != nullptr)
        return m_ActiveRotator->absoluteAngleState();
    else
        return IPS_ALERT;
}

void CaptureDeviceAdaptor::reverseRotator(bool toggled)
{
    if (m_ActiveRotator != nullptr)
    {
        m_ActiveRotator->setReversed(toggled);
        m_ActiveRotator->setConfig(SAVE_CONFIG);
    }
}

void CaptureDeviceAdaptor::readRotatorAngle()
{
    if (m_ActiveRotator != nullptr)
        emit newRotatorAngle(m_ActiveRotator->absoluteAngle(), m_ActiveRotator->absoluteAngleState());
}



void CaptureDeviceAdaptor::setActiveCamera(ISD::Camera *device)
{
    if (m_ActiveCamera == device)
        return;

    // disconnect device events if the new device is not empty
    if (m_ActiveCamera != nullptr)
    {
        m_ActiveCamera->disconnect(this);
        disconnectActiveCamera(currentSequenceJobState.data());

    }

    // store the link to the new device
    m_ActiveCamera = device;

    // connect device events if the new device is not empty
    if (m_ActiveCamera != nullptr)
    {
        // publish device events
        connect(m_ActiveCamera, &ISD::Camera::newTemperatureValue, this,
                &CaptureDeviceAdaptor::newCCDTemperatureValue, Qt::UniqueConnection);
        connect(m_ActiveCamera, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            emit CameraConnected(true);
        });
        connect(m_ActiveCamera, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            emit CameraConnected(false);
        });

        if (m_ActiveCamera->hasGuideHead())
            addGuideHead(device);
    }
    connectActiveCamera(currentSequenceJobState.data());

    // communicate new camera
    emit newCamera(device == nullptr ? "" : device->getDeviceName());
}

void CaptureDeviceAdaptor::setFilterWheel(ISD::FilterWheel *device)
{
    if (m_ActiveFilterWheel == device)
        return;

    // disconnect device events if the new device is not empty
    if (m_ActiveFilterWheel != nullptr)
        m_ActiveFilterWheel->disconnect(this);

    // store the link to the new device
    m_ActiveFilterWheel = device;

    // connect device events if the new device is not empty
    if (m_ActiveFilterWheel != nullptr)
    {
        connect(m_ActiveFilterWheel, &ISD::ConcreteDevice::Connected, this, [this]()
        {
            emit FilterWheelConnected(true);
        });
        connect(m_ActiveFilterWheel, &ISD::ConcreteDevice::Disconnected, this, [this]()
        {
            emit FilterWheelConnected(false);
        });
    }

    // communicate new device
    emit newFilterWheel(device == nullptr ? "" : device->getDeviceName());
}

void CaptureDeviceAdaptor::connectActiveCamera(SequenceJobState *state)
{
    if (state == nullptr)
        return;

    //connect state machine to device adaptor
    connect(state, &SequenceJobState::setCCDTemperature, this, &CaptureDeviceAdaptor::setCCDTemperature);
    connect(state, &SequenceJobState::setCCDBatchMode, this, &CaptureDeviceAdaptor::enableCCDBatchMode);
    connect(state, &SequenceJobState::queryHasShutter, this, &CaptureDeviceAdaptor::queryHasShutter);

    // forward own events to the state machine
    connect(this, &CaptureDeviceAdaptor::flatSyncFocusChanged, state, &SequenceJobState::flatSyncFocusChanged);
    connect(this, &CaptureDeviceAdaptor::hasShutter, state, &SequenceJobState::hasShutter);
    connect(this, &CaptureDeviceAdaptor::newCCDTemperatureValue, state, &SequenceJobState::setCurrentCCDTemperature,
            Qt::UniqueConnection);
}

void CaptureDeviceAdaptor::disconnectActiveCamera(SequenceJobState *state)
{
    if (state == nullptr)
        return;

    disconnect(state, &SequenceJobState::setCCDTemperature, this, &CaptureDeviceAdaptor::setCCDTemperature);
    disconnect(state, &SequenceJobState::setCCDBatchMode, this, &CaptureDeviceAdaptor::enableCCDBatchMode);
    disconnect(state, &SequenceJobState::queryHasShutter, this, &CaptureDeviceAdaptor::queryHasShutter);

    disconnect(this, &CaptureDeviceAdaptor::flatSyncFocusChanged, state, &SequenceJobState::flatSyncFocusChanged);
    disconnect(this, &CaptureDeviceAdaptor::hasShutter, state, &SequenceJobState::hasShutter);
    disconnect(this, &CaptureDeviceAdaptor::newCCDTemperatureValue, state, &SequenceJobState::setCurrentCCDTemperature);
}

void CaptureDeviceAdaptor::connectFilterManager(SequenceJobState *state)
{
    if (state == nullptr)
        return;

    connect(state, &SequenceJobState::changeFilterPosition, this, &CaptureDeviceAdaptor::setFilterPosition);
    connect(state, &SequenceJobState::readFilterPosition, this, &CaptureDeviceAdaptor::updateFilterPosition);
    // JEE Make this signal a QueuedConnection to avoid a loop between Capture and FilterManager. This loop can occur where
    // Capture thinks a filter change or Autofocus is required so signals FilterManager. FilterManager then determines what
    // is required and if its nothing then immediately signals done to Capture.
    connect(this, &CaptureDeviceAdaptor::filterIdChanged, state, &SequenceJobState::setCurrentFilterID, Qt::QueuedConnection);

    if (m_FilterManager.isNull() == false)
        connect(m_FilterManager.get(), &FilterManager::newStatus, state, &SequenceJobState::setFilterStatus);
}

void CaptureDeviceAdaptor::disconnectFilterManager(SequenceJobState *state)
{
    if (state == nullptr)
        return;

    disconnect(state, &SequenceJobState::readFilterPosition, this, &CaptureDeviceAdaptor::updateFilterPosition);
    disconnect(state, &SequenceJobState::changeFilterPosition, this, &CaptureDeviceAdaptor::setFilterPosition);
    disconnect(this, &CaptureDeviceAdaptor::filterIdChanged, state, &SequenceJobState::setCurrentFilterID);

    if (m_FilterManager.isNull() == false)
        disconnect(m_FilterManager.get(), &FilterManager::newStatus, state, &SequenceJobState::setFilterStatus);
}

void Ekos::CaptureDeviceAdaptor::updateFilterPosition()
{
    if (m_FilterManager.isNull())
    {
        qCritical(KSTARS_EKOS_CAPTURE) << "Filter manager is not initialized yet. Filter wheel missing from train?";
        KSNotification::event(QLatin1String("CaptureFailed"),
                              i18n("Filter manager is not initilized yet. Filter wheel missing from train?"), KSNotification::Capture,
                              KSNotification::Alert);
        emit filterIdChanged(-1);
    }
    else
        emit filterIdChanged(m_FilterManager->getFilterPosition());
}

void Ekos::CaptureDeviceAdaptor::setFilterChangeFailed()
{
    qWarning(KSTARS_EKOS_CAPTURE) << "Failed to change filter wheel to target position!";
    emit filterIdChanged(-1);
}

void CaptureDeviceAdaptor::readCurrentState(CaptureState state)
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

void CaptureDeviceAdaptor::readCurrentMountParkState()
{
    if (m_ActiveMount != nullptr)
        emit scopeParkStatusChanged(m_ActiveMount->parkStatus());
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

void CaptureDeviceAdaptor::abortFastExposure()
{
    if (m_ActiveCamera != nullptr && m_ActiveChip != nullptr && m_ActiveCamera->isFastExposureEnabled())
        m_ActiveChip->abortExposure();
}

double CaptureDeviceAdaptor::cameraGain(QMap<QString, QMap<QString, QVariant> > propertyMap)
{
    if (getActiveCamera())
    {
        // derive attributes from active camera
        if (getActiveCamera()->getProperty("CCD_GAIN"))
            return propertyMap["CCD_GAIN"].value("GAIN", -1).toDouble();
        else if (getActiveCamera()->getProperty("CCD_CONTROLS"))
            return propertyMap["CCD_CONTROLS"].value("Gain", -1).toDouble();
    }
    else // no active camera set, e.g. whien used from the scheduler
    {
        // if camera is unknown, use the custom property that is set
        if (propertyMap.keys().contains("CCD_GAIN"))
            return propertyMap["CCD_GAIN"].value("GAIN", -1).toDouble();
        else if(propertyMap.keys().contains("CCD_CONTROLS"))
            return propertyMap["CCD_CONTROLS"].value("Gain", -1).toDouble();
    }

    // none found
    return -1;

}

double CaptureDeviceAdaptor::cameraGain()
{
    double value = INVALID_VALUE;
    if (getActiveCamera() != nullptr)
        getActiveCamera()->getGain(&value);

    return value;
}

double CaptureDeviceAdaptor::cameraOffset(QMap<QString, QMap<QString, QVariant> > propertyMap)
{
    if (getActiveCamera())
    {
        if (getActiveCamera()->getProperty("CCD_OFFSET"))
            return propertyMap["CCD_OFFSET"].value("OFFSET", -1).toDouble();
        else if (getActiveCamera()->getProperty("CCD_CONTROLS"))
            return propertyMap["CCD_CONTROLS"].value("Offset", -1).toDouble();
    }
    else
    {
        // if camera is unknown, use the custom property that is set
        if (propertyMap.keys().contains("CCD_OFFSET"))
            return propertyMap["CCD_OFFSET"].value("OFFSET", -1).toDouble();
        else if(propertyMap.keys().contains("CCD_CONTROLS"))
            return propertyMap["CCD_CONTROLS"].value("Offset", -1).toDouble();
    }
    return -1;
}

double CaptureDeviceAdaptor::cameraOffset()
{
    double value = INVALID_VALUE;
    if (getActiveCamera() != nullptr)
        getActiveCamera()->getOffset(&value);

    return value;
}

double CaptureDeviceAdaptor::cameraTemperature()
{
    double value = INVALID_VALUE;
    if (getActiveCamera() != nullptr)
        getActiveCamera()->getTemperature(&value);

    return value;
}

void CaptureDeviceAdaptor::setFilterPosition(int targetFilterPosition, FilterManager::FilterPolicy policy)
{
    if (m_FilterManager.isNull() == false && m_ActiveFilterWheel != nullptr)
        m_FilterManager->setFilterPosition(targetFilterPosition, policy);
}

void CaptureDeviceAdaptor::clearFilterManager()
{
    m_FilterManager.clear();
}

void CaptureDeviceAdaptor::setFilterManager(const QSharedPointer<FilterManager> &device)
{
    // avoid doubled definition
    if (m_FilterManager == device)
        return;

    // disconnect old filter manager
    if (m_FilterManager.isNull() == false)
    {
        disconnect(m_FilterManager.get(), &FilterManager::ready, this, &CaptureDeviceAdaptor::updateFilterPosition);
        disconnectFilterManager(currentSequenceJobState.data());
    }
    //connect new filter manager
    if (device.isNull() == false)
    {
        connect(device.get(), &FilterManager::ready, this, &CaptureDeviceAdaptor::updateFilterPosition);
        connectFilterManager(currentSequenceJobState.data());
    }

    m_FilterManager = device;
}

void CaptureDeviceAdaptor::askManualScopeCover(QString question, QString title, bool light)
{
    // do not ask again
    if (light && m_ManualLightCoveringAsked == true)
    {
        emit manualScopeCoverUpdated(true, true, true);
        return;
    }
    else if (!light && m_ManualDarkCoveringAsked == true)
    {
        emit manualScopeCoverUpdated(true, true, false);
        return;
    }

    // Continue
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, light]()
    {
        emit manualScopeCoverUpdated(true, true, light);
        KSMessageBox::Instance()->disconnect(this);
        m_ManualLightCoveringAsked = false;
        m_ManualLightOpeningAsked = false;
        m_ManualDarkCoveringAsked = false;
        m_ManualDarkOpeningAsked = false;
        if (light)
            m_ManualLightCoveringAsked = true;
        else
            m_ManualDarkCoveringAsked = true;
    });

    // Cancel
    connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this, light]()
    {
        if (light)
            m_ManualLightCoveringAsked = false;
        else
            m_ManualDarkCoveringAsked = false;

        emit manualScopeCoverUpdated(true, false, light);
        KSMessageBox::Instance()->disconnect(this);
    });

    KSMessageBox::Instance()->warningContinueCancel(question, title, Options::manualCoverTimeout());

}

void CaptureDeviceAdaptor::askManualScopeOpen(bool light)
{
    // do not ask again
    if (light && m_ManualLightOpeningAsked == true)
    {
        emit manualScopeCoverUpdated(false, true, true);
        return;
    }
    else if (!light && m_ManualDarkOpeningAsked == true)
    {
        emit manualScopeCoverUpdated(false, true, false);
        return;
    }

    // Continue
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, light]()
    {
        m_ManualLightCoveringAsked = false;
        m_ManualLightOpeningAsked = false;
        m_ManualDarkCoveringAsked = false;
        m_ManualDarkOpeningAsked = false;

        if (light)
            m_ManualLightOpeningAsked = true;
        else
            m_ManualDarkOpeningAsked = true;

        emit manualScopeCoverUpdated(false, true, light);
        KSMessageBox::Instance()->disconnect(this);
    });

    // Cancel
    connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this, light]()
    {
        if (light)
            m_ManualLightOpeningAsked = false;
        else
            m_ManualDarkOpeningAsked = false;
        emit manualScopeCoverUpdated(false, false, light);
        KSMessageBox::Instance()->disconnect(this);
    });

    KSMessageBox::Instance()->warningContinueCancel(i18n("Remove cover from the telescope in order to continue."),
            i18n("Telescope Covered"), Options::manualCoverTimeout());

}

void CaptureDeviceAdaptor::setLightBoxLight(bool on)
{
    m_ActiveLightBox->setLightEnabled(on);
    emit lightBoxLight(on);
}

void CaptureDeviceAdaptor::parkDustCap(bool park)
{
    // park
    if (park == true)
        if (m_ActiveDustCap->park())
            emit dustCapStatusChanged(ISD::DustCap::CAP_PARKING);
        else
            emit dustCapStatusChanged(ISD::DustCap::CAP_ERROR);
    // unpark
    else if (m_ActiveDustCap->unpark())
        emit dustCapStatusChanged(ISD::DustCap::CAP_UNPARKING);
    else
        emit dustCapStatusChanged(ISD::DustCap::CAP_ERROR);
}

void CaptureDeviceAdaptor::slewTelescope(SkyPoint &target)
{
    if (m_ActiveMount != nullptr)
    {
        m_ActiveMount->Slew(&target);
        emit scopeStatusChanged(ISD::Mount::MOUNT_SLEWING);
    }
}

void CaptureDeviceAdaptor::setScopeTracking(bool on)
{
    if (m_ActiveMount != nullptr)
    {
        m_ActiveMount->setTrackEnabled(on);
        emit scopeStatusChanged(on ? ISD::Mount::MOUNT_TRACKING : ISD::Mount::MOUNT_IDLE);
    }
}

void CaptureDeviceAdaptor::setScopeParked(bool parked)
{
    if (m_ActiveMount != nullptr)
    {
        if (parked == true)
        {
            if (m_ActiveMount->park())
                emit scopeStatusChanged(ISD::Mount::MOUNT_PARKING);
            else
                emit scopeStatusChanged(ISD::Mount::MOUNT_ERROR);
        }
        else
        {
            if (m_ActiveMount->unpark() == false)
                emit scopeStatusChanged(ISD::Mount::MOUNT_ERROR);
        }
    }
}

void CaptureDeviceAdaptor::setDomeParked(bool parked)
{
    if (m_ActiveDome != nullptr)
    {
        if (parked == true)
        {
            if (m_ActiveDome->park())
                emit domeStatusChanged(ISD::Dome::DOME_PARKING);
            else
                emit domeStatusChanged(ISD::Dome::DOME_ERROR);
        }
        else
        {
            if (m_ActiveDome->unpark() == false)
                emit domeStatusChanged(ISD::Dome::DOME_ERROR);
        }
    }

}

void CaptureDeviceAdaptor::flatSyncFocus(int targetFilterID)
{
    if (m_FilterManager.isNull() || m_FilterManager->syncAbsoluteFocusPosition(targetFilterID - 1))
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
