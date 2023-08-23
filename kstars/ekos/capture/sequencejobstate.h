/*  Ekos state machine for a single capture job sequence.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indicommon.h"
#include "skypoint.h"
#include "capturemodulestate.h"
#include "ekos/auxiliary/filtermanager.h"
#include "fitsviewer/fitscommon.h"
#include "ekos/auxiliary/rotatorutils.h"

#include <QWidget>
#include <QVector>
#include <QMap>

namespace Ekos
{
/* Status of a single {@see SequenceJob}. */
typedef enum
{
    JOB_IDLE,     /* Initial state, nothing happens. */
    JOB_BUSY,     /* Job is running.                 */
    JOB_ERROR,    /* Error occured, unresolved.      */
    JOB_ABORTED,  /* Job stopped before completion.  */
    JOB_DONE      /* Job completed.                  */
} JOBStatus;

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

        typedef enum
        {
            PREP_NONE,           /* preparation has not been executed                               */
            PREP_BUSY,           /* preparation started                                             */
            PREP_COMPLETED,      /* preparation completed                                           */
            PREP_INIT_CAPTURE    /* initialize capturing (last step before device capturing starts) */
        } PreparationState;

        typedef enum
        {
            CAL_CHECK_TASK,
            CAL_CHECK_CONFIRMATION,
        } CalibrationCheckType;

        typedef enum
        {
            WP_NONE,           /* slewing to wall position not started */
            WP_SLEWING,        /* slewing to wall position started     */
            WP_SLEW_COMPLETED, /* wall position reached                */
            WP_TRACKING_BUSY,  /* turning tracking off running         */
            WP_TRACKING_OFF    /* wall position reached, tracking off  */
        } ScopeWallPositionStatus;

        typedef enum      /* synching the focuser to the focus position */
        {
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
        CCDFrameType getFrameType()
        {
            return m_frameType;
        }

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
         * @brief initCapture Initialize capturing (currently only setting the
         * target filter).
         * @param frameType frame type to be captured
         * @param isPreview is the captured frame only a preview?
         * @param isAutofocusReady is an autofocus possible?
         * @return true if the initialization is already completed
         */
        bool initCapture(CCDFrameType frameType, bool isPreview, bool isAutofocusReady, FITSMode mode);

        /**
         * @brief Set the flag if a maximal initial guiding deviation is required
         */
        void setEnforceInitialGuidingDrift(bool enforceInitialGuidingDrift);

        /**
         * @brief The current capture sequence job status
         */
        JOBStatus getStatus()
        {
            return m_status;
        }

        /**
         * @brief Preparation state of the sequence job
         */
        PreparationState getPreparationState() const;

        /**
         * @brief Reset the status to a dedicated value.
         * @param status new status, by default {@see JOB_IDLE}
         */
        void reset(JOBStatus status = JOB_IDLE)
        {
            m_status = status;
        }

    public slots:
        //////////////////////////////////////////////////////////////////////
        // Slots for device events that change the state.
        //////////////////////////////////////////////////////////////////////

        /**
         * @brief setFilterStatus Update the current filter state
         */
        void setFilterStatus(FilterState filterState);

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
        void setTargetCCDTemperature(double value)
        {
            targetTemperature = value;
        }
        /**
         * @brief Update the current guiding deviation.
         */
        void setCurrentGuiderDrift(double value);
        /**
         * @brief Set the target guiding deviation when capture starts
         */
        void setTargetStartGuiderDrift(double value)
        {
            targetStartGuiderDrift = value;
        }

        /**
         * @brief setFocusStatus Evaluate the current focus state
         */
        void setFocusStatus(FocusState state);

        /**
         * @brief Update the current rotator position (calculated from the rotator angle)
         */
        void setCurrentRotatorPositionAngle(double currentRotation, IPState state);
        /**
         * @brief Set the target rotation angle
         */
        void setTargetRotatorAngle(double value)
        {
            targetPositionAngle = value;
        }

        /**
         * @brief Cover for the scope with a flats light source or dark cover done
         */
        void updateManualScopeCover(bool closed, bool success, bool light);
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
        void prepareComplete(bool success = true);
        // Capture initialization complete(d)
        void initCaptureComplete(FITSMode mode);
        // change the rotator angle
        void setRotatorAngle(double rawAngle);
        // ask for the current filter position
        void readFilterPosition();
        // Change the filter to the given position and the filter change policy
        void changeFilterPosition(int targetFilterPosition, FilterManager::FilterPolicy policy);
        // set the CCD target temperature
        void setCCDTemperature(double temp);
        // set CCD to preview mode
        void setCCDBatchMode(bool m_preview);
        // ask for manually covering the scope with a flat light source or dark cover
        void askManualScopeCover(QString question, QString title, bool light);
        // ask for manually opening the scope cover
        void askManualScopeOpen(bool light);
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
        // check if the focuser needs to be moved to the focus position.
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
        // flag if auto focus has been completed for the selected filter
        bool autoFocusReady;
        // Capturing mode, necessary for the display in the FITS viewer
        FITSMode m_fitsMode;
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
         * @brief prepareRotatorCheck Check if the rotator is at the expected
         *        target angle.
         */
        void prepareRotatorCheck();

        /**
         * @brief prepareTargetFilter Initiate changing the filter to the target position
         *        {@see #targetFilterID}
         * @param frameType frame type to be captured
         * @param isPreview is the captured frame only a preview?
         */
        void prepareTargetFilter(CCDFrameType frameType, bool isPreview);

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
        IPState checkFlatsLightCoverReady();

        /**
         * @brief Check if the selected dark covers is ready.
         * @return IPS_OK if cover closed, IPS_BUSY if not and IPS_ALERT if the
         *         process should be aborted.
         */
        IPState checkDarksCoverReady();

        /**
          * @brief Ask the user to place a flat screen onto the telescope
          * @return IPS_OK if cover closed, IPS_BUSY if not and IPS_ALERT if the
          *         process should be aborted.
          */
        IPState checkManualCoverReady(bool light);

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
         * @brief Check if the focuser needs to be moved to the focus position.
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

        // ////////////////////////////////////////////////////////////////////
        // Attributes shared across all sequence jobs
        // ////////////////////////////////////////////////////////////////////
        QSharedPointer<CaptureModuleState> m_CaptureModuleState;

        // ////////////////////////////////////////////////////////////////////
        // sequence job specific states
        // ////////////////////////////////////////////////////////////////////
        // target filter ID
        int targetFilterID { Ekos::INVALID_VALUE };
        FilterManager::FilterPolicy m_filterPolicy { FilterManager::ALL_POLICIES };
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
};

}; // namespace
