/*  Ekos commands for the capture module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "ekos/ekos.h"
#include "indi/indicommon.h"
#include "indiapi.h"
#include "indi/indicamera.h"
#include "indi/indidustcap.h"
#include "indi/indidome.h"
#include "indi/indilightbox.h"
#include "indi/indimount.h"
#include "indi/indirotator.h"
#include "ekos/auxiliary/filtermanager.h"

#include "sequencejobstate.h"

namespace Ekos
{
class CaptureDeviceAdaptor: public QObject
{
        Q_OBJECT

    public:
        CaptureDeviceAdaptor(QSharedPointer<CaptureModuleState> captureModuleState);

        //////////////////////////////////////////////////////////////////////
        // current sequence job's state machine
        //////////////////////////////////////////////////////////////////////
        /**
         * @brief Set the state machine for the current sequence job and attach
         *        all active devices to it.
         */
        void setCurrentSequenceJobState(SequenceJobState *jobState);


        //////////////////////////////////////////////////////////////////////
        // Devices
        //////////////////////////////////////////////////////////////////////
        void setLightBox(ISD::LightBox *device);
        ISD::LightBox *getLightBox()
        {
            return m_ActiveLightBox;
        }
        void connectLightBox();
        void disconnectLightBox();

        void setDustCap(ISD::DustCap *device);
        ISD::DustCap *getDustCap()
        {
            return m_ActiveDustCap;
        }
        void connectDustCap();
        void disconnectDustCap();

        void setMount(ISD::Mount *device);
        ISD::Mount *getMount()
        {
            return m_ActiveTelescope;
        }
        void connectTelescope();
        void disconnectTelescope();

        void setDome(ISD::Dome *device);
        ISD::Dome *getDome()
        {
            return m_ActiveDome;
        }
        void connectDome();
        void disconnectDome();

        void setRotator(ISD::Rotator *device);
        ISD::Rotator *getRotator()
        {
            return m_ActiveRotator;
        }
        void connectRotator();
        void disconnectRotator();

        void setActiveCamera(ISD::Camera *device);
        ISD::Camera *getActiveCamera()
        {
            return m_ActiveCamera;
        }
        void connectActiveCamera();
        void disconnectActiveCamera();

        void setActiveChip(ISD::CameraChip *device);
        ISD::CameraChip *getActiveChip()
        {
            return m_ActiveChip;
        }

        void setFilterWheel(ISD::FilterWheel *device);
        ISD::FilterWheel *getFilterWheel()
        {
            return m_ActiveFilterWheel;
        }

        void setFilterManager(QSharedPointer<FilterManager> device);
        QSharedPointer<FilterManager> getFilterManager()
        {
            return m_FilterManager;
        }
        void connectFilterManager();
        void disconnectFilterManager();

        //////////////////////////////////////////////////////////////////////
        // Rotator commands
        //////////////////////////////////////////////////////////////////////
        /**
         * @brief Set the rotator's angle
         */
        void setRotatorAngle(double *rawAngle);

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

        //////////////////////////////////////////////////////////////////////
        // Filter wheel commands
        //////////////////////////////////////////////////////////////////////

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
         * @brief Check if the the focuser needs to be moved to the focus position.
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
         * @brief Update for the CCD temperature
         */
        void newCCDTemperatureValue(double value);
        /**
         * @brief Update for the rotator's angle
         */
        void newRotatorAngle(double value, IPState state);
        /**
         * @brief Update for the rotator reverse status
         */
        void rotatorReverseToggled(bool enabled);
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
         * @brief telescope status change
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
        void readCurrentState(Ekos::CaptureState state);

    private:
        // the state machine of the capture module
        QSharedPointer<CaptureModuleState> m_captureModuleState;
        // the state machine for the current sequence job
        SequenceJobState *currentSequenceJobState = nullptr;
        // the light box device
        ISD::LightBox *m_ActiveLightBox { nullptr };
        // the dust cap
        ISD::DustCap *m_ActiveDustCap { nullptr };
        // the current telescope
        ISD::Mount *m_ActiveTelescope { nullptr };
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
};
}; // namespace
