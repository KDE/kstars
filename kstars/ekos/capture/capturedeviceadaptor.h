/*  Ekos commands for the capture module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "ekos/ekos.h"
#include "indi/indicommon.h"
#include "indiapi.h"

#include "indi/indidome.h"
#include "indi/indicamerachip.h"
#include "indi/indidustcap.h"
#include "indi/indimount.h"

#include "ekos/auxiliary/filtermanager.h"

namespace {
class Camera;
class LightBox;
class Rotator;
}

namespace Ekos
{

class SequenceJobState;

class CaptureDeviceAdaptor: public QObject
{
        Q_OBJECT

public:
    CaptureDeviceAdaptor() {}

        //////////////////////////////////////////////////////////////////////
        // current sequence job's state machine
        //////////////////////////////////////////////////////////////////////
        /**
         * @brief Set the state machine for the current sequence job and attach
         *        all active devices to it.
         */
        void setCurrentSequenceJobState(QSharedPointer<SequenceJobState> jobState);


        //////////////////////////////////////////////////////////////////////
        // Devices
        //////////////////////////////////////////////////////////////////////
        void setLightBox(ISD::LightBox *device)
        {
            m_ActiveLightBox = device;
        }
        ISD::LightBox *lightBox()
        {
            return m_ActiveLightBox;
        }

        void setDustCap(ISD::DustCap *device);
        ISD::DustCap *dustCap()
        {
            return m_ActiveDustCap;
        }

        void setMount(ISD::Mount *device);
        ISD::Mount *mount()
        {
            return m_ActiveMount;
        }

        void setDome(ISD::Dome *device);
        ISD::Dome *dome()
        {
            return m_ActiveDome;
        }

        void setRotator(ISD::Rotator *device);
        ISD::Rotator *rotator()
        {
            return m_ActiveRotator;
        }

        void setActiveCamera(ISD::Camera *device);
        ISD::Camera *getActiveCamera()
        {
            return m_ActiveCamera;
        }

        void setActiveChip(ISD::CameraChip *device)
        {
            m_ActiveChip = device;
        }
        ISD::CameraChip *getActiveChip()
        {
            return m_ActiveChip;
        }

        // FIXME add support for guide head, not implemented yet
        void addGuideHead(ISD::Camera *device)
        {
            Q_UNUSED(device)
        }

        void setFilterWheel(ISD::FilterWheel *device);
        ISD::FilterWheel *filterWheel()
        {
            return m_ActiveFilterWheel;
        }

        void clearFilterManager();
        void setFilterManager(const QSharedPointer<FilterManager> &device);
        const QSharedPointer<FilterManager> &getFilterManager() const
        {
            return m_FilterManager;
        }

        /**
         * @brief disconnectDevices Connect all current devices to the job's
         * state machine
         */
        void disconnectDevices(SequenceJobState *state);

        //////////////////////////////////////////////////////////////////////
        // Rotator commands
        //////////////////////////////////////////////////////////////////////
        /**
         * @brief Set the rotator's angle
         */
        void setRotatorAngle(double rawAngle);

        /**
         * @brief Get the rotator's angle
         */
        double getRotatorAngle();

        /**
         * @brief Get the rotator's angle state
         */
        IPState getRotatorAngleState();

        /**
         * @brief reverseRotator Toggle rotation reverse
         * @param toggled If true, reverse rotator normal direction. If false, use rotator normal direction.
         */
        void reverseRotator(bool toggled);

        /**
         * @brief Read the current rotator's angle from the rotator device
         *        and emit it as {@see newRotatorAngle()}
         */
        void readRotatorAngle();

        //////////////////////////////////////////////////////////////////////
        // Camera commands
        //////////////////////////////////////////////////////////////////////

        /**
         * @brief Set the CCD target temperature
         * @param temp
         */
        void setCCDTemperature(double temp);

        /**
         * @brief Set CCD to batch mode
         * @param enable true iff batch mode
         */
        void enableCCDBatchMode(bool enable);

        /**
         * @brief Abort exposure if fast exposure is enabled
         */
        void abortFastExposure();

        /**
         * @brief getGain Retrieve the gain value from the custom property value. Depending
         *        on the camera, it is either stored as GAIN property value of CCD_GAIN or as
         *        Gain property value from CCD_CONTROLS.
         */
        double cameraGain(QMap<QString, QMap<QString, QVariant> > propertyMap);

        /**
         * @brief cameraGain Retrieve the gain value from the active camera
         */
        double cameraGain();

        /**
         * @brief getOffset Retrieve the offset value from the custom property value. Depending
         *        on the camera, it is either stored as OFFSET property value of CCD_OFFSET or as
         *        Offset property value from CCD_CONTROLS.
         */
        double cameraOffset(QMap<QString, QMap<QString, QVariant> > propertyMap);
        /**
         * @brief cameraOffset Retrieve the offset value from the active camera
         */
        double cameraOffset();

        /**
         * @brief cameraTemperature Retrieve the current chip temperature from the active camera
         */
        double cameraTemperature();

        //////////////////////////////////////////////////////////////////////
        // Filter wheel commands
        //////////////////////////////////////////////////////////////////////

        /**
         * @brief Select the filter at the given position
         */
        void setFilterPosition(int targetFilterPosition, FilterManager::FilterPolicy policy = FilterManager::ALL_POLICIES);
        /**
         * @brief updateFilterPosition Inform the sequence job state machine about the current filter position
         */
        void updateFilterPosition();
        /**
         * @brief setFilterChangeFailed Inform the sequence job state machine that filter change operation failed.
         */
        void setFilterChangeFailed();

        //////////////////////////////////////////////////////////////////////
        // Flat capturing commands
        //////////////////////////////////////////////////////////////////////

        /**
         * @brief Ask for covering the scope manually with a flats light source or dark cover
         */
        void askManualScopeCover(QString question, QString title, bool light);
        /**
         * @brief Ask for opening the scope cover manually
         */
        void askManualScopeOpen(bool light);
        /**
         * @brief Turn light on in the light box
         */
        void setLightBoxLight(bool on);
        /**
         * @brief park the dust cap
         */
        void parkDustCap(bool park);
        /**
         * @brief Slew the telescope to a target
         */
        void slewTelescope(SkyPoint &target);
        /**
         * @brief Turn scope tracking on and off
         */
        void setScopeTracking(bool on);
        /**
         * @brief Park / unpark telescope
         */
        void setScopeParked(bool parked);
        /**
         * @brief Park / unpark dome
         */
        void setDomeParked(bool parked);
        /**
         * @brief Check if the focuser needs to be moved to the focus position.
         */
        void flatSyncFocus(int targetFilterID);

        //////////////////////////////////////////////////////////////////////
        // Dark capturing commands
        //////////////////////////////////////////////////////////////////////

        /**
         * @brief Check whether the CCD has a shutter
         */
        void queryHasShutter();

signals:
        /**
         * @brief filterIdChanged Update of the currently selected filter ID
         */
        void filterIdChanged(int id);
        /**
         * @brief newCamera A new camera has been set
         * @param name device name (empty if none)
         */
        void newCamera(QString name);
        /**
         * @brief CameraConnected signal if the camera got connected
         * @param connected is it connected?
         */
        void CameraConnected(bool connected);
        /**
         * @brief Update for the CCD temperature
         */
        void newCCDTemperatureValue(double value);
        /**
         * @brief newRotator A new rotator has been set
         * @param name device name (empty if none)
         */
        void newRotator(QString name);
        /**
         * @brief Update for the rotator's angle
         */
        void newRotatorAngle(double value, IPState state);
        /**
         * @brief Update rotation for calibration in internal guider
         */
        void newPA(const double Angle, const bool FlipRotationDone);
        /**
         * @brief Update for the rotator reverse status
         */
        void rotatorReverseToggled(bool enabled);
        /**
         * @brief newFilterWheel A new filter wheel has been set
         * @param name device name (empty if none)
         */
        void newFilterWheel(QString name);
        /**
         * @brief FilterWheelConnected signal if the filter wheel got connected
         * @param connected is it connected?
         */
        void FilterWheelConnected(bool connected);
        /**
         * @brief Cover for the scope with a flats light source (light is true) or dark (light is false)
         */
        void manualScopeCoverUpdated(bool closed, bool success, bool light);
        /**
         * @brief Light box light is on.
         */
        void lightBoxLight(bool on);
        /**
         * @brief dust cap status change
         */
        void dustCapStatusChanged(ISD::DustCap::Status status);
        /**
         * @brief telescope status change
         */
        void scopeStatusChanged(ISD::Mount::Status status);
        /**
         * @brief telescope pier side change
         */
        void pierSideChanged(ISD::Mount::PierSide pierside);
        /**
         * @brief telescope park status change
         */
        void scopeParkStatusChanged(ISD::ParkStatus status);
        /**
         * @brief dome status change
         */
        void domeStatusChanged(ISD::Dome::Status status);
        /**
         * @brief flat sync focus status change
         */
        void flatSyncFocusChanged(bool completed);
        /**
         * @brief CCD has a shutter
         */
        void hasShutter(bool present);

public slots:
        /**
         * @brief Slot that reads the requested device state and publishes the corresponding event
         * @param state device state that needs to be read directly
         */
        void readCurrentState(CaptureState state);
        /**
         * @brief Slot that reads the current park state of the mount and publishes it.
         */
        void readCurrentMountParkState();

private:
        // the state machine for the current sequence job
        QSharedPointer<SequenceJobState> currentSequenceJobState;
        // the light box device
        ISD::LightBox *m_ActiveLightBox { nullptr };
        // the dust cap
        ISD::DustCap *m_ActiveDustCap { nullptr };
        // the current mount
        ISD::Mount *m_ActiveMount { nullptr };
        // the current dome
        ISD::Dome *m_ActiveDome { nullptr };
        // active rotator device
        ISD::Rotator * m_ActiveRotator { nullptr };
        // active camera device
        ISD::Camera * m_ActiveCamera { nullptr };
        // active CCD chip
        ISD::CameraChip * m_ActiveChip { nullptr };
        // currently active filter wheel device
        ISD::FilterWheel * m_ActiveFilterWheel { nullptr };
        // currently active filter manager
        QSharedPointer<FilterManager> m_FilterManager;

        // flag if light manual cover has been asked
        bool m_ManualLightCoveringAsked { false };
        bool m_ManualLightOpeningAsked { false };

        // flag if dark manual cover has been asked
        bool m_ManualDarkCoveringAsked { false };
        bool m_ManualDarkOpeningAsked { false };

        //////////////////////////////////////////////////////////////////////
        // Device Connections
        //////////////////////////////////////////////////////////////////////
        void connectDustCap(SequenceJobState *state);
        void disconnectDustCap(SequenceJobState *state);
        void connectMount(SequenceJobState *state);
        void disconnectMount(SequenceJobState *state);
        void connectDome(SequenceJobState *state);
        void disconnectDome(SequenceJobState *state);
        void connectRotator(SequenceJobState *state);
        void disconnectRotator(SequenceJobState *state);
        void connectActiveCamera(SequenceJobState *state);
        void disconnectActiveCamera(SequenceJobState *state);
        void connectFilterManager(SequenceJobState *state);
        void disconnectFilterManager(SequenceJobState *state);
};
}; // namespace
