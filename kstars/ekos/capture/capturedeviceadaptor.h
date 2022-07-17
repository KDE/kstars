/*  Ekos commands for the capture module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "ekos/ekos.h"
#include "indi/indicommon.h"
#include "indiapi.h"
#include "indi/indiccd.h"
#include "indi/indicap.h"
#include "indi/indidome.h"
#include "indi/indilightbox.h"
#include "indi/inditelescope.h"
#include "ekos/auxiliary/filtermanager.h"

#include "sequencejobstate.h"

namespace Ekos
{
class CaptureDeviceAdaptor: public QObject
{
    Q_OBJECT

public:
    CaptureDeviceAdaptor();

    //////////////////////////////////////////////////////////////////////
    // current sequence job's state machine
    //////////////////////////////////////////////////////////////////////

    SequenceJobState *currentSequenceJobState = nullptr;

    /**
     * @brief Set the state machine for the current sequence job and attach
     *        all active devices to it.
     */
    void setCurrentSequenceJobState(SequenceJobState *jobState);


    //////////////////////////////////////////////////////////////////////
    // Devices
    //////////////////////////////////////////////////////////////////////
    void setLightBox(ISD::LightBox *device);
    ISD::LightBox *getLightBox() { return m_ActiveLightBox; }
    void connectLightBox();
    void disconnectLightBox();

    void setDustCap(ISD::DustCap *device);
    ISD::DustCap *getDustCap() { return m_ActiveDustCap; }
    void connectDustCap();
    void disconnectDustCap();

    void setTelescope(ISD::Telescope *device);
    ISD::Telescope *getTelescope() { return m_ActiveTelescope; }
    void connectTelescope();
    void disconnectTelescope();

    void setDome(ISD::Dome *device);
    ISD::Dome *getDome() { return m_ActiveDome; }
    void connectDome();
    void disconnectDome();

    void setRotator(ISD::GDInterface *device);
    ISD::GDInterface *getRotator() { return m_ActiveRotator; }
    void connectRotator();
    void disconnectRotator();

    void setActiveCCD(ISD::CCD *device);
    ISD::CCD *getActiveCCD() { return m_ActiveCamera; }
    void connectActiveCCD();
    void disconnectActiveCCD();

    void setActiveChip(ISD::CCDChip *device);
    ISD::CCDChip *getActiveChip() { return m_ActiveChip; }
    // void connectActiveChip();
    // void disconnectActiveChip();

    void setFilterWheel(ISD::GDInterface *device);
    ISD::GDInterface *getFilterWheel() { return m_ActiveFilterWheel; }
    // void connectFilterWheel();
    // void disconnectFilterWheel();

    void setFilterManager(const QSharedPointer<FilterManager> &value) { filterManager = value; }
    const QSharedPointer<FilterManager> getFilterManager() { return filterManager; }
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

    /**
     * @brief Check if rotator is currently reversed.
     */
    bool isRotatorReversed();

    /**
     * @brief Slot reading updates from the rotator device
     */
    void updateRotatorNumber(INumberVectorProperty *nvp);

    /**
     * @brief Slot reading updates from the rotator device
     */
    void updateRotatorSwitch(ISwitchVectorProperty *svp);

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
     * @brief Ask for covering the scope manually with a flats light source
     */
    void askManualScopeLightCover(QString question, QString title);
    /**
     * @brief Ask for opening the scope cover manually
     */
    void askManualScopeLightOpen();
    /**
     * @brief Turn light on in the light box
     */
    void setLightBoxLight(bool on);
    /**
     * @brief park the dust cap
     */
    void parkDustCap(bool park);
    /**
     * @brief Turn light on in the dust cap
     */
    void setDustCapLight(bool on);
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
    void newRotatorReversed(bool enabled);
    /**
     * @brief Cover for the scope with a flats light source
     */
    void manualScopeLightCover(bool closed, bool success);
    /**
     * @brief Light box light is on.
     */
    void lightBoxLight(bool on);
    /**
     * @brief dust cap light is on.
     */
    void dustCapLight(bool on);
    /**
     * @brief dust cap status change
     */
    void dustCapStatusChanged(ISD::DustCap::Status status);
    /**
     * @brief telescope status change
     */
    void scopeStatusChanged(ISD::Telescope::Status status);
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
    // the light box device
    ISD::LightBox *m_ActiveLightBox { nullptr };
    // the dust cap
    ISD::DustCap *m_ActiveDustCap { nullptr };
    // the current telescope
    ISD::Telescope *m_ActiveTelescope { nullptr };
    // the current dome
    ISD::Dome *m_ActiveDome { nullptr };
    // active rotator device
    ISD::GDInterface * m_ActiveRotator { nullptr };
    // active camera device
    ISD::CCD * m_ActiveCamera { nullptr };
    // active CCD chip
    ISD::CCDChip * m_ActiveChip { nullptr };
    // currently active filter wheel device
    ISD::GDInterface * m_ActiveFilterWheel { nullptr };
    // Filter Manager
    QSharedPointer<FilterManager> filterManager;

    // flag if manual cover has been asked
    bool m_ManualCoveringAsked { false };
};
}; // namespace
