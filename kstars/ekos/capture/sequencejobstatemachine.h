/*  Ekos state machine for a single capture job sequence.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ekos/ekos.h"
#include "indi/indistd.h"

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

class SequenceJobStateMachine: public QObject
{
    Q_OBJECT

    friend class SequenceJob;

public:
    SequenceJobStateMachine();

    /* Action types to be executed before capturing may start. */
    typedef enum
    {
        ACTION_FILTER,      /* Change the filter and wait until the correct filter is set.                                    */
        ACTION_TEMPERATURE, /* Set the camera chip target temperature and wait until the target temperature has been reached. */
        ACTION_ROTATOR,     /* Set the camera rotator target angle and wait until the target angle has been reached.          */
        ACTION_GUIDER_DRIFT /* Wait until the guiding deviation is below the configured threshold.                            */
    } PrepareActions;

    /**
     * @brief Trigger all peparation actions before a capture may be started.
     * @param frameType frame type for which the preparation should be done
     * @param enforceCCDTemp flag if the CCD temperature should be set to the target value.
     * @param enforceStartGuiderDrift flag if the guider drift needs to be taken into account
     * @param isPreview flag if the captures are in the preview mode
     */
    void prepareCapture(CCDFrameType frameType, bool enforceCCDTemp, bool enforceStartGuiderDrift, bool isPreview);

    /**
     * @brief The current capture sequence job status
     */
    JOBStatus getStatus() { return m_status; }

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
     * @brief Update the current rotator position
     */
    void setCurrentRotatorAngle(double value);
    /**
     * @brief Set the target rotation angle
     */
    void setTargetRotatorAngle(double value) { targetRotation = value; }

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


private:
    // current status of the capture sequence job
    JOBStatus m_status { JOB_IDLE };
    // Map tracking whether the current value has been initialized.
    // With this construct we could do a lazy initialization of current values if setCurrent...()
    // sets this flag to true. This is necessary since we listen to the events, but as long as
    // the value (e.g. the CCD temperature) does not change, there won't be an event.
    QMap<PrepareActions, bool> isInitialized;

    // Is the capture preparation ready?
    bool prepareReady { true };

    // ////////////////////////////////////////////////////////////////////
    // capture preparation relevant flags
    // ////////////////////////////////////////////////////////////////////

    // Mapping PrepareActions --> bool marks whether a certain action is completed (=true) or not (=false)
    QMap<PrepareActions, bool> prepareActions;

    // capture frame type (light, flat, dark, bias)
    CCDFrameType m_frameType { FRAME_LIGHT };
    // should a certain temperature should be enforced?
    bool m_enforceTemperature { false };
    // Ensure that guiding deviation is below a configured thresholdß
    bool m_enforceStartGuiderDrift { false };

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
     * @brief Is guiding good enough for start capturing?
     * This is the case if either the guider is not active, the frame type is
     * not a light frame, start guide limit is not active or the guiding deviation
     * is below the configured threshold.
     */
    bool guiderDriftOkForStarting();

    // ////////////////////////////////////////////////////////////////////
    // capture attributes
    // ////////////////////////////////////////////////////////////////////
    // current and target filter ID
    int currentFilterID { Ekos::INVALID_VALUE };
    int targetFilterID { Ekos::INVALID_VALUE };
    // current and target temperature
    double currentTemperature { Ekos::INVALID_VALUE };
    double targetTemperature { Ekos::INVALID_VALUE };
    // current and target rotation in absolute ticks, NOT angle
    double currentRotation { Ekos::INVALID_VALUE };
    double targetRotation { Ekos::INVALID_VALUE };
    // current and target guiding deviation at start time
    double currentGuiderDrift { 1e8 };
    double targetStartGuiderDrift { 0.0 };


};

}; // namespace
