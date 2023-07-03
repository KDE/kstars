/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "capturemodulestate.h"
#include "capturedeviceadaptor.h"

#include "indiapi.h"
#include "ekos/auxiliary/darkprocessor.h"

#include <QObject>

namespace Ekos
{
class CaptureProcess : public QObject
{
    Q_OBJECT

public:
    typedef enum
    {
        ADU_LEAST_SQUARES,
        ADU_POLYNOMIAL
    } ADUAlgorithm;

    CaptureProcess(QSharedPointer<CaptureModuleState> newModuleState, QSharedPointer<CaptureDeviceAdaptor> newDeviceAdaptor);

    // ////////////////////////////////////////////////////////////////////
    // capturing process steps
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief startCapture Start capturing.
     */
    void startCapture();

    /**
     * @brief prepareJob Update the counters of existing frames and continue with prepareActiveJob(), if there exist less
     *        images than targeted. If enough images exist, continue with processJobCompletion().
     */
    void prepareJob(SequenceJob *job);

    /**
     * @brief prepareActiveJobStage1 Check for pre job script to execute. If none, move to stage 2
     */
    void prepareActiveJobStage1();
    /**
     * @brief prepareActiveJobStage2 Reset #calibrationStage and continue with preparePreCaptureActions().
     */
    void prepareActiveJobStage2();

    /**
     * @brief executeJob Start the execution of #activeJob by initiating updatePreCaptureCalibrationStatus().
     */
    void executeJob();

    /**
     * @brief preparePreCaptureActions Trigger setting the filter, temperature, (if existing) the rotator angle and
     *        let the #activeJob execute the preparation actions before a capture may
     *        take place (@see SequenceJob::prepareCapture()).
     *
     * After triggering the settings, this method returns. This mechanism is slightly tricky, since it
     * asynchronous and event based and works as collaboration between Capture and SequenceJob. Capture has
     * the connection to devices and SequenceJob knows the target values.
     *
     * Each time Capture receives an updated value - e.g. the current CCD temperature
     * (@see updateCCDTemperature()) - it informs the #activeJob about the current CCD temperature.
     * SequenceJob checks, if it has reached the target value and if yes, sets this action as as completed.
     *
     * As soon as all actions are completed, SequenceJob emits a prepareComplete() event, which triggers
     * executeJob() from the CaptureProcess.
     */
    void prepareJobExecution();

    /**
     * @brief Check all tasks that might be pending before capturing may start.
     *
     * The following checks are executed:
     *  1. Are there any pending jobs that failed? If yes, return with IPS_ALERT.
     *  2. Has pausing been initiated (@see checkPausing()).
     *  3. Is a meridian flip already running (@see m_MeridianFlipState->checkMeridianFlipRunning()) or ready
     *     for execution (@see CaptureModuleState::checkMeridianFlipReady()).
     *  4. check guide deviation for non meridian flip stages if the initial guide limit is set.
     *     Wait until the guide deviation is reported to be below the limit
     *     (@see Capture::setGuideDeviation(double, double)).
     *  5. Check if dithering is required or running.
     *  6. Check if re-focusing is required
     *     Needs to be checked after dithering checks to avoid dithering in parallel
     *     to focusing, since @startFocusIfRequired() might change its value over time
     *  7. Resume guiding if it was suspended (@see Capture::resumeGuiding())
     *
     * @return IPS_OK iff no task is pending, IPS_BUSY otherwise (or IPS_ALERT if a problem occured)
     */
    IPState checkLightFramePendingTasks();

    /**
     * @brief checkNextExposure Try to start capturing the next exposure (@see startNextExposure()).
     *        If startNextExposure() returns, that there are still some jobs pending,
     *        we wait for 1 second and retry to start it again.
     *        If one of the pending preparation jobs has problems, the looping stops.
     */
    void checkNextExposure();

    /**
     * @brief startNextExposure Ensure that all pending preparation tasks are be completed (focusing, dithering, etc.)
     *        and start the next exposure.
     *
     * Checks of pending preparations depends upon the frame type:
     *
     * - For light frames, pending preparations like focusing, dithering etc. needs
     *   to be checked before each single frame capture. efore starting to capture the next light frame,
     *   checkLightFramePendingTasks() is called to check if all pending preparation tasks have
     *   been completed successfully. As soon as this is the case, the sequence timer
     *   #seqTimer is started to wait the configured delay and starts capturing the next image.
     *
     * - For bias, dark and flat frames, preparation jobs are only executed when starting a sequence.
     *   Hence, for these frames we directly start the sequence timer #seqTimer.
     *
     * @return IPS_OK, iff all pending preparation jobs are completed (@see checkLightFramePendingTasks()).
     *         In that case, the #seqTimer is started to wait for the configured settling delay and then
     *         capture the next image (@see Capture::captureImage). In case that a pending task aborted,
     *         IPS_IDLE is returned.
     */

    IPState startNextExposure();

    /**
     * @brief resumeSequence Try to continue capturing.
     *
     * Take the active job, if there is one, or search for the next one that is either
     * idle or aborted. If a new job is selected, call startNextJob() to prepare it.
     * If the current job is still active, initiate checkNextExposure().
     *
     * @return IPS_OK if there is a job that may be continued, IPS_BUSY otherwise.
     */
    IPState resumeSequence();

    /**
     * @brief newFITS process new FITS data received from camera. Update status of active job and overall sequence.
     * @param data pointer to blob containing FITS data
     */
    void processFITSData(const QSharedPointer<FITSData> &data);

    /**
     * @brief processCaptureCompleted Manage the capture process after a captured image has been successfully downloaded from the camera.
     *
     * When a image frame has been captured and downloaded successfully, send the image to the client (if configured)
     * and execute the book keeping for the captured frame. After this, either processJobCompletion() is executed
     * in case that the job is completed, and resumeSequence() otherwise.
     *
     * Special case for flat capturing: exposure time calibration is executed in this process step as well.
     *
     * Book keeping means:
     * - increase / decrease the counters for focusing and dithering
     * - increase the frame counter
     * - update the average download time
     *
     * @return IPS_OK if processing has been completed, IPS_BUSY if otherwise.
     */
    IPState processCaptureCompleted();

    /**
     * @brief Manage the capture process after a captured image has been successfully downloaded from the camera.
     *
     */
    void imageCapturingCompleted();


    /**
     * @brief processPreCaptureCalibrationStage Execute the tasks that need to be completed before capturing may start.
     *
     * For light frames, checkLightFramePendingTasks() is called.
     *
     * @return IPS_OK if all necessary tasks have been completed
     */
    IPState processPreCaptureCalibrationStage();

    /**
     * @brief updatePreCaptureCalibrationStatusThis is a wrapping loop for processPreCaptureCalibrationStage(),
     *        which contains all checks before captureImage() may be called.
     *
     * If processPreCaptureCalibrationStage() returns IPS_OK (i.e. everything is ready so that
     * capturing may be started), captureImage() is called. Otherwise, it waits for a second and
     * calls itself again.
     */
    void updatePreCaptureCalibrationStatus();

    /**
     * @brief processJobCompletionStage1 Process job completion. In stage 1 when simply check if the is a post-job script to be running
     * if yes, we run it and wait until it is done before we move to stage2
     */
    void processJobCompletion1();

    /**
     * @brief processJobCompletionStage2 Stop execution of the current sequence and check whether there exists a next sequence
     *        and start it, if there is a next one to be started (@see resumeSequence()).
     */
    void processJobCompletion2();

    /**
     * @brief startNextJob Select the next job that is either idle or aborted and
     * call prepareJob(*SequenceJob) to prepare its execution and
     * resume guiding if it was suspended (and no meridian flip is running).
     * @return IPS_OK if a job to be executed exists, IPS_IDLE otherwise.
     */
    IPState startNextJob();

    /**
     * @brief captureImage Initiates image capture in the active job.
     */
    void captureImage();

    // ////////////////////////////////////////////////////////////////////
    // capturing actions
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief setExposureProgress Manage exposure progress reported by
     * the camera device.
     */
    void setExposureProgress(ISD::CameraChip *tChip, double value, IPState state);

    /**
     * @brief continueFramingAction If framing is running, start the next capture sequence
     * @return IPS_OK in all cases
     */
    IPState continueFramingAction(const QSharedPointer<FITSData> &imageData);

    /**
     * @brief updateDownloadTimesAction Add the current download time to the list of already measured ones
     */
    IPState updateDownloadTimesAction();

    /**
     * @brief previewImageCompletedAction Activities required when a preview image has been captured.
     * @return IPS_OK if a preview has been completed, IPS_IDLE otherwise
     */
    IPState previewImageCompletedAction(QSharedPointer<FITSData> imageData);

    /**
     * @brief updateCompletedCaptureCounters Update counters if an image has been captured
     * @return
     */
    IPState updateCompletedCaptureCountersAction();

    /**
     * @brief updateImageMetadataAction Update meta data of a captured image
     */
    IPState updateImageMetadataAction(QSharedPointer<FITSData> imageData);

    /**
     * @brief runCaptureScript Run the pre-/post capture/job script
     * @param scriptType script type (pre-/post capture/job)
     * @param precond additional pre condition for starting the script
     * @return IPS_BUSY, of script exists, IPS_OK otherwise
     */
    IPState runCaptureScript(ScriptTypes scriptType, bool precond = true);

    /**
     * @brief scriptFinished Slot managing the return status of
     * pre/post capture/job scripts
     */
    void scriptFinished(int exitCode, QProcess::ExitStatus status);

    /**
     * @brief processCaptureTimeout If exposure timed out, let's handle it.
     */
    void processCaptureTimeout();

    /**
     * @brief processCaptureError Handle when image capture fails
     * @param type error type
     */
    void processCaptureError(ISD::Camera::ErrorType type);

    /**
     * @brief checkFlatCalibration check the flat calibration
     * @param imageData current image data to be analysed
     * @param exp_min minimal possible exposure time
     * @param exp_max maximal possible exposure time
     * @return false iff calibration has not been reached yet
     */
    bool checkFlatCalibration(QSharedPointer<FITSData> imageData, double exp_min, double exp_max);

    /**
     * @brief calculateFlatExpTime calculate the next flat exposure time from the measured ADU value
     * @param currentADU ADU of the last captured frame
     * @return next exposure time to be tried for the flat capturing
     */
    double calculateFlatExpTime(double currentADU);

    /**
     * @brief clearFlatCache Clear the measured values for flat calibrations
     */
    void clearFlatCache();

    /**
         * @brief Connect or disconnect the camera device
         * @param connection flag if connect (=true) or disconnect (=false)
         */
    void connectCamera(bool connection);


    // ////////////////////////////////////////////////////////////////////
    // helper functions
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief checkPausing check if a pause has been planned and pause subsequently
     * @param continueAction action to be executed when resume after pausing
     * @return true iff capturing has been paused
     */
    bool checkPausing(CaptureModuleState::ContinueAction continueAction);

    //  Based on  John Burkardt LLSQ (LGPL)
    void llsq(QVector<double> x, QVector<double> y, double &a, double &b);

    /**
     * @brief generateScriptArguments Generate argument list to pass to capture script
     * @return generates argument list consisting of one argument -metadata followed by JSON-formatted key:value pair:
     * -ts UNIX timestamp
     * -image full path to captured image (if any)
     * -size size of file in bytes (if any)
     * -job {name, index}
     * -capture {name, index}
     * -filter
     * TODO depending on user feedback.
     */
    QStringList generateScriptArguments() const;

    // ////////////////////////////////////////////////////////////////////
    // attributes access
    // ////////////////////////////////////////////////////////////////////
    QProcess &captureScript()
    {
        return m_CaptureScript;
    }

signals:
    // controls for capture execution
    void stopCapture(CaptureState targetState = CAPTURE_IDLE);
    void syncGUIToJob(SequenceJob *job);
    void jobExecutionPreparationStarted();
    void jobPrepared(SequenceJob *job);
    void captureImageStarted();
    void darkFrameCompleted();
    void updateMeridianFlipStage(MeridianFlipState::MFStage stage);
    void cameraReady();
    void newExposureValue(ISD::CameraChip * tChip, double value, IPState state);
    void processingFITSfinished(bool success);
    void reconnectDriver(const QString &camera, const QString &filterWheel);
    // communication with other modules
    void newImage(SequenceJob *job, const QSharedPointer<FITSData> &data);
    void suspendGuiding();
    void resumeGuiding();
    void captureComplete(const QVariantMap &metadata);
    void sequenceChanged(const QJsonArray &sequence);
    void driverTimedout(const QString &deviceName);
    // new log text for the module log window
    void newLog(const QString &text);


private:
    QSharedPointer<CaptureModuleState> m_State;
    QSharedPointer<CaptureDeviceAdaptor> m_DeviceAdaptor;
    QPointer<DarkProcessor> m_DarkProcessor;

    // Pre-/post capture script process
    QProcess m_CaptureScript;
    // Flat field automation
    QVector<double> ExpRaw, ADURaw;
    ADUAlgorithm targetADUAlgorithm { ADU_LEAST_SQUARES };

    /**
     * @brief activeJob Shortcut to the active job held in the state machine
     */
    SequenceJob *activeJob()
    {
        return  m_State->getActiveJob();
    }

    /**
     * @brief activeCamera Shortcut to the active camera held in the device adaptor
     */
    ISD::Camera *activeCamera()
    {
        return m_DeviceAdaptor->getActiveCamera();
    }



};
} // Ekos namespace
