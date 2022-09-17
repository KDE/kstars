/*  Ekos state machine for the Capture module
    SPDX-FileCopyrightText: 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ekos/ekos.h"
#include "indiapi.h"
#include "indi/indistd.h"
#include "indi/indidustcap.h"
#include "indi/indimount.h"
#include "indi/indidome.h"

#include <QObject>

namespace Ekos
{
class CaptureModuleState: public QObject
{
    Q_OBJECT
public:
    /* Action types to be executed before capturing may start. */
    typedef enum
    {
        ACTION_FILTER,              /* Change the filter and wait until the correct filter is set.                                    */
        ACTION_TEMPERATURE,         /* Set the camera chip target temperature and wait until the target temperature has been reached. */
        ACTION_ROTATOR,             /* Set the camera rotator target angle and wait until the target angle has been reached.          */
        ACTION_GUIDER_DRIFT,        /* Wait until the guiding deviation is below the configured threshold.                            */
        ACTION_PREPARE_LIGHTSOURCE, /* Setup the selected flat lights source.                                                         */
        ACTION_MOUNT_PARK,          /* Park the mount.                                                                                */
        ACTION_DOME_PARK,           /* Park the dome.                                                                                 */
        ACTION_FLAT_SYNC_FOCUS,     /* Move the focuser to the focus position for the selected filter.                                */
        ACTION_SCOPE_COVER          /* Ensure that the scope cover (if present) is opened.                                            */
    } PrepareActions;

    typedef enum {
        SHUTTER_YES,    /* the CCD has a shutter                             */
        SHUTTER_NO,     /* the CCD has no shutter                            */
        SHUTTER_BUSY,   /* determining whether the CCD has a shutter running */
        SHUTTER_UNKNOWN /* unknown whether the CCD has a shutter             */
    } ShutterStatus;

    typedef enum
    {
        CAP_IDLE,
        CAP_PARKING,
        CAP_UNPARKING,
        CAP_PARKED,
        CAP_ERROR,
        CAP_UNKNOWN
    } CapState;

    typedef enum {
        CAP_LIGHT_OFF,     /* light is on               */
        CAP_LIGHT_ON,      /* light is off              */
        CAP_LIGHT_UNKNOWN, /* unknown whether on or off */
        CAP_LIGHT_BUSY     /* light state changing      */
    } LightState;

    CaptureModuleState(QObject *parent = nullptr);

    // ////////////////////////////////////////////////////////////////////
    // capture attributes
    // ////////////////////////////////////////////////////////////////////
    // current filter ID
    int currentFilterID { Ekos::INVALID_VALUE };
    // Map tracking whether the current value has been initialized.
    // With this construct we could do a lazy initialization of current values if setCurrent...()
    // sets this flag to true. This is necessary since we listen to the events, but as long as
    // the value does not change, there won't be an event.
    QMap<PrepareActions, bool> isInitialized;

    // ////////////////////////////////////////////////////////////////////
    // flat preparation attributes
    // ////////////////////////////////////////////////////////////////////
    // flag if telescope has been covered
    bool telescopeCovered { false };
    // flag if there is a light box device
    bool hasLightBox { false };
    // flag if there is a dust cap device
    bool hasDustCap { false };
    // flag if there is a telescope device
    bool hasTelescope { false };
    // flag if there is a dome device
    bool hasDome { false };

    // ////////////////////////////////////////////////////////////////////
    // dark preparation attributes
    // ////////////////////////////////////////////////////////////////////
    ShutterStatus shutterStatus { SHUTTER_UNKNOWN };

    // ////////////////////////////////////////////////////////////////////
    // state accessors
    // ////////////////////////////////////////////////////////////////////
    CaptureState getCaptureState() const { return m_CaptureState; }
    void setCaptureState(CaptureState value) { m_CaptureState = value; }

    FocusState getFocusState() const { return m_FocusState;}
    void setFocusState(FocusState value) { m_FocusState = value; }

    GuideState getGuideState() const { return m_GuideState;}
    void setGuideState(GuideState value) { m_GuideState = value; }

    IPState getDitheringState() const { return m_DitheringState;}
    void setDitheringState(IPState value) { m_DitheringState = value; }

    AlignState getAlignState() const { return m_AlignState;}
    void setAlignState(AlignState value) { m_AlignState = value; }

    FilterState getFilterManagerState() const { return m_FilterManagerState;}
    void setFilterManagerState(FilterState value) { m_FilterManagerState = value; }

    LightState getLightBoxLightState() const { return m_lightBoxLightState;}
    void setLightBoxLightState(LightState value) { m_lightBoxLightState = value; }

    CapState getDustCapState() const { return m_dustCapState;}
    void setDustCapState(CapState value) { m_dustCapState = value; }

    ISD::Mount::Status getScopeState() const { return m_scopeState;}
    void setScopeState(ISD::Mount::Status value) { m_scopeState = value; }

    ISD::ParkStatus getScopeParkState() const { return m_scopeParkState;}
    void setScopeParkState(ISD::ParkStatus value) { m_scopeParkState = value; }

    ISD::Dome::Status getDomeState() const { return m_domeState;}
    void setDomeState(ISD::Dome::Status value) { m_domeState = value; }

private:
    // ////////////////////////////////////////////////////////////////////
    // device states
    // ////////////////////////////////////////////////////////////////////
    CaptureState m_CaptureState { CAPTURE_IDLE };
    FocusState m_FocusState { FOCUS_IDLE };
    GuideState m_GuideState { GUIDE_IDLE };
    IPState m_DitheringState {IPS_IDLE};
    AlignState m_AlignState { ALIGN_IDLE };
    FilterState m_FilterManagerState { FILTER_IDLE };
    LightState m_lightBoxLightState { CAP_LIGHT_UNKNOWN };
    CapState m_dustCapState { CAP_UNKNOWN };
    ISD::Mount::Status m_scopeState { ISD::Mount::MOUNT_IDLE };
    ISD::ParkStatus m_scopeParkState { ISD::PARK_UNKNOWN };
    ISD::Dome::Status m_domeState { ISD::Dome::DOME_IDLE };

};

}; // namespace
