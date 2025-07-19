/*  Ekos state machine for the Capture module
    SPDX-FileCopyrightText: 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include "Options.h"

#include "capturetypes.h"
#include "ekos/ekos.h"
#include "indiapi.h"
#include "indi/indistd.h"
#include "indi/indidustcap.h"
#include "indi/indicamera.h"
#include "indi/indimount.h"
#include "indi/indidome.h"

#include "ekos/manager/meridianflipstate.h"
#include "ekos/auxiliary/filtermanager.h"
#include "ekos/scheduler/schedulertypes.h"
#include "ekos/capture/refocusstate.h"
#include "ekos/focus/focusutils.h"
#include "placeholderpath.h"

// Wait 3-minutes as maximum beyond exposure
// value.
#define CAPTURE_TIMEOUT_THRESHOLD  180000

class FITSData;

namespace Ekos
{

constexpr double HFR_CHECK_DEFAULT_THRESHOLD = 10.0;

class SequenceJob;
class SequenceQueue;
class CaptureDeviceAdaptor;
class RefocusState;

class CameraState: public QObject
{
        Q_OBJECT
    public:
        /* Interval with double lower and upper bound */
        typedef struct
        {
            double min, max;
        } DoubleRange;

        CameraState(QObject *parent = nullptr);

        // ////////////////////////////////////////////////////////////////////
        // sequence jobs
        // ////////////////////////////////////////////////////////////////////
        QList<QSharedPointer<SequenceJob> > &allJobs();

        QSharedPointer<SequenceQueue> getSequenceQueue()
        {
            return m_sequenceQueue;
        }

        const QSharedPointer<SequenceJob> &getActiveJob()
        {
            return m_activeJob;
        }
        void setActiveJob(const QSharedPointer<SequenceJob> &value);

        const QUrl &sequenceURL() const;
        void setSequenceURL(const QUrl &newSequenceURL);

        // ////////////////////////////////////////////////////////////////////
        // pointer to last captured frame
        // ////////////////////////////////////////////////////////////////////
        QSharedPointer<FITSData> imageData()
        {
            return m_ImageData;
        }
        void setImageData(QSharedPointer<FITSData> newImageData)
        {
            m_ImageData = newImageData;
        }

        // ////////////////////////////////////////////////////////////////////
        // capture attributes
        // ////////////////////////////////////////////////////////////////////
        // current filter ID
        int currentFilterID { Ekos::INVALID_VALUE };
        // Map tracking whether the current value has been initialized.
        // With this construct we could do a lazy initialization of current values if setCurrent...()
        // sets this flag to true. This is necessary since we listen to the events, but as long as
        // the value does not change, there won't be an event.
        QMap<CaptureWorkflowActionType, bool> isInitialized;

        // ////////////////////////////////////////////////////////////////////
        // flat preparation attributes
        // ////////////////////////////////////////////////////////////////////
        // flag if telescope has been covered
        typedef enum
        {
            MANAUL_COVER_OPEN,
            MANUAL_COVER_CLOSED_LIGHT,
            MANUAL_COVER_CLOSED_DARK
        } ManualCoverState;

        ManualCoverState m_ManualCoverState { MANAUL_COVER_OPEN };
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
        // state changes
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief captureStarted The preparation to capture have been started.
         */
        void initCapturePreparation();

        // ////////////////////////////////////////////////////////////////////
        // state accessors
        // ////////////////////////////////////////////////////////////////////
        CaptureState getCaptureState() const
        {
            return m_CaptureState;
        }
        void setCaptureState(CaptureState value);

        bool isStartingCapture() const
        {
            return m_StartingCapture;
        }
        void setStartingCapture(bool newStartingCapture)
        {
            m_StartingCapture = newStartingCapture;
        }

        CaptureContinueAction getContinueAction() const
        {
            return m_ContinueAction;
        }
        void setContinueAction(CaptureContinueAction newPauseFunction)
        {
            m_ContinueAction = newPauseFunction;
        }

        FocusState getFocusState() const
        {
            return m_FocusState;
        }
        void setFocusState(FocusState value)
        {
            m_FocusState = value;
        }

        GuideState getGuideState() const
        {
            return m_GuideState;
        }
        void setGuideState(GuideState state);

        // short cut for all guiding states that indicate guiding is on
        bool isGuidingOn();
        // short cut for all guiding states that indicate guiding in state GUIDING
        bool isActivelyGuiding();

        bool useGuideHead() const
        {
            return m_useGuideHead;
        }
        void setUseGuideHead(bool value)
        {
            m_useGuideHead = value;
        }
        bool isGuidingDeviationDetected() const
        {
            return m_GuidingDeviationDetected;
        }
        void setGuidingDeviationDetected(bool newDeviationDetected)
        {
            m_GuidingDeviationDetected = newDeviationDetected;
        }

        bool suspendGuidingOnDownload() const
        {
            return m_SuspendGuidingOnDownload;
        }
        void setSuspendGuidingOnDownload(bool value)
        {
            m_SuspendGuidingOnDownload = value;
        }

        uint SpikesDetected() const
        {
            return m_SpikesDetected;
        }
        uint increaseSpikesDetected()
        {
            return ++m_SpikesDetected;
        }
        void resetSpikesDetected()
        {
            m_SpikesDetected = 0;
        }

        IPState getDitheringState() const
        {
            return m_DitheringState;
        }
        void setDitheringState(IPState value)
        {
            m_DitheringState = value;
        }

        AlignState getAlignState() const
        {
            return m_AlignState;
        }
        void setAlignState(AlignState value);

        FilterState getFilterManagerState() const
        {
            return m_FilterManagerState;
        }
        void setFilterManagerState(FilterState value)
        {
            m_FilterManagerState = value;
        }

        int getCurrentFilterPosition() const
        {
            return m_CurrentFilterPosition;
        }

        const QString &getCurrentFilterName() const
        {
            return m_CurrentFilterName;
        }

        const QString &CurrentFocusFilterName() const
        {
            return m_CurrentFocusFilterName;
        }

        void setCurrentFilterPosition(int position, const QString &name, const QString &focusFilterName);

        LightState getLightBoxLightState() const
        {
            return m_lightBoxLightState;
        }
        void setLightBoxLightState(LightState value)
        {
            m_lightBoxLightState = value;
        }

        bool lightBoxLightEnabled() const
        {
            return m_lightBoxLightEnabled;
        }
        void setLightBoxLightEnabled(bool value)
        {
            m_lightBoxLightEnabled = value;
        }

        /**
         * @brief dust cap status change
         */
        void dustCapStateChanged(ISD::DustCap::Status status);

        CapState getDustCapState() const
        {
            return m_dustCapState;
        }
        void setDustCapState(CapState value)
        {
            m_dustCapState = value;
        }

        ISD::Mount::Status getScopeState() const
        {
            return m_scopeState;
        }
        void setScopeState(ISD::Mount::Status value)
        {
            m_scopeState = value;
        }

        ISD::Mount::PierSide getPierSide() const
        {
            return m_pierSide;
        }
        void setPierSide(ISD::Mount::PierSide value)
        {
            m_pierSide = value;
        }

        ISD::ParkStatus getScopeParkState() const
        {
            return m_scopeParkState;
        }
        void setScopeParkState(ISD::ParkStatus value)
        {
            m_scopeParkState = value;
        }

        ISD::Dome::Status getDomeState() const
        {
            return m_domeState;
        }
        void setDomeState(ISD::Dome::Status value)
        {
            m_domeState = value;
        }

        QSharedPointer<MeridianFlipState> getMeridianFlipState();
        void setMeridianFlipState(QSharedPointer<MeridianFlipState> state);

        QSharedPointer<RefocusState> getRefocusState() const
        {
            return m_refocusState;
        }

        const QString &observerName() const
        {
            return m_ObserverName;
        }
        void setObserverName(const QString &value);

        bool ignoreJobProgress() const
        {
            return m_ignoreJobProgress;
        }
        void setIgnoreJobProgress(bool value)
        {
            m_ignoreJobProgress = value;
        }

        bool isRememberFastExposure() const
        {
            return m_RememberFastExposure;
        }
        void setRememberFastExposure(bool value)
        {
            m_RememberFastExposure = value;
        }

        bool dirty() const
        {
            return m_Dirty;
        }
        void setDirty(bool value)
        {
            m_Dirty = value;
        }

        bool isBusy() const
        {
            return m_Busy;
        }
        void setBusy(bool busy);

        bool isLooping() const
        {
            return m_Looping;
        }
        void setLooping(bool newLooping)
        {
            m_Looping = newLooping;
        }

        QJsonArray &getSequence()
        {
            return m_SequenceArray;
        }
        void setSequence(const QJsonArray &value)
        {
            m_SequenceArray = value;
        }

        PlaceholderPath &placeholderPath()
        {
            return m_currentPlaceholderPath;
        }
        void initPlaceholderPath()
        {
            m_currentPlaceholderPath = PlaceholderPath();
        }

        /**
         * @brief generateFilename generate the final file name for storing captured frames locally.
         * @param extension file name extension (including ".")
         * @param filename resulting file name
         * @return
         */
        bool generateFilename(const QString &extension, QString *filename);

        // ////////////////////////////////////////////////////////////////////
        // counters
        // ////////////////////////////////////////////////////////////////////

        int getAlignmentRetries() const
        {
            return m_AlignmentRetries;
        }
        int increaseAlignmentRetries()
        {
            return ++m_AlignmentRetries;
        }
        void resetAlignmentRetries()
        {
            m_AlignmentRetries = 0;
        }

        int getDitherCounter() const
        {
            return m_ditherCounter;
        }
        void decreaseDitherCounter();

        /**
         * @brief resetDitherCounter Reset the dither counter to its start value. If a per job counter is
         * set to > 0, this value is used and the general dither counter otherwise.
         */
        void resetDitherCounter();

        int nextSequenceID() const
        {
            return m_nextSequenceID;
        }
        void setNextSequenceID(int id)
        {
            m_nextSequenceID = id;
        }

        uint16_t capturedFramesCount(const QString &signature) const
        {
            return m_capturedFramesMap[signature];
        }
        void setCapturedFramesCount(const QString &signature, uint16_t count);

        double lastRemainingFrameTimeMS() const
        {
            return m_lastRemainingFrameTimeMS;
        }
        void setLastRemainingFrameTimeMS(double value)
        {
            m_lastRemainingFrameTimeMS = value;
        }

        // ////////////////////////////////////////////////////////////////////
        // Timers
        // ////////////////////////////////////////////////////////////////////
        QTimer &getCaptureTimeout()
        {
            return m_captureTimeout;
        }
        uint8_t captureTimeoutCounter() const
        {
            return m_CaptureTimeoutCounter;
        }
        void setCaptureTimeoutCounter(uint8_t value)
        {
            m_CaptureTimeoutCounter = value;
        }
        uint8_t deviceRestartCounter() const
        {
            return m_DeviceRestartCounter;
        }
        void setDeviceRestartCounter(uint8_t value)
        {
            m_DeviceRestartCounter = value;
        }
        QTimer &downloadProgressTimer()
        {
            return m_downloadProgressTimer;
        }
        QElapsedTimer &downloadTimer()
        {
            return m_DownloadTimer;
        }
        QTimer &getCaptureDelayTimer()
        {
            return m_captureDelayTimer;
        }
        QTimer &getGuideDeviationTimer()
        {
            return m_guideDeviationTimer;
        }

        QTimer &getGuideSettleTimer()
        {
            return m_guideSettleTimer;
        }

        QTime &imageCountDown()
        {
            return m_imageCountDown;
        }
        void imageCountDownAddMSecs(int value)
        {
            m_imageCountDown = m_imageCountDown.addMSecs(value);
        }

        QTime &sequenceCountDown()
        {
            return m_sequenceCountDown;
        }
        void sequenceCountDownAddMSecs(int value)
        {
            m_sequenceCountDown = m_sequenceCountDown.addMSecs(value);
        }

        /**
         * @brief changeSequenceValue Change a single sequence in the sequence array
         * @param index position in the array
         * @param key sequence key
         * @param value new sequence value
         */
        void changeSequenceValue(int index, QString key, QString value);

        // ////////////////////////////////////////////////////////////////////
        // Action checks
        // ////////////////////////////////////////////////////////////////////
        /**
             * @brief Check, whether dithering is necessary and, in that case initiate it.
             *
             *  Dithering is only required for batch images and does not apply for PREVIEW.
             *
             * There are several situations that determine, if dithering is necessary:
             * 1. the current job captures light frames AND the dither counter has reached 0 AND
             * 2. guiding is running OR the manual dithering option is selected AND
             * 3. there is a guiding camera active AND
             * 4. there hasn't just a meridian flip been finised.
             *
             * @return true iff dithering is necessary.
             */

        bool checkDithering();

        bool checkCapturing()
        {
            return (m_CaptureState == CAPTURE_CAPTURING || m_CaptureState == CAPTURE_PAUSE_PLANNED);
        }

        /**
         * @brief updateMFMountState Handle changes of the meridian flip mount state
         */
        void updateMFMountState(MeridianFlipState::MeridianFlipMountState status);

        /**
         * @brief updateMeridianFlipStage Update the meridian flip stage
         */
        void updateMeridianFlipStage(const MeridianFlipState::MFStage &stage);

        /**
         * @brief checkMeridianFlipActive
         * @return true iff the meridian flip itself or post flip actions are running
         */
        bool checkMeridianFlipActive();

        /**
         * @brief Check whether a meridian flip has been requested and trigger it
         * @return true iff a meridian flip has been triggered
         */
        bool checkMeridianFlipReady();

        /**
         * @brief checkPostMeridianFlipActions Execute the checks necessary after the mount
         * has completed the meridian flip.
         * @return true iff the post meridian flip actions are ongoing, false if completed or not necessary
         */
        bool checkPostMeridianFlipActions();

        /**
         * @brief Check if an alignment needs to be executed after completing
         * a meridian flip.
         * @return
         */
        bool checkAlignmentAfterFlip();

        /**
         * @brief checkGuideDeviationTimeout Handle timeout when no guide deviation has been received.
         */
        void checkGuideDeviationTimeout();

        /**
         * @brief Check if the mount's flip has been completed and start guiding
         * if necessary. Starting guiding after the meridian flip works through
         * the signal {@see startGuidingAfterFlip()}
         * @return true if guiding needs to start but is not running yet
         */
        bool checkGuidingAfterFlip();

        /**
         * @brief processGuidingFailed React when guiding failed.
         *
         * If aguiding has been started before and stopped, capturing aborts except
         * for the case that either
         *  - a meridian flip is running
         *  - a job is running for non light frames
         *  - capturing is either paused or suspended
         *  In these case, nothing is done.
         */
        void processGuidingFailed();

        /**
         * @brief Process changes necessary when the focus state changes.
         */
        void updateFocusState(FocusState state);

        /**
         * @brief Check if focusing is running (abbreviating function for focus state
         * neither idle nor completed nor aborted).
         */
        bool checkFocusRunning()
        {
            return (m_FocusState != FOCUS_IDLE && m_FocusState != FOCUS_COMPLETE && m_FocusState != FOCUS_ABORTED);
        }

        /**
         * @brief Converts from RefocusReason (used by Capture) to AutofocusReason (used by Focus)
         * @param reasonInfo for the associated reason code
         * @return Reason code for the Autofocus
         */
        AutofocusReason getAFReason(RefocusState::RefocusReason state, QString &reasonInfo);

        /**
         * @brief Start focusing if necessary (see {@see RefocusState#checkFocusRequired()}).
         * @return TRUE if we need to run focusing, false if not necessary
         */
        bool startFocusIfRequired();

        /**
         * @brief Start adaptive focus if necessary
         * @return TRUE if we need adaptive focus, false if not necessary
         */

        void updateAdaptiveFocusState(bool success);

        /**
         * @brief calculate new HFR threshold based on median value for current selected filter
         */
        void updateHFRThreshold();

        /**
         * @brief get the focus filter for the currently active capture filter
         */
        QString getFocusFilterName();

        /**
         * @brief Slot that listens to guiding deviations reported by the Guide module.
         *
         * Depending on the current status, it triggers several actions:
         * - If there is no active job, it calls {@see CameraState::checkMeridianFlipReady()}, which may initiate a meridian flip.
         * - If guiding has been started after a meridian flip and the deviation is within the expected limits,
         *   the meridian flip is regarded as completed by setMeridianFlipStage(MF_NONE) (@see setMeridianFlipStage()).
         * - If the deviation is beyond the defined limit, capturing is suspended (@see suspend()) and the
         *   #guideDeviationTimer is started.
         * - Otherwise, it checks if there has been a job suspended and restarts it, since guiding is within the limits.
         */

        void setGuideDeviation(double deviation_rms);

        /**
         * @brief addDownloadTime Record a new download time
         */
        void addDownloadTime(double time);

        /**
         * @brief averageDownloadTime Determine the average download time
         * @return
         */
        double averageDownloadTime()
        {
            return (downloadsCounter == 0 ? 0 : totalDownloadTime / downloadsCounter);
        }

        /**
         * @brief setDarkFlatExposure Given a dark flat job, find the exposure suitable from it by searching for
         * completed flat frames.
         * @param job Dark flat job
         * @return True if a matching exposure is found and set, false otherwise.
         * @warning This only works if the flat frames were captured in the same live session.
         * If the flat frames were captured in another session (i.e. Ekos restarted), then all automatic exposure
         * calculation results are discarded since Ekos does not save this information to the sequene file.
         * Possible solution is to write to a local config file to keep this information persist between sessions.
         */
        bool setDarkFlatExposure(const QSharedPointer<SequenceJob> &job);

        /**
         * @brief checkSeqBoundary Determine the next file number sequence.
         *        That is, if we have file1.png and file2.png, then the next
         *        sequence should be file3.png.
         */
        void checkSeqBoundary();
        /**
         * @brief isModelinDSLRInfo Check if the DSLR model is already known
         */
        bool isModelinDSLRInfo(const QString &model);

        // ////////////////////////////////////////////////////////////////////
        // Helper functions
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief activeJobID Determine the ID of the currently active job
         */
        int activeJobID();

        /**
         * @brief pPendingJobCount Returns the number of pending uncompleted jobs in the sequence queue.
         */
        int pendingJobCount();

        /**
         * @brief getJobState Returns the job state (Idle, In Progress, Error, Aborted, Complete)
         * @param id job number. Job IDs start from 0 to N-1.
         */

        QString jobState(int id);

        /**
          * @brief jobFilterName Returns the job filter name.
          * @param id job number. Job IDs start from 0 to N-1.
          */
        QString jobFilterName(int id);

        /**
         * @param id job number. Job IDs start from 0 to N-1.
         * @return Returns the frame type (light, dark, ...) of the job.
         */
        CCDFrameType jobFrameType(int id);

        /**
         *  @brief jobImageProgress Returns The number of images completed capture in the job.
         * @param id job number. Job IDs start from 0 to N-1.
         */
        int jobImageProgress(int id);

        /**
         * @param id job number. Job IDs start from 0 to N-1.
         * @return Returns the total number of images to capture in the job.
         */
        int jobImageCount(int id);

        /**
         * @param id job number. Job IDs start from 0 to N-1.
         * @return Returns the number of seconds left in an exposure operation.
         */
        double jobExposureProgress(int id);

        /**
         * @param id job number. Job IDs start from 0 to N-1.
         * @return Returns the total requested exposure duration in the job.
         */
        double jobExposureDuration(int id);

        /**
          * @return Returns the percentage of completed captures in all active jobs
          */
        double progressPercentage();

        /**
         * @return Returns true if there's an active job and it is a preview job.
         */
        bool isActiveJobPreview();

        /**
         * @return Returns time left in seconds until active job is estimated to be complete.
         */
        int activeJobRemainingTime();

        /**
         * @return Returns overall time left in seconds until all jobs are estimated to be complete
         */
        int overallRemainingTime();

        /**
         * Returns the overall sequence queue status. If there are no jobs pending, it returns "Invalid". If all jobs are idle, it returns "Idle". If all jobs are complete, it returns "Complete". If one or more jobs are aborted
         * it returns "Aborted" unless it was temporarily aborted due to guiding deviations, then it would return "Suspended". If one or more jobs have errors, it returns "Error". If any jobs is under progress, returns "Running".
         */
        QString sequenceQueueStatus();

        /**
         * @brief getCalibrationSettings Get Calibration settings
         * @return settings as JSON object
         */
        QJsonObject calibrationSettings();

        /**
         * @brief setCalibrationSettings Set Calibration settings
         * @param settings as JSON object
         */
        void setCalibrationSettings(const QJsonObject &settings);

        /**
         * @brief hasCapturedFramesMap Check if at least one frame has been recorded
         */
        bool hasCapturedFramesMap()
        {
            return m_capturedFramesMap.count() > 0;
        }
        /**
         * @brief addCapturedFrame Record a captured frame
         */
        void addCapturedFrame(const QString &signature);
        /**
         * @brief removeCapturedFrameCount Reduce the frame counts for the given signature
         */
        void removeCapturedFrameCount(const QString &signature, uint16_t count);
        /**
         * @brief clearCapturedFramesMap Clear the map of captured frames counts
         */
        void clearCapturedFramesMap()
        {
            m_capturedFramesMap.clear();
        }

        bool isCaptureRunning()
        {
            return (m_CaptureState == CAPTURE_PROGRESS || m_CaptureState == CAPTURE_CAPTURING || m_CaptureState == CAPTURE_PAUSE_PLANNED ||
                    m_CaptureState == CAPTURE_IMAGE_RECEIVED || m_CaptureState == CAPTURE_CHANGING_FILTER || m_CaptureState == CAPTURE_CALIBRATING);
        }

        bool isCaptureStopped()
        {
            return (m_CaptureState == CAPTURE_IDLE || m_CaptureState == CAPTURE_ABORTED || m_CaptureState == CAPTURE_SUSPENDED);
        }

        bool isCapturePausing()
        {
            return (m_CaptureState == CAPTURE_PAUSE_PLANNED || m_CaptureState == CAPTURE_PAUSED);
        }

        ScriptTypes captureScriptType() const
        {
            return m_CaptureScriptType;
        }
        void setCaptureScriptType(ScriptTypes value)
        {
            m_CaptureScriptType = value;
        }
        double targetADU() const
        {
            return m_targetADU;
        }
        void setTargetADU(double value)
        {
            m_targetADU = value;
        }
        double targetADUTolerance() const
        {
            return m_TargetADUTolerance;
        }
        void setTargetADUTolerance(double value)
        {
            m_TargetADUTolerance = value;
        }
        bool skyFlat() const
        {
            return m_skyFlat;
        }
        void setSkyFlat(bool enabled)
        {
            m_skyFlat = enabled;
        }
        SkyPoint &wallCoord()
        {
            return m_wallCoord;
        }
        void setWallCoord(SkyPoint value)
        {
            m_wallCoord = value;
        }
        const DoubleRange &exposureRange() const
        {
            return m_ExposureRange;
        }
        void setExposureRange(double min, double max)
        {
            m_ExposureRange.min = min;
            m_ExposureRange.max = max;
        }

        QMap<ISD::CameraChip *, QVariantMap> &frameSettings()
        {
            return m_frameSettings;
        }
        void setFrameSettings(const QMap<ISD::CameraChip *, QVariantMap> &value)
        {
            m_frameSettings = value;
        }

        FlatFieldDuration flatFieldDuration() const
        {
            return m_flatFieldDuration;
        }
        void setFlatFieldDuration(FlatFieldDuration value)
        {
            m_flatFieldDuration = value;
        }

        uint32_t calibrationPreAction() const
        {
            return m_CalibrationPreAction;
        }
        void setCalibrationPreAction(uint32_t value)
        {
            m_CalibrationPreAction = value;
        }

        QList<QMap<QString, QVariant> > &DSLRInfos()
        {
            return m_DSLRInfos;
        }

        const QString &opticalTrain() const
        {
            return m_opticalTrain;
        }
        void setOpticalTrain(const QString &newOpticalTrain)
        {
            m_opticalTrain = newOpticalTrain;
        }

        void setPrepareComplete(bool success);

signals:
        // controls for capture execution
        void captureBusy(bool busy);
        void startCapture();
        void abortCapture();
        void suspendCapture();
        void executeActiveJob();
        void updatePrepareState(CaptureState state);
        void captureStarted(CaptureResult rc);
        // request the execution of an action orchestrated by the capture module
        void requestAction(CaptureWorkflowActionType action);
        // mount meridian flip status update event
        void newMeridianFlipStage(MeridianFlipState::MFStage status);
        // meridian flip started
        void meridianFlipStarted();
        // new guiding deviation measured
        void newGuiderDrift(double deviation_rms);
        // guiding should be started after a successful meridian flip
        void guideAfterMeridianFlip();
        // reset non guided dithering properties
        void resetNonGuidedDither();
        // new capture state
        void newStatus(CaptureState status);
        // forward new focus status
        void newFocusStatus(FocusState status);
        // forward new adaptive focus status
        void newAdaptiveFocusStatus(bool success);
        // check focusing is necessary for the given HFR
        void checkFocus(double hfr);
        // run Autofocus
        void runAutoFocus(AutofocusReason autofocusReason, const QString &reasonInfo);
        // reset the focus frame
        void resetFocusFrame();
        // signal focus module to perform adaptive focus
        void adaptiveFocus();
        // abort capturing if fast exposure mode is used
        void abortFastExposure();
        // new HFR focus limit calculated
        void newLimitFocusHFR(double hfr);
        // Select the filter at the given position
        void newFilterPosition(int targetFilterPosition, FilterManager::FilterPolicy policy = FilterManager::ALL_POLICIES);
        // capture sequence status changes
        void sequenceChanged(const QJsonArray &sequence);
        // new log text for the module log window
        void newLog(const QString &text);

    private:
        // initialize the class
        void init();

        // Container for the list of SequenceJobs.
        QSharedPointer<SequenceQueue> m_sequenceQueue;
        // Currently active sequence job.
        QSharedPointer<SequenceJob> m_activeJob;
        // pointer to the image data
        QSharedPointer<FITSData> m_ImageData;
        // CCD Chip frame settings
        QMap<ISD::CameraChip *, QVariantMap> m_frameSettings;
        // DSLR Infos
        QList<QMap<QString, QVariant>> m_DSLRInfos;

        // current optical train
        QString m_opticalTrain;
        // current filter position
        // TODO: check why we have both currentFilterID and this, seems redundant
        int m_CurrentFilterPosition { -1 };
        // current filter name matching the filter position
        QString m_CurrentFilterName { "--" };
        // holds the filter name used for focusing or "--" if the current one is used
        QString m_CurrentFocusFilterName { "--" };
        // Captured Frames Map
        CapturedFramesMap m_capturedFramesMap;
        // are we in the starting phase of capturing?
        bool m_StartingCapture { true };
        // Does the camera have a dedicated guiding chip?
        bool m_useGuideHead { false };
        // Guide Deviation
        bool m_GuidingDeviationDetected { false };
        // suspend guiding when downloading a captured image
        bool m_SuspendGuidingOnDownload { false };
        // Guiding spikes
        uint m_SpikesDetected { 0 };
        // Timer for guiding recovery
        QTimer m_guideDeviationTimer;
        // Timer for settling after guiding start
        QTimer m_guideSettleTimer;
        // Capture timeout timer
        QTimer m_captureTimeout;
        uint8_t m_CaptureTimeoutCounter { 0 };
        uint8_t m_DeviceRestartCounter { 0 };
        // time left of the current exposure
        QTime m_imageCountDown;
        double m_lastRemainingFrameTimeMS;
        // Timer for delay before capturing an image
        // @see CaptureProcess::captureImageWithDelay()
        QTimer m_captureDelayTimer;
        // time left for the current sequence
        QTime m_sequenceCountDown;
        // timer for updating the download progress
        QTimer m_downloadProgressTimer;
        // timer measuring the download time
        QElapsedTimer m_DownloadTimer;
        // sum over all recorded list of download times
        double totalDownloadTime {0};
        // number of downloaded frames
        uint downloadsCounter {0};
        // current placeholder path
        PlaceholderPath m_currentPlaceholderPath;
        // next capture sequence ID
        int m_nextSequenceID { 0 };
        // how to continue after pausing
        CaptureContinueAction m_ContinueAction { CAPTURE_CONTINUE_ACTION_NONE };
        // name of the observer
        QString m_ObserverName;
        // ignore already captured files
        bool m_ignoreJobProgress { true };
        // Fast Exposure
        bool m_RememberFastExposure {false};
        // Set dirty bit to indicate sequence queue file was modified and needs saving.
        bool m_Dirty { false };
        // Capturing (incl. preparation actions) is active
        bool m_Busy { false };
        // preview loop running
        bool m_Looping { false };
        // script type of the currently running script
        ScriptTypes m_CaptureScriptType { SCRIPT_N };
        // Flat field automation
        double m_TargetADUTolerance { 1000 };
        double m_targetADU { 0 };
        bool m_skyFlat { false };
        SkyPoint m_wallCoord;
        FlatFieldDuration m_flatFieldDuration { DURATION_MANUAL };
        uint32_t m_CalibrationPreAction { CAPTURE_PREACTION_NONE };
        bool m_lightBoxLightEnabled { false };
        // Allowed camera exposure times
        DoubleRange m_ExposureRange;
        // Misc
        QJsonArray m_SequenceArray;

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
        ISD::Mount::PierSide m_pierSide { ISD::Mount::PIER_UNKNOWN };
        ISD::ParkStatus m_scopeParkState { ISD::PARK_UNKNOWN };
        ISD::Dome::Status m_domeState { ISD::Dome::DOME_IDLE };

        // ////////////////////////////////////////////////////////////////////
        // counters
        // ////////////////////////////////////////////////////////////////////
        // Number of alignment retries
        int m_AlignmentRetries { 0 };
        // How many images to capture before dithering operation is executed?
        uint m_ditherCounter { 0 };

        /* Refocusing */
        QSharedPointer<RefocusState> m_refocusState;
        /* Meridian Flip */
        QSharedPointer<MeridianFlipState> mf_state;

        /**
         * @brief Add log message
         */
        void appendLogText(const QString &message);
};

}; // namespace
