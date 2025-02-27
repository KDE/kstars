/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "camerastate.h"
#include "sequencejob.h"

#include "indiapi.h"

#include <QObject>

class FITSViewer;

namespace Ekos
{

class CaptureDeviceAdaptor;
class DarkProcessor;

/**
 * @class CameraProcess
 * @brief The CameraProcess class holds the entire business logic to control capturing execution.
 *
 * Capture Execution
 * =================
 * Executing the sequence jobs is a complex process (explained here for light frames) and works roughly
 * as follows and starts by calling {@see Capture#start()} either from the scheduler, by DBUS or by
 * pressing the start button:
 * 1. Select the next sequence job to be executed ({@see startNextPendingJob()}. If the list of jobs is
 * empty, an {@see #addJob()} event is sent. The corresponding callback function
 * {@see #jobAdded(SequenceJob*)} is triggered. Now we know that at least one sequence job is
 * to be executed.
 * 2. Prepare the selected job
 *    - update the counters of captured frames ({@see #prepareJob(SequenceJob *)})
 *    - execute the pre job script, if existing ({@see #prepareActiveJobStage1()})
 *    - set temperature, rotator angle and wait (if required) for the initial guiding
 *      deviation being below the configured threshold ({@see #prepareJobExecution()})
 *      and wait until these parameters are OK.
 * 3. Prepare capturing a single frame
 *    We still need to do some preparation steps before capturing starts.
 *    - {@see #executeJob()} is the starting point, which simply sets the capture state
 *      to "busy" and sets the FITS attributes to the camera
 *    - Check all tasks that need to be completed before capturing may start (post meridian
 *      flip actions, guiding deviation, dithering, re-focusing, ..., see {@see #checkLightFramePendingTasks()}
 * 4. Capture a single frame
 *    - Initiate capturing (set diverse settings of {@see #activeCamera()} (see {@see #captureImage})
 *    - hand over the control of capturing to the sequence job ({@see SequenceJob#startCapturing()})
 *    - Select the correct filter (@see SequenceJobState#prepareTargetFilter()}
 *    - As soon as the correct filter is set, the sequence job state will send the event
 *      {@see SequenceJobState::initCaptureComplete()}, which will finally call trigger
 *      {@see SequenceJob::capture()}
 * 5. Listen upon capturing progress
 *    - listen to the event {@see ISD::Camera::newExposureValue}, update the remaining
 *      time and wait until the INDI state changes from busy to OK
 *    - start the download timer to measure download times
 *    - listen to the event {@see ISD::Camera::newImage} and start processing the FITS image
 *      as soon as it has been recieved
 * 6. Process received image
 *    - update the FITS image meta data {@see #updateImageMetadataAction()}
 *    - update time calculation and counters and execute post capture script ({@see imageCapturingCompleted()})
 * 7. Check how to continue the sequence execution ({@see resumeSequence()})
 *    - if the current sequence job isn't completed,
 *      - execute the post capture script
 *      - start next exposure (similar to 3.) ({@see startNextExposure()})
 *        TODO: check why we need this separate method and cannot use {@see updatePreCaptureCalibrationStatus()}
 *    - if the current sequence is complete,
 *      - execute the post sequence script ({@see processJobCompletion1()})
 *      - stop the current sequence job ({@see processJobCompletion2()})
 *      - recall {@see resumeSequence()}, which calls {@see startNextJob()}
 *      - if there is another job to be executed, jump to 2., otherwise Capture is completed
 *        by sending a stopCapture(CAPTURE_COMPLETE) event
 *
 *  ADU based flats calibration
 *  ===========================
 *  Capturing ADU based flats begins like capturing other frame types. The difference begins as soon as an
 *  image has been received (step 6 above) in {@see imageCapturingCompleted()}. Here {@see checkFlatCalibration()}
 *  is called to check the frame's ADU. If the ADU isn't in the expected range, a new frame is captured with
 *  modified exposure time ({@see startNextExposure()}.
 *
 *  Autofocus
 *  =========
 *  Capture has three ways that trigger autofocus during a capturing sequence: HFR based, temperature drift based,
 *  timer based and post meridian flip based. Each time the capture execution reaches the preparation of caturing
 *  a single frame (3. above) (see {@see CameraState#startFocusIfRequired()} and
 *  {@see RefocusState#checkFocusRequired()}).
 *
 *  Meridian Flip
 *  =============
 *  The meridian flip itself is executed by the Mount module and is controlled by
 *  (see {@see MeridianFlipState}). Nevertheless, the Capture module plays an
 *  important rule in the meridian flip:
 *  1. Accept a flip to be executed
 *     As soon as a meridian flip has been planned (informed through
 *     {@see #updateMFMountState(MeridianFlipState::MeridianFlipMountState)}, the meridian flip state is set
 *     to MF_REQUESTED.
 *     - If capturing is running the state remains in this state until the frame has been captured. As soon as
 *       the capturing state changes to id, suspended or aborted (see {@see CameraState::setCaptureState(CaptureState)}),
 *       the meridian flip state is set to MF_ACCEPTED (see {@see MeridianFlipState::updateMeridianFlipStage(const MFStage)}).
 *       This is triggered from {@see #checkLightFramePendingTasks()}, i.e. this function is looping once per second until
 *       the meridian flip has been completed.
 *     - If capturing is not running, the latter happens immediately.
 *     Now the meridian flip is started.
 *  2. Post MF actions
 *     As soon as the meridian flip has been completed (and the Capture module is waiting for it), the Capture module
 *     takes over the control and executes all necessary tasks: aligning, re-focusing, guiding, etc. This happens all through
 *     {@see #checkLightFramePendingTasks()}. As soon as all has recovered, capturing continues.
 */
class CameraProcess : public QObject
{
        Q_OBJECT

    public:
        typedef enum
        {
            ADU_LEAST_SQUARES,
            ADU_POLYNOMIAL
        } ADUAlgorithm;

        typedef struct FitsvViewerTabIDs
        {
            int normalTabID { -1 };
            int calibrationTabID { -1 };
            int focusTabID { -1 };
            int guideTabID { -1 };
            int alignTabID { -1 };

        } FitsvViewerTabIDs;


        CameraProcess(QSharedPointer<CameraState> newModuleState, QSharedPointer<CaptureDeviceAdaptor> newDeviceAdaptor);

        // ////////////////////////////////////////////////////////////////////
        // handle connectivity to modules and devices
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief setMount Connect to the given mount device (and deconnect the old one
         * if existing)
         * @param device pointer to Mount device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setMount(ISD::Mount *device);

        /**
         * @brief setRotator Connect to the given rotator device (and deconnect
         *  the old one if existing)
         * @param device pointer to rotator INDI device
         * @return True if added successfully, false if duplicate or failed to add.
         */
        bool setRotator(ISD::Rotator * device);

        /**
         * @brief setDustCap Connect to the given dust cap device (and deconnect
         * the old one if existing)
         * @param device pointer to dust cap INDI device
         * @return True if added successfully, false if duplicate or failed to add.
         */
        bool setDustCap(ISD::DustCap *device);

        /**
         * @brief setLightBox Connect to the given dust cap device (and deconnect
         * the old one if existing)
         * @param device pointer to light box INDI device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setLightBox(ISD::LightBox *device);

        /**
         * @brief setDome Connect to the given dome device
         * @param device point to dome INDI device
         * @return True if added successfully, false if duplicate or failed to add.
         */
        bool setDome(ISD::Dome *device);

        /**
         * @brief setCamera Connect to the given camera device (and deconnect
         * the old one if existing)
         * @param device pointer to camera INDI device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setCamera(ISD::Camera *device);

        /**
         * @brief setScope Set active train telescope name
         * @param name Name of scope
         */
        void setScope(const QString &name)
        {
            m_Scope = name;
        }

        /**
          * @brief Connect or disconnect the camera device
          * @param connection flag if connect (=true) or disconnect (=false)
          */
        void setCamera(bool connection);

        /**
         * @brief setFilterWheel Connect to the given filter wheel device (and deconnect
         * the old one if existing)
         * @param device pointer to filter wheel INDI device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setFilterWheel(ISD::FilterWheel *device);

        /**
         * Toggle video streaming if supported by the device.
         * @param enabled Set to true to start video streaming, false to stop it if active.
         */
        void toggleVideo(bool enabled);

        // ////////////////////////////////////////////////////////////////////
        // capturing process steps
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief toggleSequence Toggle sequence state depending on its current state.
         * 1. If paused, then resume sequence.
         * 2. If idle or completed, then start sequence.
         * 3. Otherwise, abort current sequence.
         */
        Q_SCRIPTABLE void toggleSequence();

        /**
         * @brief startNextPendingJob Start the next pending job.
         *
         * Find the next job to be executed:
         * 1. If there are already some jobs defined, {@see #findNextPendingJob()} is
         *    used to find the next job to be executed.
         * 2. If the list is empty, the current settings are used to create a job instantly,
         *    which subsequently will be executed.
         */
        void startNextPendingJob();

        /**
         * @brief Counterpart to the event {@see#createJob(SequenceJob::SequenceJobType)}
         * where the event receiver reports whether one has been added successfully
         * and of which type it was.
         */
        void jobCreated(QSharedPointer<SequenceJob> newJob);

        /**
         * @brief capturePreview Capture a preview (single or looping ones)
         */
        void capturePreview(bool loop = false);

        /**
         * @brief stopCapturing Stopping the entire capturing state
         * (envelope for aborting, suspending, pausing, ...)
         * @param targetState state capturing should be having afterwards
         */
        void stopCapturing(CaptureState targetState);

        /**
         * @brief pauseCapturing Pauses capturing as soon as the current
         * capture is complete.
         */
        Q_SCRIPTABLE void pauseCapturing();

        /**
         * @brief startJob Start the execution of a selected sequence job:
         * - Initialize the state for capture preparation ({@see CameraState#initCapturePreparation()}
         * - Prepare the selected job ({@see #prepareJob(SequenceJob *)})
         * @param job selected sequence job
         */
        void startJob(const QSharedPointer<SequenceJob> &job);

        /**
         * @brief prepareJob Update the counters of existing frames and continue with prepareActiveJob(), if there exist less
         *        images than targeted. If enough images exist, continue with processJobCompletion().
         */
        void prepareJob(const QSharedPointer<SequenceJob> &job);

        /**
         * @brief prepareActiveJobStage1 Check for pre job script to execute. If none, move to stage 2
         */
        void prepareActiveJobStage1();
        /**
         * @brief prepareActiveJobStage2 Reset #calibrationStage and continue with preparePreCaptureActions().
         */
        void prepareActiveJobStage2();

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
         * executeJob() from the CameraProcess.
         */
        void prepareJobExecution();

        /**
         * @brief executeJob Start the execution of #activeJob by initiating updatePreCaptureCalibrationStatus().
         */
        Q_SCRIPTABLE void executeJob();

        /**
         * @brief refreshOpticalTrain Refresh the devices from the optical train configuration
         * @param name name of the optical train configuration
         */
        void refreshOpticalTrain(QString name);

        /**
         * @brief Check all tasks that might be pending before capturing may start.
         *
         * The following checks are executed:
         *  1. Are there any pending jobs that failed? If yes, return with IPS_ALERT.
         *  2. Has pausing been initiated (@see checkPausing()).
         *  3. Is a meridian flip already running (@see m_MeridianFlipState->checkMeridianFlipRunning()) or ready
         *     for execution (@see CameraState#checkMeridianFlipReady()).
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
         * @brief updatePreCaptureCalibrationStatus This is a wrapping loop for processPreCaptureCalibrationStage(),
         *        which contains all checks before captureImage() may be called.
         *
         * If processPreCaptureCalibrationStage() returns IPS_OK (i.e. everything is ready so that
         * capturing may be started), captureImage() is called. Otherwise, it waits for a second and
         * calls itself again.
         */
        void updatePreCaptureCalibrationStatus();

        /**
         * @brief processPreCaptureCalibrationStage Execute the tasks that need to be completed before capturing may start.
         *
         * For light frames, checkLightFramePendingTasks() is called.
         *
         * @return IPS_OK if all necessary tasks have been completed
         */
        IPState processPreCaptureCalibrationStage();

        /**
         * @brief captureStarted Manage the result when capturing has been started
         */
        void captureStarted(CaptureResult rc);

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
         *
         * Manage the capture process after a captured image has been successfully downloaded
         * from the camera:
         * - stop timers for timeout and download progress
         * - update the download time calculation
         * - update captured frames counters ({@see updateCompletedCaptureCountersAction()})
         * - check flat calibration (for flats only)
         * - execute the post capture script (if existing)
         * - resume the sequence ({@see resumeSequence()})
         *
         * @param data pointer to blob containing FITS data
         * @param extension defining the file type
         */
        void processFITSData(const QSharedPointer<FITSData> &data, const QString &extension);

        /**
         * @brief showFITSPreview Directly show the FITS data as preview
         * @param data pointer to blob containing FITS data
         */
        void showFITSPreview(const QSharedPointer<FITSData> &data);

        /**
         * @brief setNewRemoteFile A new image has been stored as remote file
         * @param file local file path
         */
        void processNewRemoteFile(QString file);

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

        /**
         * @brief resetFrame Reset frame settings of the camera
         */
        Q_SCRIPTABLE void resetFrame();

        // ////////////////////////////////////////////////////////////////////
        // capturing actions
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief setExposureProgress Manage exposure progress reported by
         * the camera device.
         */
        void setExposureProgress(ISD::CameraChip *tChip, double value, IPState state);

        /**
         * @brief setDownloadProgress update the Capture Module and Summary
         *        Screen's estimate of how much time is left in the download
         */
        void setDownloadProgress();

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
        IPState previewImageCompletedAction();

        /**
         * @brief updateCompletedCaptureCounters Update counters if an image has been captured
         */
        void updateCompletedCaptureCountersAction();

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
         * @brief setCamera select camera device
         * @param name Name of the camera device
        */
        void selectCamera(QString name);

        /**
         * @brief configureCamera Refreshes the CCD information in the capture module.
         */
        void checkCamera();

        /**
         * @brief syncDSLRToTargetChip Syncs INDI driver CCD_INFO property to the DSLR values.
         * This include Max width, height, and pixel sizes.
         * @param model Name of camera driver in the DSLR database.
         */
        void syncDSLRToTargetChip(const QString &model);

        /**
         * @brief reconnectDriver Reconnect the camera driver
         */
        void reconnectCameraDriver(const QString &camera, const QString &filterWheel);

        /**
         * @brief Generic method for removing any connected device.
         */
        void removeDevice(const QSharedPointer<ISD::GenericDevice> &device);

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
         * @brief updateFITSViewer display new image in the configured FITSViewer tab.
         */
        void updateFITSViewer(const QSharedPointer<FITSData> data, const FITSMode &captureMode, const FITSScale &captureFilter,
                              const QString &filename, const QString &deviceName);
        void updateFITSViewer(const QSharedPointer<FITSData> data, ISD::CameraChip *tChip, const QString &filename);

        // ////////////////////////////////////////////////////////////////////
        // video streaming
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief getVideoWindow Return the current video window and initialize it if required.
         */
        QSharedPointer<StreamWG> getVideoWindow();

        void updateVideoWindow(int width, int height, bool streamEnabled);
        void closeVideoWindow();
        void showVideoFrame(INDI::Property prop, int width, int height);

        // ////////////////////////////////////////////////////////////////////
        // XML capture sequence file handling
        // ////////////////////////////////////////////////////////////////////
        /**
         * Loads the Ekos Sequence Queue file in the Sequence Queue. Jobs are appended to existing jobs.
         * @param fileURL full URL of the filename
         * @param targetName override the target defined in the sequence queue file (necessary for using the target of the scheduler)
         */
        bool loadSequenceQueue(const QString &fileURL, const QString &targetName = "", bool setOptions = true);

        /**
         * Saves the Sequence Queue to the Ekos Sequence Queue file.
         * @param fileURL full URL of the filename
         */
        bool saveSequenceQueue(const QString &path, bool loadOptions = true);

        // ////////////////////////////////////////////////////////////////////
        // helper functions
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief checkPausing check if a pause has been planned and pause subsequently
         * @param continueAction action to be executed when resume after pausing
         * @return true iff capturing has been paused
         */
        bool checkPausing(CaptureContinueAction continueAction);

        /**
         * @brief findExecutableJob find next job to be executed
         */
        const QSharedPointer<SequenceJob> findNextPendingJob();

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

        /**
         * @brief Does the CCD has a cooler control (On/Off) ?
         */
        bool hasCoolerControl();

        /**
         * @brief Set the CCD cooler ON/OFF
         *
         */
        bool setCoolerControl(bool enable);

        /**
         * @brief restartCamera Restarts the INDI driver associated with a camera. Remote and Local drivers are supported.
         * @param name Name of camera to restart. If a driver defined multiple cameras, they would be removed and added again
         * after driver restart.
         * @note Restarting camera should only be used as a last resort when it comes completely unresponsive. Due the complex
         * nature of driver interactions with Ekos, restarting cameras can lead to unexpected behavior.
         */
        void restartCamera(const QString &name);

        /**
         * @brief frameTypes Retrieve the frame types from the active camera's primary chip.
         */
        QStringList frameTypes();
        /**
         * @brief filterLabels list of currently available filter labels
         */
        QStringList filterLabels();

        /**
         * @brief getGain Update the gain value from the custom property value. Depending
         *        on the camera, it is either stored as GAIN property value of CCD_GAIN or as
         *        Gain property value from CCD_CONTROLS.
         */
        void updateGain(double value, QMap<QString, QMap<QString, QVariant> > &propertyMap);

        /**
         * @brief getOffset Update the offset value from the custom property value. Depending
         *        on the camera, it is either stored as OFFSET property value of CCD_OFFSET or as
         *        Offset property value from CCD_CONTROLS.
         */
        void updateOffset(double value, QMap<QString, QMap<QString, QVariant> > &propertyMap);


        // ////////////////////////////////////////////////////////////////////
        // attributes access
        // ////////////////////////////////////////////////////////////////////
        QProcess &captureScript()
        {
            return m_CaptureScript;
        }

    signals:
        // controls for capture execution
        void addJob (const QSharedPointer<SequenceJob> &job);
        void createJob(SequenceJob::SequenceJobType jobtype = SequenceJob::JOBTYPE_BATCH);
        void jobStarting();
        void stopCapture(CaptureState targetState = CAPTURE_IDLE);
        void captureAborted(double exposureSeconds);
        void captureStopped();
        void requestAction(CaptureWorkflowActionType action);
        void syncGUIToJob(const QSharedPointer<SequenceJob> &job);
        void updateFrameProperties(int reset);
        void updateJobTable(const QSharedPointer<SequenceJob> &job, bool full = false);
        void jobExecutionPreparationStarted();
        void jobPrepared(const QSharedPointer<SequenceJob> &job);
        void captureImageStarted();
        void captureTarget(QString targetName);
        void captureRunning();
        void newExposureProgress(const QSharedPointer<SequenceJob> &job);
        void newDownloadProgress(double downloadTimeLeft);
        void downloadingFrame();
        void updateCaptureCountDown(int deltaMS);
        void darkFrameCompleted();
        void updateMeridianFlipStage(MeridianFlipState::MFStage stage);
        void cameraReady();
        void refreshCamera(bool isValid);
        void refreshCameraSettings();
        void refreshFilterSettings();
        void processingFITSfinished(bool success);
        void rotatorReverseToggled(bool enabled);
        // communication with other modules
        void newImage(const QSharedPointer<SequenceJob> &job, const QSharedPointer<FITSData> &data);
        void newView(const QSharedPointer<FITSView> &view);
        void suspendGuiding();
        void resumeGuiding();
        void abortFocus();
        void captureComplete(const QVariantMap &metadata);
        void sequenceChanged(const QJsonArray &sequence);
        void driverTimedout(const QString &deviceName);
        // new log text for the module log window
        void newLog(const QString &text);


    private:
        QSharedPointer<CameraState> m_State;
        QSharedPointer<CaptureDeviceAdaptor> m_DeviceAdaptor;
        QPointer<DarkProcessor> m_DarkProcessor;
        QSharedPointer<FITSViewer> m_FITSViewerWindow;
        QSharedPointer<StreamWG> m_VideoWindow;
        FitsvViewerTabIDs m_fitsvViewerTabIDs = {-1, -1, -1, -1, -1};
        QElapsedTimer m_CaptureOperationsTimer;

        // Pre-/post capture script process
        QProcess m_CaptureScript;
        QString m_Scope;
        // Flat field automation
        QVector<double> ExpRaw, ADURaw;
        ADUAlgorithm targetADUAlgorithm { ADU_LEAST_SQUARES };


        /**
         * @brief activeJob Shortcut for the module state
         */
        QSharedPointer<CameraState> state() const
        {
            return m_State;
        }

        /**
         * @brief activeJob Shortcut to device adapter
         */
        QSharedPointer<CaptureDeviceAdaptor> devices()
        {
            return m_DeviceAdaptor;
        }

        /**
         * @brief Get or create FITSViewer if we are using FITSViewer
         * or if capture mode is calibrate since for now we are forced to open the file in the viewer
         * this should be fixed in the future and should only use FITSData.
         */
        QSharedPointer<FITSViewer> getFITSViewer();

        /**
         * @brief activeJob Shortcut to the active job held in the state machine
         */
        const QSharedPointer<SequenceJob> &activeJob()
        {
            return  state()->getActiveJob();
        }

        /**
         * @brief activeCamera Shortcut to the active camera held in the device adaptor
         */
        ISD::Camera *activeCamera();

        /**
         * @brief resetAllJobs Iterate over all jobs and reset them.
         */
        void resetAllJobs();
        /**
         * @brief resetJobStatus Reset a single job to the given status
         */
        void resetJobStatus(JOBStatus newStatus);
        /**
         * @brief updatedCaptureCompleted Update the completed captures count to the given
         * number.
         */
        void updatedCaptureCompleted(int count);
        /**
         * @brief Update the video recording status.
         * @param enabled true if recording is on
         */
        void updateVideoRecordStatus(bool enabled);
        /**
         * @brief captureImageWithDelay Helper function that starts the sequence delay timer
         * for starting to capture after the configured delay.
         */
        IPState captureImageWithDelay();
        /**
         * @brief saveReceivedImage Save the received image if the state allows it
         * @return true iff everything worked as expected
         */
        bool checkSavingReceivedImage(const QSharedPointer<FITSData> &data, const QString &extension, QString &filename);

        /**
         * @brief createTabText Create the tab to be displayed in the FITSViewer tab
         */
        QString createTabTitle(const FITSMode &captureMode, const QString &deviceName);

        /**
         * Check capture operations timeout
         */
        void checkCaptureOperationsTimeout(const std::function<void()> &slot);
};
} // Ekos namespace
