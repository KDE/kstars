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

namespace Ekos
{
class CaptureCommandProcessor: public QObject
{
    Q_OBJECT

public:
    CaptureCommandProcessor();

    //////////////////////////////////////////////////////////////////////
    // Devices
    //////////////////////////////////////////////////////////////////////
    void setLightBox(ISD::LightBox *device);
    void setDustCap(ISD::DustCap *device);
    void setTelescope(ISD::Telescope *device);
    void setDome(ISD::Dome *device);
    //////////////////////////////////////////////////////////////////////
    // Rotator commands
    //////////////////////////////////////////////////////////////////////
    // active rotator device
    ISD::GDInterface * activeRotator { nullptr };

    /**
     * @brief Set the rotator's angle
     */
    void setRotatorAngle(double *rawAngle);

    //////////////////////////////////////////////////////////////////////
    // Camera commands
    //////////////////////////////////////////////////////////////////////
    // active camera device
    ISD::CCD * activeCCD { nullptr };
    // active CCD chip
    ISD::CCDChip * activeChip { nullptr };

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
    // currently active filter wheel device
    ISD::GDInterface * activeFilterWheel { nullptr };
    // Filter Manager
    QSharedPointer<FilterManager> filterManager;
    void setFilterManager(const QSharedPointer<FilterManager> &value) { filterManager = value; }

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

private:
    // the light box device
    ISD::LightBox *m_lightBox { nullptr };
    // the dust cap
    ISD::DustCap *m_dustCap { nullptr };
    // the current telescope
    ISD::Telescope *m_telescope { nullptr };
    // the current dome
    ISD::Dome *m_dome { nullptr };
    // flag if manual cover has been asked
    bool m_ManualCoveringAsked { false };
};
}; // namespace
