/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "ui_camera.h"
#include "ui_limits.h"
#include "ui_calibrationoptions.h"
#include "capturetypes.h"
#include "customproperties.h"
#include "rotatorsettings.h"
#include "sequencejob.h"

namespace
{

// Columns in the job table
enum JobTableColumnIndex
{
    JOBTABLE_COL_STATUS = 0,
    JOBTABLE_COL_FILTER,
    JOBTABLE_COL_COUNTS,
    JOBTABLE_COL_EXP,
    JOBTABLE_COL_TYPE,
    JOBTABLE_COL_BINNING,
    JOBTABLE_COL_ISO,
    JOBTABLE_COL_OFFSET
};
} // namespace

class DSLRInfo;

namespace Ekos
{

class Capture;
class CaptureDeviceAdaptor;
class CameraProcess;
class CameraState;
class ScriptsManager;
class FilterManager;
class SequenceJob;

class Camera : public QWidget, public Ui::Camera
{
    Q_OBJECT
    friend class Capture;
public:

    // default constructor
    explicit Camera(int id = 0, bool standAlone = false, QWidget *parent = nullptr);
    // constructor for standalone editor
    explicit Camera(bool standAlone = false, QWidget *parent = nullptr);
    ~Camera();

    // ////////////////////////////////////////////////////////////////////
    // device control
    // ////////////////////////////////////////////////////////////////////
    // Gain
    // This sets and gets the custom properties target gain
    // it does not access the ccd gain property
    void setGain(double value);
    double getGain();

    void setOffset(double value);
    double getOffset();

    /**
     * @brief Add new Rotator
     * @param name name of the new rotator
    */
    void setRotator(QString name);

    /**
     * @brief addDSLRInfo Save DSLR Info the in the database. If the interactive dialog was open, close it.
     * @param model Camera name
     * @param maxW Maximum width in pixels
     * @param maxH Maximum height in pixels
     * @param pixelW Pixel horizontal size in microns
     * @param pixelH Pizel vertical size in microns
     */
    void addDSLRInfo(const QString &model, uint32_t maxW, uint32_t maxH, double pixelW, double pixelH);

    // ////////////////////////////////////////////////////////////////////
    // Main capturing actions
    // ////////////////////////////////////////////////////////////////////
    /**
     * @brief Start the execution of the Capture::SequenceJob list #jobs.
     *
     * Starting the execution of the Capture::SequenceJob list selects the first job
     * from the list that may be executed and starts to prepare the job (@see prepareJob()).
     *
     * Several factors determine, which of the jobs will be selected:
     * - First, the list is searched to find the first job that is marked as idle or aborted.
     * -  If none is found, it is checked whether ignoring job progress is set. If yes,
     *    all jobs are are reset (@see reset()) and the first one from the list is selected.
     *    If no, the user is asked whether the jobs should be reset. If the user declines,
     *    starting is aborted.
     */
    void start();

    /**
     * Stops currently running jobs:
     *           CAPTURE_IDLE: capture in idle state waiting for further action (e.g. single sequence
     *                         is complete, next one starting)
     *       CAPTURE_COMPLETE: all capture sequences are complete
     *          CAPTURE_ABORT: capture aborted either by user interaction or by a technical error
     *        CAPTURE_SUSPEND: capture suspended and waiting to be restarted
     * @param targetState status of the job after stop
     */
    void stop(CaptureState targetState = CAPTURE_IDLE);

    /**
      * Aborts all jobs and mark current state as ABORTED. It simply calls stop(CAPTURE_ABORTED)
      */
    void abort()
    {
        stop(CAPTURE_ABORTED);
    }

    /**
     * Aborts all jobs and mark current state as SUSPENDED. It simply calls stop(CAPTURE_SUSPENDED)
     * The only difference between SUSPENDED and ABORTED it that capture module can automatically resume a suspended
     * state on its own without external trigger once the right conditions are met. When whatever reason caused the module
     * to go into suspended state ceases to exist, the capture module automatically resumes. On the other hand, ABORTED state
     * must be started via an external programmatic or user trigger (e.g. click the start button again).
     */
    void suspend()
    {
        stop(CAPTURE_SUSPENDED);
    }

    /** DBUS interface function.
     * @brief pause Pauses the Sequence Queue progress AFTER the current capture is complete.
     */
    void pause();

    /**
     * @brief toggleSequence Toggle sequence state depending on its current state.
     * 1. If paused, then resume sequence.
     * 2. If idle or completed, then start sequence.
     * 3. Otherwise, abort current sequence.
     */
    void toggleSequence();

    /**
      * Toggle video streaming if supported by the device.
      * @param enabled Set to true to start video streaming, false to stop it if active.
      */
    void toggleVideo(bool enabled);

    /**
     * @brief restartCamera Restarts the INDI driver associated with a camera. Remote and Local drivers are supported.
     * @param name Name of camera to restart. If a driver defined multiple cameras, they would be removed and added again
     * after driver restart.
     */
    void restartCamera(const QString &name);

    /**
     * @brief capturePreview Capture a single preview image
     */
    void capturePreview();

    /**
     * @brief startFraming Like captureOne but repeating.
     */
    void startFraming();

    /**
     * @brief generateDarkFlats Generate a list of dark flat jobs from available flat frames.
     */
    void generateDarkFlats();

    /**
     * @brief setDownloadProgress update the Capture Module and Summary
     *        Screen's estimate of how much time is left in the download
     */
    void updateDownloadProgress(double downloadTimeLeft, const QString &devicename);

    void updateCaptureCountDown(int deltaMillis);

    // ////////////////////////////////////////////////////////////////////
    // Job handling
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief createJob Create a new job with the settings given in the GUI.
     * @param jobtype batch, preview, looping or dark flat job.
     * @param filenamePreview if the job is to generate a preview filename
     * @return pointer to job created or nullptr otherwise.
     */
    QSharedPointer<SequenceJob> createJob(SequenceJob::SequenceJobType jobtype = SequenceJob::JOBTYPE_BATCH,
                           FilenamePreviewType filenamePreview = FILENAME_NOT_PREVIEW);

        /**
     * @brief removeJob Remove a job sequence from the queue
     * @param index Row index for job to remove, if left as -1 (default), the currently selected row will be removed.
     *        if no row is selected, the last job shall be removed.
     * @param true if sequence is removed. False otherwise.
     */
    Q_INVOKABLE bool removeJob(int index = -1);

    /**
     * @brief modifyJob Select a job and enter edit state
     * @param index Row index for job to edit, if left as -1 (default), the currently selected row will be edited.
     * @param true if sequence is in edit state. False otherwise.
     */
    Q_INVOKABLE bool modifyJob(int index = -1);

    // ////////////////////////////////////////////////////////////////////
    // Process control
    // ////////////////////////////////////////////////////////////////////
    /**
      * Enables or disables the maximum guiding deviation and sets its value.
      * @param enable If true, enable the guiding deviation check, otherwise, disable it.
      * @param value if enable is true, it sets the maximum guiding deviation in arcsecs. If the value is exceeded, the capture operation is aborted until the value falls below the value threshold.
      */
    void setMaximumGuidingDeviation(bool enable, double value);

    /**
      * Enables or disables the in sequence focus and sets Half-Flux-Radius (HFR) limit.
      * @param enable If true, enable the in sequence auto focus check, otherwise, disable it.
      * @param HFR if enable is true, it sets HFR in pixels. After each exposure, the HFR is re-measured and if it exceeds the specified value, an autofocus operation will be commanded.
         */
    void setInSequenceFocus(bool enable, double HFR);

    /**
     * Loads the Ekos Sequence Queue file in the Sequence Queue. Jobs are appended to existing jobs.
     * @param fileURL full URL of the filename
     * @param targetName override the target in the sequence queue file (necessary for using the target of the scheduler)
     */
     bool loadSequenceQueue(const QString &fileURL, QString targetName = "");

    /**
     * Saves the Sequence Queue to the Ekos Sequence Queue file.
     * @param fileURL full URL of the filename
     */
    bool saveSequenceQueue(const QString &path);

    /**
     * @brief Update the camera's capture state
     */
    void updateCameraStatus(CaptureState status);

    /**
     * Aborts any current jobs and remove all sequence queue jobs.
     */
    void clearSequenceQueue();

    // shortcuts
    void loadSequenceQueue();
    void saveSequenceQueue();
    void saveSequenceQueueAs();

    QVariantMap getAllSettings() const;
    void setAllSettings(const QVariantMap &settings, const QVariantMap *standAloneSettings = nullptr);

    // ////////////////////////////////////////////////////////////////////
    // Optical Train handling
    // ////////////////////////////////////////////////////////////////////
    void setupOpticalTrainManager();
    void initOpticalTrain();
    void refreshOpticalTrain(const int id);


    QString opticalTrain() const
    {
        return opticalTrainCombo->currentText();
    }

    /**
     * @brief Select the optical train
     */
    void selectOpticalTrain(QString name);

    // Utilities for storing stand-alone variables.
    void storeTrainKey(const QString &key, const QStringList &list);
    void storeTrainKeyString(const QString &key, const QString &str);

    // ////////////////////////////////////////////////////////////////////
    // Filter Manager and filters
    // ////////////////////////////////////////////////////////////////////
    void setupFilterManager();
    void clearFilterManager();

    /**
     * @brief checkFilter Refreshes the filter wheel information in the capture module.
     */
    void refreshFilterSettings();

    const QSharedPointer<FilterManager> &filterManager() const
    {
        return m_FilterManager;
    }

    /**
     * @brief shortcut for updating the current filter information for the state machine
     */
    void updateCurrentFilterPosition();

    /**
     * @brief Add new Filter Wheel
     * @param name device name of the new filter wheel
    */
    void setFilterWheel(QString name);

    // ////////////////////////////////////////////////////////////////////
    // Devices and process engine
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief activeJob Shortcut to device adapter
     */
    QSharedPointer<CaptureDeviceAdaptor> devices()
    {
        return m_DeviceAdaptor;
    }

    QSharedPointer<CameraProcess> process()
    {
        return m_cameraProcess;
    }

    // shortcut for the module state
    QSharedPointer<CameraState> state() const
    {
        return m_cameraState;
    }

    // shortcut for the active job
    const QSharedPointer<SequenceJob> &activeJob()
    {
        return state()->getActiveJob();
    }

    // Shortcut to the active camera held in the device adaptor
    ISD::Camera *activeCamera();

    /**
     * @brief currentScope Retrieve the scope parameters from the optical train.
     */
    QJsonObject currentScope();

    /**
     * @brief currentReducer Retrieve the reducer parameters from the optical train.
     */
    double currentReducer();

    /**
     * @brief currentAperture Determine the current aperture
     * @return
     */
    double currentAperture();

    void setForceTemperature(bool enabled)
    {
        cameraTemperatureS->setChecked(enabled);
    }

    // Propagate meridian flip state changes to the UI
    void updateMeridianFlipStage(MeridianFlipState::MFStage stage);

    // ////////////////////////////////////////////////////////////////////
    // sub dialogs
    // ////////////////////////////////////////////////////////////////////

    void openExposureCalculatorDialog();

    // Script Manager
    void handleScriptsManager();

    /**
     * @brief showTemperatureRegulation Toggle temperature regulation dialog which sets temperature ramp and threshold
     */
    void showTemperatureRegulation();

    void createDSLRDialog();

    // Observer
    void showObserverDialog();

    // ////////////////////////////////////////////////////////////////////
    // Standalone editor
    // ////////////////////////////////////////////////////////////////////
    /**
     *  Gets called when the stand-alone editor gets a show event.
     * Do this initialization here so that if the live capture module was
     * used after startup, it will have set more recent remembered values.
     */
    void onStandAloneShow(QShowEvent* event);

    bool m_standAlone {false};
    void setStandAloneGain(double value);
    void setStandAloneOffset(double value);

    CustomProperties *customPropertiesDialog() const
    {
        return m_customPropertiesDialog.get();
    }


    // ////////////////////////////////////////////////////////////////////
    // Access to properties
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief Set the observer name.
     */
    void setObserverName(const QString &value)
    {
        state()->setObserverName(value);
    };

    QString getObserverName()
    {
        return state()->observerName();
    }

    QVariantMap &settings()
    {
        return m_settings;
    }
    void setSettings(const QVariantMap &newSettings)
    {
        m_settings = newSettings;
    }

    int cameraId() const
    {
        return m_cameraId;
    }

signals:
    // communication with other modules
    void ready();
    void requestAction(int cameraID, CaptureWorkflowActionType action);
    void refreshCamera(uint id, bool isValid);
    void newExposureProgress(const QSharedPointer<SequenceJob> &job, const QString &trainname);
    void newDownloadProgress(double, const QString &trainname);
    void newImage(const QSharedPointer<SequenceJob> &job, const QSharedPointer<FITSData> &data, const QString &trainname);
    void captureTarget(QString targetName);
    void captureComplete(const QVariantMap &metadata, const QString &trainname);
    void resetNonGuidedDither();
    void runAutoFocus(AutofocusReason autofocusReason, const QString &reasonInfo, const QString &trainname);
    void resetFocusFrame(const QString &trainname);
    void abortFocus(const QString &trainname);
    void adaptiveFocus(const QString &trainname);
    void settingsUpdated(const QVariantMap &settings);
    void sequenceChanged(const QJsonArray &sequence);
    void newLocalPreview(const QString &preview);
    void dslrInfoRequested(const QString &cameraName);
    void filterManagerUpdated(ISD::FilterWheel *device);
    void newFilterStatus(FilterState state);
    void trainChanged();
    void newLog(const QString &text);

    // Signals for the Analyze tab.
    void captureStarting(double exposureSeconds, const QString &filter);
    void captureAborted(double exposureSeconds);

    // communication with other modules
    void checkFocus(double, const QString &trainname);
    void meridianFlipStarted(const QString &trainname);
    void guideAfterMeridianFlip();
    void newStatus(CaptureState status, const QString &devicename, int cameraID);
    void suspendGuiding();
    void resumeGuiding();
    void driverTimedout(const QString &deviceName);

private slots:
    // ////////////////////////////////////////////////////////////////////
    // slots handling device and module events
    // ////////////////////////////////////////////////////////////////////

    void setVideoStreamEnabled(bool enabled);

    // Cooler
    void setCoolerToggled(bool enabled);

    // Filter
    void setFilterStatus(FilterState filterState);

    // Jobs
    void resetJobs();
    bool selectJob(QModelIndex i);
    void editJob(QModelIndex i);
    void resetJobEdit(bool cancelled = false);

private:

    /**
     * @brief initCamera Initialize all camera settings
     */
    void initCamera();
    // ////////////////////////////////////////////////////////////////////
    // Device updates
    // ////////////////////////////////////////////////////////////////////
    // Rotator
    void updateRotatorAngle(double value);
    void setRotatorReversed(bool toggled);

    /**
     * @brief updateCCDTemperature Update CCD temperature in capture module.
     * @param value Temperature in celcius.
     */
    void updateCCDTemperature(double value);

    // Auto Focus
    /**
     * @brief setFocusStatus Forward the new focus state to the capture module state machine
     */
    void setFocusStatus(FocusState newstate);

    /**
     * @brief updateFocusStatus Handle new focus state
     */
    void updateFocusStatus(FocusState newstate);

    // Adaptive Focus
    /**
     * @brief focusAdaptiveComplete Forward the new focus state to the capture module state machine
     */
    void focusAdaptiveComplete(bool success)
    {
        // directly forward it to the state machine
        state()->updateAdaptiveFocusState(success);
    }

    /**
     * @brief New camera selected
     * @param isValid is the selected camera valid.
    */
    void updateCamera(bool isValid);

    /**
     * @brief checkCamera Refreshes the CCD information in the capture module.
     */
    void refreshCameraSettings();

    /**
     * @brief processCCDNumber Process number properties arriving from CCD. Currently, only CCD and Guider frames are processed.
     * @param nvp pointer to number property.
     */
    void processCameraNumber(INDI::Property prop);

    /**
     * @brief Slot receiving the update of the current target distance.
     * @param targetDiff distance to the target in arcseconds.
     */
    void updateTargetDistance(double targetDiff);

    // ////////////////////////////////////////////////////////////////////
    // Capture actions
    // ////////////////////////////////////////////////////////////////////
    /**
     * @brief captureStarted Change the UI after the capturing process
     * has been started.
     */
    void jobStarting();

    /**
     * @brief addJob Add a new job to the UI. This is used when a job is loaded from a capture sequence file. In
     * contrast to {@see #createJob()}, the job's attributes are taken from the file and only the UI gehts updated.
     */
    void addJob(const QSharedPointer<SequenceJob> &job);

    /**
     * @brief jobEditFinished Editing of an existing job finished, update its
     *        attributes from the UI settings. The job under edit is taken from the
     *        selection in the job table.
     * @return true if job updated succeeded.
     */
    Q_INVOKABLE void editJobFinished();

    /**
     * @brief imageCapturingCompleted Capturing a single frame completed
     */
    Q_INVOKABLE void imageCapturingCompleted();

    /**
     * @brief captureStopped Capturing has stopped
     */
    Q_INVOKABLE void captureStopped();

    /**
     * @brief processFITSfinished processing new FITS data received from camera finished.
     * @param success true iff processing was successful
     */
    Q_INVOKABLE void processingFITSfinished(bool success);

    /**
     * @brief captureRunning Manage the result when capturing has been started
     */
    Q_INVOKABLE void captureRunning();

    /**
     * @brief captureImageStarted Image capturing for the active job has started.
     */
    Q_INVOKABLE void captureImageStarted();

    /**
     * @brief jobPreparationStarted Preparation actions for the current active job have beenstarted.
     */
    Q_INVOKABLE void jobExecutionPreparationStarted();

    /**
     * @brief jobPrepared Select the job that is currently in preparation.
     */
    void jobPrepared(const QSharedPointer<SequenceJob> &job);

    /**
     * @brief Set the name of the target to be captured.
     */
    void setTargetName(const QString &newTargetName);

    // ////////////////////////////////////////////////////////////////////
    // UI controls
    // ////////////////////////////////////////////////////////////////////
    void checkFrameType(int index);

    /**
     * @brief updateStartButtons Update the start and the pause button to new states of capturing
     * @param start start capturing
     * @param pause pause capturing
     */
    void updateStartButtons(bool start, bool pause = false);

    void setBusy(bool enable);

    /**
     * @brief Listen to device property changes (temperature, rotator) that are triggered by
     *        SequenceJob.
     */
    void updatePrepareState(CaptureState prepareState);

    /**
     * @brief updateJobTable Update the table row values for the given sequence job. If the job
     * is null, all rows will be updated
     * @param job as identifier for the row
     * @param full if false, then only the status and the counter will be updated.
     */
    void updateJobTable(const QSharedPointer<SequenceJob> &job, bool full = false);

    /**
     * @brief updateJobFromUI Update all job attributes from the UI settings.
     */
    void updateJobFromUI(const QSharedPointer<SequenceJob> &job, FilenamePreviewType filenamePreview = FILENAME_NOT_PREVIEW);

    /**
     * @brief syncGUIToJob Update UI to job settings
     */
    void syncGUIToJob(const QSharedPointer<SequenceJob> &job);

    void syncCameraInfo();

    // create a new row in the job table and fill it with the given job's values
    void createNewJobTableRow(const QSharedPointer<SequenceJob> &job);

    /**
     * @brief Update the style of the job's row, depending on the job's state
     */
    void updateRowStyle(const QSharedPointer<SequenceJob> &job);

    /**
     * @brief updateCellStyle Update the cell's style. If active is true, set a bold and italic font and
     * a regular font otherwise.
     */
    void updateCellStyle(QTableWidgetItem *cell, bool active);

    /**
     * @brief syncControl Sync setting to widget. The value depends on the widget type.
     * @param settings Map of all settings
     * @param key name of widget to sync
     * @param widget pointer of widget to set
     * @return True if sync successful, false otherwise
     */
    bool syncControl(const QVariantMap &settings, const QString &key, QWidget * widget);

    /**
      * @brief moveJobUp Move the job in the sequence queue one place up or down.
      */
    void moveJob(bool up);


    void removeJobFromQueue();

    void saveFITSDirectory();

    /**
     * @brief updateCaptureFormats Update encoding and transfer formats
     */
    void updateCaptureFormats();

    /**
     * @brief updateHFRCheckAlgo Update the in-sequence HFR check algorithm
     */
    void updateHFRCheckAlgo();

    /**
      * Clear in-sequence focus settings. It sets the autofocus HFR to zero so that next autofocus value is remembered for the in-sequence focusing.
      */
    void clearAutoFocusHFR();

    // selection of a job
    void selectedJobChanged(QModelIndex current, QModelIndex previous);

    // Clear Camera Configuration
    void clearCameraConfiguration();

    // Change filter name in INDI
    void editFilterName();
    bool editFilterNameInternal(const QStringList &labels, QStringList &newLabels);

    void updateVideoDurationUnit();

    // ////////////////////////////////////////////////////////////////////
    // Settings
    // ////////////////////////////////////////////////////////////////////
    /**
     * @brief loadSettings Load setting from Options and set them accordingly.
     */
    void loadGlobalSettings();

    /**
     * @brief syncLimitSettings Update Limits UI from Options
     */
    void syncLimitSettings();

    /**
     * @brief settleSettings Run this function after timeout from debounce timer to update database
     * and emit settingsChanged signal. This is required so we don't overload output.
     */
    void settleSettings();

    /**
     * @brief syncSettings When checkboxes, comboboxes, or spin boxes are updated, save their values in the
     * global and per-train settings.
     */
    void syncSettings();

    /**
     * @brief Connect GUI elements to sync settings once updated.
     */
    void connectSyncSettings();
    /**
     * @brief Stop updating settings when GUI elements are updated.
     */
    void disconnectSyncSettings();

    // ////////////////////////////////////////////////////////////////////
    // helper functions
    // ////////////////////////////////////////////////////////////////////
    // object initializstion
    void init();
    // camera device name
    QString getCameraName()
    {
        return activeCamera() == nullptr ? "" : activeCamera()->getDeviceName();
    }

    // check if the upload paths are filled correctly
    bool checkUploadPaths(FilenamePreviewType filenamePreview, const QSharedPointer<SequenceJob> &job);

    // Create a Json job from the current job table row
    QJsonObject createJsonJob(const QSharedPointer<SequenceJob> &job, int currentRow);

    /**
     * @return Returns true if an ongoing capture is a preview capture.
     */
    bool isActiveJobPreview()
    {
        return state() && state()->isActiveJobPreview();
    }

    // Filename preview
    void generatePreviewFilename();
    QString previewFilename(FilenamePreviewType previewType = FILENAME_LOCAL_PREVIEW);

    // Updating the upload mode
    void selectUploadMode(int index);
    void checkUploadMode(int index);

    /**
     * @brief updateJobTableCountCell Update the job counter in the job table of a sigle job
     */
    void updateJobTableCountCell(const QSharedPointer<SequenceJob> &job, QTableWidgetItem *countCell);

    void cullToDSLRLimits();

    void resetFrameToZero();

    // reset = 0 --> Do not reset
    // reset = 1 --> Full reset
    // reset = 2 --> Only update limits if needed
    void updateFrameProperties(int reset = 0);

    // ////////////////////////////////////////////////////////////////////
    // Sub dialogs
    // ////////////////////////////////////////////////////////////////////
    QSharedPointer<FilterManager> m_FilterManager;
    std::unique_ptr<Ui::Limits> m_LimitsUI;
    QPointer<QDialog> m_LimitsDialog;
    std::unique_ptr<Ui::Calibration> m_CalibrationUI;
    QPointer<QDialog> m_CalibrationDialog;
    QPointer<ScriptsManager> m_scriptsManager;
    QSharedPointer<RotatorSettings> m_RotatorControlPanel;
    std::unique_ptr<DSLRInfo> m_DSLRInfoDialog;

    // ////////////////////////////////////////////////////////////////////
    // Module logging
    // ////////////////////////////////////////////////////////////////////
    void appendLogText(const QString &text)
    {
        emit newLog(QString("[%1] ").arg(getCameraName()) + text);
    }

    // ////////////////////////////////////////////////////////////////////
    // Attributes
    // ////////////////////////////////////////////////////////////////////
    int m_cameraId;
    QVariantMap m_settings;
    QVariantMap m_GlobalSettings;
    std::unique_ptr<CustomProperties> m_customPropertiesDialog;
    QTimer m_DebounceTimer;

    bool m_standAloneUseCcdGain { true};
    bool m_standAloneUseCcdOffset { true};
    bool m_JobUnderEdit { false };

    QUrl m_dirPath;

    QSharedPointer<CaptureDeviceAdaptor> m_DeviceAdaptor;
    QSharedPointer<CameraProcess> m_cameraProcess;
    QSharedPointer<CameraState> m_cameraState;

    // Controls
    double GainSpinSpecialValue { INVALID_VALUE };
    double OffsetSpinSpecialValue { INVALID_VALUE };
};
} // namespace Ekos
