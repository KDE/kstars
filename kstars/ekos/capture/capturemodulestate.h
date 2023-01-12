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
            ACTION_SCOPE_COVER          /* Ensure that the scope cover (if present) is opened.                                            */
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
            MANUAL_COVER_CLOSED_DARK,

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
        void setGuideState(GuideState value)
        {
            m_GuideState = value;
        }

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
        void setAlignState(AlignState value)
        {
            m_AlignState = value;
        }

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


        double getFileHFR() const
        {
            return m_fileHFR;
        }

        void setFileHFR(double newFileHFR)
        {
            m_fileHFR = newFileHFR;
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
             * @brief Check whether a meridian flip has been requested and trigger it
             * @return true iff a meridian flip has been triggered
             */
        bool checkMeridianFlipReady();

        /**
             * @brief Check if the mount's flip has been completed and start guiding
             * if necessary. Starting guiding after the meridian flip works through
             * the signal {@see startGuidingAfterFlip()}
             * @return true if guiding needs to start but is not running yet
             */
        bool checkGuidingAfterFlip();

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
         * @brief calculate new HFR threshold based on median value for current selected filter
         */
        void updateHFRThreshold();

        /**
             * @brief Check if an alignment needs to be executed after completing
             * a meridian flip.
             * @return
             */
        bool checkAlignmentAfterFlip();

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


        bool isCaptureRunning()
        {
            return (m_CaptureState != CAPTURE_IDLE && m_CaptureState != CAPTURE_COMPLETE && m_CaptureState != CAPTURE_ABORTED);
        }

signals:
        // controls for capture execution
        void startCapture();
        void abortCapture();
        void suspendCapture();
        // guiding should be started after a successful meridian flip
        void guideAfterMeridianFlip();
        // new capture state
        void newStatus(Ekos::CaptureState status);
        // forward new focus status
        void newFocusStatus(FocusState status);
        // check focusing is necessary for the given HFR
        void checkFocus(double hfr);
        // reset the focuser to the last known focus position
        void resetFocus();
        // abort capturing if fast exposure mode is used
        void abortFastExposure();
        // new HFR focus limit calculated
        void newLimitFocusHFR(double hfr);
        // Select the filter at the given position
        void newFilterPosition(int targetFilterPosition);
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
