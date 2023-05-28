/*  Ekos state machine for the Capture module
    SPDX-FileCopyrightText: 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include "Options.h"

#include "ekos/ekos.h"
#include "indiapi.h"
#include "indi/indistd.h"
#include "indi/indidustcap.h"
#include "indi/indimount.h"
#include "indi/indidome.h"

#include "ekos/manager/meridianflipstate.h"
#include "ekos/capture/refocusstate.h"
#include "ekos/auxiliary/filtermanager.h"
#include "ekos/scheduler/schedulerjob.h"

namespace Ekos
{

class SequenceJob;

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
            ACTION_SCOPE_COVER,         /* Ensure that the scope cover (if present) is opened.                                            */
            ACTION_AUTOFOCUS            /* Execute autofocus (might be triggered due to filter change).                                   */
        } PrepareActions;

        typedef enum
        {
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

        typedef enum
        {
            CAP_LIGHT_OFF,     /* light is on               */
            CAP_LIGHT_ON,      /* light is off              */
            CAP_LIGHT_UNKNOWN, /* unknown whether on or off */
            CAP_LIGHT_BUSY     /* light state changing      */
        } LightState;

        typedef enum
        {
            CONTINUE_ACTION_NONE,            /* do nothing              */
            CONTINUE_ACTION_NEXT_EXPOSURE,   /* start next exposure     */
            CONTINUE_ACTION_CAPTURE_COMPLETE /* recall capture complete */
        } ContinueAction;

        CaptureModuleState(QObject *parent = nullptr);

        // ////////////////////////////////////////////////////////////////////
        // sequence jobs
        // ////////////////////////////////////////////////////////////////////
        QList<SequenceJob *> &allJobs()
        {
            return m_allJobs;
        }

        SequenceJob *getActiveJob() const
        {
            return m_activeJob;
        }
        void setActiveJob(SequenceJob *value)
        {
            m_activeJob = value;
        };

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

        ContinueAction getContinueAction() const
        {
            return m_ContinueAction;
        }
        void setContinueAction(ContinueAction newPauseFunction)
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

        QTimer &getCaptureDelayTimer()
        {
            return m_captureDelayTimer;
        }

        QTimer &getGuideDeviationTimer()
        {
            return m_guideDeviationTimer;
        }

        bool isGuidingDeviationDetected() const
        {
            return m_GuidingDeviationDetected;
        }
        void setGuidingDeviationDetected(bool newDeviationDetected)
        {
            m_GuidingDeviationDetected = newDeviationDetected;
        }

        int SpikesDetected() const
        {
            return m_SpikesDetected;
        }
        int increaseSpikesDetected()
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


        const QString &targetName() const
        {
            return m_TargetName;
        }
        void setTargetName(const QString &newTargetName)
        {
            m_TargetName = newTargetName;
        }

        double getFileHFR() const
        {
            return m_fileHFR;
        }
        void setFileHFR(double newFileHFR)
        {
            m_fileHFR = newFileHFR;
        }

        bool ignoreJobProgress() const
        {
            return m_ignoreJobProgress;
        }
        void setIgnoreJobProgress(bool value)
        {
            m_ignoreJobProgress = value;
        }

        bool dirty() const
        {
            return m_Dirty;
        }
        void setDirty(bool value)
        {
            m_Dirty = value;
        }

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
        void resetDitherCounter()
        {
            m_ditherCounter = Options::ditherFrames();
        }

        /**
         * @brief checkSeqBoundary Determine the next file number sequence.
         *        That is, if we have file1.png and file2.png, then the next
         *        sequence should be file3.png.
         */
        void checkSeqBoundary();

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
        void setCapturedFramesCount(const QString &signature, uint16_t count)
        {
            m_capturedFramesMap[signature] = count;
        }

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
         * @brief Start focusing if necessary
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
         * @brief Slot that listens to guiding deviations reported by the Guide module.
         *
         * Depending on the current status, it triggers several actions:
         * - If there is no active job, it calls {@see m_captureModuleState->checkMeridianFlipReady()}, which may initiate a meridian flip.
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
        bool setDarkFlatExposure(SequenceJob *job);

        /**
         * @brief checkSeqBoundary Determine the next file number sequence.
         *        That is, if we have file1.png and file2.png, then the next
         *        sequence should be file3.png.
         */
        void checkSeqBoundary(QUrl sequenceURL);

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
            return (m_CaptureState != CAPTURE_IDLE && m_CaptureState != CAPTURE_COMPLETE && m_CaptureState != CAPTURE_ABORTED);
        }

    signals:
        // controls for capture execution
        void startCapture();
        void abortCapture();
        void suspendCapture();
        // mount meridian flip status update event
        void newMeridianFlipStage(MeridianFlipState::MFStage status);
        // meridian flip started
        void meridianFlipStarted();
        // guiding should be started after a successful meridian flip
        void guideAfterMeridianFlip();
        // new capture state
        void newStatus(Ekos::CaptureState status);
        // forward new focus status
        void newFocusStatus(FocusState status);
        // forward new adaptive focus status
        void newAdaptiveFocusStatus(bool success);
        // check focusing is necessary for the given HFR
        void checkFocus(double hfr);
        // reset the focuser to the last known focus position
        void resetFocus();
        // signal focus module to perform adaptive focus
        void adaptiveFocus();
        // abort capturing if fast exposure mode is used
        void abortFastExposure();
        // new HFR focus limit calculated
        void newLimitFocusHFR(double hfr);
        // Select the filter at the given position
        void newFilterPosition(int targetFilterPosition, FilterManager::FilterPolicy policy = FilterManager::ALL_POLICIES);
        // new log text for the module log window
        void newLog(const QString &text);

    private:
        // list of all sequence jobs
        QList<SequenceJob *> m_allJobs;
        // Currently active sequence job.
        SequenceJob *m_activeJob { nullptr };

        // current filter position
        // TODO: check why we have both currentFilterID and this, seems redundant
        int m_CurrentFilterPosition { -1 };
        // current filter name matching the filter position
        QString m_CurrentFilterName { "--" };
        // holds the filter name used for focusing or "--" if the current one is used
        QString m_CurrentFocusFilterName { "--" };
        // HFR value as loaded from the sequence file
        double m_fileHFR { 0 };
        // Captured Frames Map
        SchedulerJob::CapturedFramesMap m_capturedFramesMap;
        // are we in the starting phase of capturing?
        bool m_StartingCapture { true };
        // Guide Deviation
        bool m_GuidingDeviationDetected { false };
        // Guiding spikes
        int m_SpikesDetected { 0 };
        // Timer for guiding recovery
        QTimer m_guideDeviationTimer;
        // Timer to start the entire capturing with the delay configured
        // for the first capture job that is ready to be executed.
        // @see Capture::start().
        QTimer m_captureDelayTimer;
        // sum over all recorded list of download times
        double totalDownloadTime;
        // number of downloaded frames
        uint downloadsCounter {0};
        // next capture sequence ID
        int m_nextSequenceID { 0 };
        // how to continue after pausing
        ContinueAction m_ContinueAction { CONTINUE_ACTION_NONE };
        // name of the capture target
        QString m_TargetName;
        // ignore already captured files
        bool m_ignoreJobProgress { true };
        // Set dirty bit to indicate sequence queue file was modified and needs saving.
        bool m_Dirty { false };


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
