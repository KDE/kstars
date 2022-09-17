/*  Ekos state machine for a single capture job sequence.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indicommon.h"
#include "skypoint.h"
#include "capturemodulestate.h"

#include <QWidget>
#include <QVector>
#include <QMap>

namespace Ekos
{
/* Status of a single {@see SequenceJob}. */
typedef enum {
    JOB_IDLE,     /* Initial state, nothing happens. */
    JOB_BUSY,     /* Job is running.                 */
    JOB_ERROR,    /* Error occured, unresolved.      */
    JOB_ABORTED,  /* Job stopped before completion.  */
    JOB_DONE      /* Job completed.                  */
} JOBStatus;

/* Result when starting to capture {@see SequenceJob::capture(bool, FITSMode)}. */
typedef enum
{
    CAPTURE_OK,               /* Starting a new capture succeeded.                                          */
    CAPTURE_FRAME_ERROR,      /* Setting frame parameters failed, capture not started.                      */
    CAPTURE_BIN_ERROR,        /* Setting binning parameters failed, capture not started.                    */
    CAPTURE_FILTER_BUSY,      /* Filter change needed and not completed, capture not started yet.           */
    CAPTURE_FOCUS_ERROR,      /* NOT USED.                                                                  */
    CAPTURE_GUIDER_DRIFT_WAIT /* Waiting until guide drift is below the threshold, capture not started yet. */
} CAPTUREResult;

class SequenceJobState: public QObject
{
    Q_OBJECT

    friend class SequenceJob;
    friend class CaptureDeviceAdaptor;
    // Fixme: too many friends
    friend class Capture;

public:
    typedef enum
    {
        CAL_NONE,                    /* initial state */
        CAL_DUSTCAP_PARKING,         /* unused        */
        CAL_DUSTCAP_PARKED,          /* unused        */
        CAL_LIGHTBOX_ON,             /* unused        */
        CAL_SLEWING,                 /* unused        */
        CAL_SLEWING_COMPLETE,        /* unused        */
        CAL_MOUNT_PARKING,           /* unused        */
        CAL_MOUNT_PARKED,            /* unused        */
        CAL_DOME_PARKING,            /* unused        */
        CAL_DOME_PARKED,             /* unused        */
        CAL_PRECAPTURE_COMPLETE,     /* unused        */
        CAL_CALIBRATION,
        CAL_CALIBRATION_COMPLETE,
        CAL_CAPTURING
    } CalibrationStage;

    typedef enum {
        PREP_NONE,      /* preparation has not been executed */
        PREP_BUSY,      /* preparation started               */
        PREP_COMPLETED, /* preparation completed             */
        PREP_ABORTED    /* preparation aborted (UNUSED)      */
    } PreparationState;

    typedef enum
    {
        CAL_CHECK_TASK,
        CAL_CHECK_CONFIRMATION,
    } CalibrationCheckType;

    typedef enum {
        WP_NONE,           /* slewing to wall position not started */
        WP_SLEWING,        /* slewing to wall position started     */
        WP_SLEW_COMPLETED, /* wall position reached                */
        WP_TRACKING_BUSY,  /* turning tracking off running         */
        WP_TRACKING_OFF    /* wall position reached, tracking off  */
    } ScopeWallPositionStatus;

    typedef enum {    /* synching the focuser to the focus position */
        FS_NONE,      /* not started                                */
        FS_BUSY,      /* running                                    */
        FS_COMPLETED  /* completed                                  */
    } FlatSyncStatus;

    SequenceJobState(const QSharedPointer<CaptureModuleState> &sharedState);

    /**
     * @brief Initialize the state machine.
     * @param frameType frame type for which the preparation should be done
     */
    void setFrameType(CCDFrameType frameType);

    /**
     * @brief Trigger all peparation actions before a capture may be started.
     * @param enforceCCDTemp flag if the CCD temperature should be set to the target value.
     * @param enforceInitialGuidingDrift flag if the initial guiding drift must be below the target value.
     * @param isPreview flag if the captures are in the preview mode
     */
    void prepareLightFrameCapture(bool enforceCCDTemp, bool enforceInitialGuidingDrift, bool isPreview);

    /**
     * @brief Initiate tasks required so that capturing of flats may start.
     * @param enforceCCDTemp flag if the CCD temperature should be set to the target value.
     * @param isPreview flag if the captures are in the preview mode
     */
    void prepareFlatFrameCapture(bool enforceCCDTemp, bool isPreview);

    /**
     * @brief Initiate tasks required so that capturing of darks may start.
     * @param enforceCCDTemp flag if the CCD temperature should be set to the target value.
     * @param isPreview flag if the captures are in the preview mode
     */
    void prepareDarkFrameCapture(bool enforceCCDTemp, bool isPreview);

    /**
     * @brief Initiate tasks required so that capturing of bias may start.
     * @param enforceCCDTemp flag if the CCD temperature should be set to the target value.
     * @param isPreview flag if the captures are in the preview mode
     */
    void prepareBiasFrameCapture(bool enforceCCDTemp, bool isPreview);

    /**
     * @brief Set the flag if a maximal initial guiding deviation is required
     */
    void setEnforceInitialGuidingDrift(bool enforceInitialGuidingDrift);

    /**
     * @brief The current capture sequence job status
     */
    JOBStatus getStatus() { return m_status; }

    /**
     * @brief Preparation state of the sequence job
     */
    PreparationState getPreparationState() const;

    /**
     * @brief Reset the status to a dedicated value.
     * @param status new status, by default {@see JOB_IDLE}
     */
    void reset(JOBStatus status = JOB_IDLE) { m_status = status; }

public slots:
    //////////////////////////////////////////////////////////////////////
    // Slots for device events that change the state.
    //////////////////////////////////////////////////////////////////////

    /**
     * @brief Update the currently active filter ID
     */
    void setCurrentFilterID(int value);

    /**
     * @brief Update the current CCD temperature
     */
    void setCurrentCCDTemperature(double value);
    /**
     * @brief Set the target CCD temperature
     */
    void setTargetCCDTemperature(double value) { targetTemperature = value; }
    /**
     * @brief Update the current guiding deviation.
     */
    void setCurrentGuiderDrift(double value);
    /**
     * @brief Set the target guiding deviation when capture starts
     */
    void setTargetStartGuiderDrift(double value) { targetStartGuiderDrift = value; }

    /**
     * @brief Update the current rotator position (calculated from the rotator angle)
     */
    void setCurrentRotatorPositionAngle(double currentRotation, IPState state);
    /**
     * @brief Set the target rotation angle
     */
    void setTargetRotatorAngle(double value) { targetPositionAngle = value; }

    /**
     * @brief Cover for the scope with a flats light source done
     */
    void manualScopeLightCover(bool closed, bool success);
    /**
     * @brief Light box light is on.
     */
    void lightBoxLight(bool on);
    /**
     * @brief dust cap status change
     */
    void dustCapStateChanged(ISD::DustCap::Status status);
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

signals:
    // communicate that a preparation step needs to be executed
    void prepareState(Ekos::CaptureState state);
    // ask for the current device state
    void readCurrentState(Ekos::CaptureState state);
    // Capture preparation complete(d)
    void prepareComplete();
    // change the rotator angle
    void setRotatorAngle(double *rawAngle);
    // set the CCD target temperature
    void setCCDTemperature(double temp);
    // set CCD to preview mode
    void setCCDBatchMode(bool m_preview);
    // ask for manually covering the scope with a flat light source
    void askManualScopeLightCover(QString question, QString title);
    // ask for manually opening the scope cover
    void askManualScopeLightOpen();
    // turn light on in the light box
    void setLightBoxLight(bool on);
    // turn light on in the dust cap
    void setDustCapLight(bool on);
    // park the dust cap
    void parkDustCap(bool park);
    // slew the telescope to a target
    void slewTelescope(SkyPoint &target);
    // turn scope tracking on and off
    void setScopeTracking(bool on);
    // park / unpark telescope
    void setScopeParked(bool parked);
    // park / unpark dome
    void setDomeParked(bool parked);
    // check if the the focuser needs to be moved to the focus position.
    void flatSyncFocus(int targetFilterID);
    // ask whether the CCD has a shutter
    void queryHasShutter();
    // abort capturing
    void abortCapture();
    // log entry
    void newLog(QString);

private:
    // current status of the capture sequence job
    JOBStatus m_status { JOB_IDLE };

    // Is the capture preparation ready?
    PreparationState m_PreparationState { PREP_NONE };

    // ////////////////////////////////////////////////////////////////////
    // capture preparation relevant flags
    // ////////////////////////////////////////////////////////////////////

    // Mapping PrepareActions --> bool marks whether a certain action is completed (=true) or not (=false)
    QMap<CaptureModuleState::PrepareActions, bool> prepareActions;
    // This is a workaround for a specific INDI behaviour. If a INDI property is set, it sends this value
    // back to the clients. If the value does not immediately change to the target value (like e.g. the CCD
    // temperature), the first value after setting a property must be ignored.
    QMap<CaptureModuleState::PrepareActions, bool> ignoreNextValue;

    // capture frame type (light, flat, dark, bias)
    CCDFrameType m_frameType { FRAME_NONE };
    // do we shoot a preview?
    bool m_isPreview { false };
    // should a certain temperature should be enforced?
    bool m_enforceTemperature { false };
    // should the a certain maximal initial guiding drift should be enforced?
    bool m_enforceInitialGuiding { false };

    // ////////////////////////////////////////////////////////////////////
    // flat preparation attributes
    // ////////////////////////////////////////////////////////////////////
    // status of the focuser synchronisation
    FlatSyncStatus flatSyncStatus { FS_NONE };
    // light source for flat capturing
    FlatFieldSource flatFieldSource { SOURCE_MANUAL };
    // wall coordinates for capturing flats with the wall as light source
    SkyPoint wallCoord;
    // telescope status for flats using the wall position
    ScopeWallPositionStatus wpScopeStatus { WP_NONE };
    // flag if the mount should be parking before capturing
    bool preMountPark;
    // flag if the dome should be parking before capturing
    bool preDomePark;
    // flag if auto focus has been completed for the selected filter
    bool autoFocusReady;

    // ////////////////////////////////////////////////////////////////////
    // capture preparation state
    // ////////////////////////////////////////////////////////////////////
    /**
     * @brief Check if all actions are ready and emit {@see prepareComplete()} if all are ready.
     */
    void checkAllActionsReady();

    /**
     * @brief Check if all preparation actions are completed
     * @return true if all preparation actions are completed
     */
    bool areActionsReady();

    /**
     * @brief Clear all actions required for preparation
     */
    void setAllActionsReady();

    /**
     * @brief Prepare CCD temperature checks
     */
    void prepareTemperatureCheck(bool enforceCCDTemp);

    /**
     * @brief Before capturing light frames check, if the scope cover is open.
     * @return IPS_OK if cover closed, IPS_BUSY if not and IPS_ALERT if the
     *         process should be aborted.
     */
    IPState checkLightFrameScopeCoverOpen();

    /**
     * @brief Check if a certain action has already been initialized
     */
    bool isInitialized(CaptureModuleState::PrepareActions action);
    /**
     * @brief Set a certain action as initialized
     */
    void setInitialized(CaptureModuleState::PrepareActions action, bool init);

    // ////////////////////////////////////////////////////////////////////
    // flats preparation state
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief Check if the selected flats light source is ready.
     * @return IPS_OK if cover closed, IPS_BUSY if not and IPS_ALERT if the
     *         process should be aborted.
     */
    IPState checkFlatsLightSourceReady();

    /**
      * @brief Ask the user to place a flat screen onto the telescope
      * @return IPS_OK if cover closed, IPS_BUSY if not and IPS_ALERT if the
      *         process should be aborted.
      */
    IPState checkManualFlatsCoverReady();

    /**
     * @brief Check if the telescope cap with internal light source is ready
     *        for capturing flats.
     * @return IPS_OK if cap is closed, IPS_BUSY if not and IPS_ALERT if the
     *         process should be aborted.
     */
    IPState checkFlatCapReady();

    /**
     * @brief Check if the telescope dust cap is ready for capturing flats or darks.
     * @return IPS_OK if cap is closed, IPS_BUSY if not and IPS_ALERT if the
     *         process should be aborted.
     */
    IPState checkDustCapReady(CCDFrameType frameType);

    /**
     * @brief Check if the telescope is pointing to the desired position on the wall
     *        for capturing flats.
     * @return IPS_OK if cap is closed, IPS_BUSY if not and IPS_ALERT if the
     *         process should be aborted.
     */
    IPState checkWallPositionReady(CCDFrameType frametype);

    /**
     * @brief Check mount parking before capturing flats.
     * @return IPS_OK if cap is closed, IPS_BUSY if not and IPS_ALERT if the
     *         process should be aborted.
     */
    IPState checkPreMountParkReady();

    /**
     * @brief Check dome parking before capturing flats.
     * @return IPS_OK if cap is closed, IPS_BUSY if not and IPS_ALERT if the
     *         process should be aborted.
     */
    IPState checkPreDomeParkReady();

    /**
     * @brief Check if the the focuser needs to be moved to the focus position.
     * @return IPS_OK if cover closed, IPS_BUSY if not and IPS_ALERT if the
      *        process should be aborted.
     */
    IPState checkFlatSyncFocus();

    // ////////////////////////////////////////////////////////////////////
    // darks preparation state
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief Check if the CCD has a shutter or not.
     * @return IPS_OK if ready, IPS_BUSY if running and IPS_ALERT if the
     *         process should be aborted.
     */
    IPState checkHasShutter();
    /**
     * @brief Check if the scope is covered manually.
     * @return IPS_OK if ready, IPS_BUSY if running and IPS_ALERT if the
     *         process should be aborted.
     */
    IPState checkManualCover();

    // ////////////////////////////////////////////////////////////////////
    // Attributes shared across all sequence jobs
    // ////////////////////////////////////////////////////////////////////
    QSharedPointer<CaptureModuleState> m_CaptureModuleState;

    // ////////////////////////////////////////////////////////////////////
    // sequence job specific states
    // ////////////////////////////////////////////////////////////////////
    // target filter ID
    int targetFilterID { Ekos::INVALID_VALUE };
    // target temperature
    double targetTemperature { Ekos::INVALID_VALUE };
    // target rotation in absolute ticks, NOT angle
    double targetPositionAngle { Ekos::INVALID_VALUE };
    // target guiding deviation at start time
    double targetStartGuiderDrift { 0.0 };
    // calibration state
    CalibrationStage calibrationStage { CAL_NONE };
    // query state for (un)covering the scope
    CalibrationCheckType coverQueryState { CAL_CHECK_TASK };
    void prepareRotatorCheck();
};

}; // namespace
