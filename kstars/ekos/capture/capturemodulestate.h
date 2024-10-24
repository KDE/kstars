/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "capturetypes.h"
#include "ekos/manager/meridianflipstate.h"
#include "ekos/ekos.h"

#include <QObject>
#include <QSharedPointer>
#include <QTimer>

namespace Ekos
{
class Camera;
class CameraState;

class CaptureModuleState : public QObject
{
    Q_OBJECT

    friend class Capture;

signals:
    void dither();
    // new log text for the module log window
    void newLog(const QString &text);

public:
    explicit CaptureModuleState(QObject *parent = nullptr);

    // ////////////////////////////////////////////////////////////////////
    // shared attributes
    // ////////////////////////////////////////////////////////////////////
    bool forceInSeqAF(const QString opticaltrain) const
     {
        if (m_ForceInSeqAF.contains(opticaltrain))
            return m_ForceInSeqAF[opticaltrain];
        else
            // default value is off
            return false;
    }
    void setForceInSeqAF(bool value, const QString opticaltrain)
    {
        m_ForceInSeqAF[opticaltrain] = value;
    }

    // ////////////////////////////////////////////////////////////////////
    // Captured frames count: signature --> number of frames
    // ////////////////////////////////////////////////////////////////////

    QSharedPointer<CapturedFramesMap> globalCapturedFramesMap() const
    {
        return m_globalCapturedFramesMap;
    }

    uint16_t capturedFramesCount(const QString &signature) const
    {
        return m_globalCapturedFramesMap->value(signature);
    }
    /**
     * @brief addCapturedFrame Increase the number of captured frames
     */
    void addCapturedFrame(const QString &signature, int count);


    // ////////////////////////////////////////////////////////////////////
    // Cameras
    // ////////////////////////////////////////////////////////////////////
    const QList<QSharedPointer<Camera>> &cameras() const
    {
        return m_Cameras;
    }
     QList<QSharedPointer<Camera>> &mutableCameras()
    {
        return m_Cameras;
    }

     void addCamera(QSharedPointer<Camera> newCamera);
     void removeCamera(int pos);

    // ////////////////////////////////////////////////////////////////////
    // State handling
    // ////////////////////////////////////////////////////////////////////

     // Capturing
    void captureStateChanged(CaptureState value, const QString &trainname, int cameraID);
    // Guiding
    void setGuideStatus(GuideState newstate);
    void setGuideDeviation(double delta_ra, double delta_dec);
    // meridian flip
    void updateMFMountState(MeridianFlipState::MeridianFlipMountState status);

    // ////////////////////////////////////////////////////////////////////
    // Action handling
    // ////////////////////////////////////////////////////////////////////
    /**
     * @brief activeAction Currently active action for a single camera, coordinated by the capture module
     * @param cameraID ID of the camera
     * @return workflow action, or CAPTURE_ACTION_NONE if none is active
     */
    CaptureWorkflowActionType activeAction(int cameraID);

    /**
     * @brief isActionActive Determine whether an action is actively coordinated by the capture module
     * @param cameraID ID of the camera
     */
    bool isActionActive(int cameraID)
    {
        return m_activeActions.contains(cameraID);
    }

private:

    // ////////////////////////////////////////////////////////////////////
    // shared attributes
    // ////////////////////////////////////////////////////////////////////
    // User has requested an autofocus run as soon as possible
    QMap<QString, bool> m_ForceInSeqAF;
    // global map signature --> captured frame
    QSharedPointer<CapturedFramesMap> m_globalCapturedFramesMap;
    // ////////////////////////////////////////////////////////////////////
    // Cameras
    // ////////////////////////////////////////////////////////////////////
    QList<QSharedPointer<Camera>> m_Cameras;

    // ////////////////////////////////////////////////////////////////////
    // Action handling
    // ////////////////////////////////////////////////////////////////////

    // currently active actions, maximal one per camera
    QMap<int, CaptureWorkflowActionType> m_activeActions;
    // planned action queues, one queue per camera
    QMap<int, QQueue<CaptureWorkflowActionType>> m_actionQueues;

    /**
     * @brief getActionQueue Retrieve the action queue for a given camera ID
     */
    QQueue<CaptureWorkflowActionType> &getActionQueue(int cameraId);

    /**
     * @brief setActiveAction Set the currently active action for a camera, that is
     *        coordinated by the capture module
     * @param cameraID ID of the camera
     * @param action workflow action, or CAPTURE_ACTION_NONE if none is active
     */
    void setActiveAction(int cameraID, CaptureWorkflowActionType action);

    /**
     * @brief requestAction Handle the execution request of an action.
     * @param cameraID ID of the camera
     * @param action workflow action
     */
    void handleActionRequest(int cameraID, CaptureWorkflowActionType action);

    /**
     * @brief checkActionExecution Check if next actions need to be executed.
     */
    void checkActionExecution();

    /**
     * @brief enqueueAction enqueue an action for future execution
     * @param cameraID ID of the camera for which the action should be executed
     */
    void enqueueAction(int cameraID, CaptureWorkflowActionType action);

    /**
     * @brief checkActiveActions Iterate over all cameras and check if the active
     *        actions may be executed and trigger next action from the action queue
     *        if no active action exists.
     */
    void checkActiveActions();

    /**
     * @brief checkAndExecuteNextAction check if there is an action pending for the
     *        given camera.
     * @param cameraID ID of the camera for which the action should be executed. If the
     *        ID == -1, execute this check for all cameras.
     */
    void checkNextActionExecution(int cameraID = -1);

    /**
     * @brief clearAllActions Clear all action queues and reset the active actions.
     */
    void clearAllActions(int cameraID);

    // ////////////////////////////////////////////////////////////////////
    // Action execution
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief prepareDitheringAction If a dither request has been received, capturing
     *        either requests all capturing cameras to pause or, if this would take too long,
     *        aborts their capturing. As a threshold, we set to pause all cameras where the
     *        remaining time is less than 50% of the exposure time of the camera that requested
     *        the dithering. All others will be aborted.
     * @param cameraId ID of the camera that requests dithering
     */
    void prepareDitheringAction(int cameraId);

    /**
     * @brief checkReadyForDithering Check if dithering may be requested, i.e. for no camera
     *        capturing is still running. If this is the case, request dithering.
     * @param cameraId ID of the camera that requests dithering
     */
    void checkReadyForDithering(int cameraId);

    /**
     * @brief startDithering Enforce dithering, abort all other capturing processes
     */
    void startDithering();

    /**
     * @brief setupRestartPostMF Setup the checks required before the suspended follower jobs
     *        might be restarted.
     */
    void setupRestartPostMF();

    /**
     * @brief pauseCapturingImmediately Pause capturing immediately by calling first to pause
     *        and subsequently to suspend.
     */
    void pauseCapturingImmediately(int cameraID = -1, bool followersOnly = true);

    // timer to avoid waiting infinitely long for settling of other cameras
    QTimer m_DitheringTimer;

    // meridian flip mount state
    MeridianFlipState::MeridianFlipMountState m_MFMountState {MeridianFlipState::MOUNT_FLIP_NONE};

    // ////////////////////////////////////////////////////////////////////
    // helper functions
    // ////////////////////////////////////////////////////////////////////
    /**
     * @brief Check if a meridian flip is ready to start, running or some post flip actions are not completed.
     */
    bool checkMeridianFlipActive();

    /**
     * @brief leadState Retrieve the state machine of the lead camera.
     */
    const QSharedPointer<CameraState> leadState();
};
} // namespace Ekos

