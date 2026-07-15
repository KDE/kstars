/*
    SPDX-FileCopyrightText: 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars:
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "vect.h"
#include "indi/indicommon.h"

#include <QObject>
#include <QPointer>
#include <QTime>
#include <QVector>
#include <QFile>
#include <QElapsedTimer>

#include <cstdint>
#include <sys/types.h>
#include "guidestars.h"
#include "calibration.h"

#include "gpg.h"
#include "mount_guider.h"

class FITSData;
class Edge;
class GuideLog;
class LinearGuider;
class HysteresisGuider;

// For now also copied in guidealgorithms.cpp
#define SMART_THRESHOLD    0
#define SEP_THRESHOLD      1
#define CENTROID_THRESHOLD 2
#define AUTO_THRESHOLD     3
#define NO_THRESHOLD       4
#define SEP_MULTISTAR      5

#define GUIDE_RA    0
#define GUIDE_DEC   1
#define CHANNEL_CNT 2

/**
 * @brief Lifecycle state of the AI feed-forward prediction system.
 *
 * DISABLED   – no weights loaded, or AI not configured.
 * WARMUP     – weights loaded, model running, but prediction not yet valid.
 * SHADOW     – weights loaded, model running; predictions are logged to CSV
 *              but NOT blended into guide pulses (observer mode).
 * ACTIVE     – weights loaded, model running; predictions ARE blended.
 * FALLBACK   – model was ACTIVE but confidence dropped; reverted to standard.
 */
enum class AIGuideState
{
    DISABLED,
    WARMUP,
    SHADOW,
    ACTIVE,
    FALLBACK
};

/// Helper to stringify AIGuideState for the debug CSV
inline QString aiGuideStateString(AIGuideState s)
{
    switch (s)
    {
        case AIGuideState::DISABLED:
            return QStringLiteral("DISABLED");
        case AIGuideState::WARMUP:
            return QStringLiteral("WARMUP");
        case AIGuideState::SHADOW:
            return QStringLiteral("SHADOW");
        case AIGuideState::ACTIVE:
            return QStringLiteral("ACTIVE");
        case AIGuideState::FALLBACK:
            return QStringLiteral("FALLBACK");
    }
    return QStringLiteral("UNKNOWN");
}

// input params
class cproc_in_params
{
    public:
        cproc_in_params();
        void reset(void);

        bool enabled[CHANNEL_CNT];
        bool enabled_axis1[CHANNEL_CNT];
        bool enabled_axis2[CHANNEL_CNT];
        bool average;
        double proportional_gain[CHANNEL_CNT];
        double integral_gain[CHANNEL_CNT];
        int max_pulse_arcsec[CHANNEL_CNT];
        double min_pulse_arcsec[CHANNEL_CNT];
};

//output params
class cproc_out_params
{
    public:
        cproc_out_params();
        void reset(void);

        double delta[2];
        GuideDirection pulse_dir[2];
        int pulse_length[2];
        double sigma[2];
};

typedef struct
{
    double focal_ratio;
    double fov_wd, fov_ht;
    double focal, aperture;
} info_params_t;

class cgmath : public QObject
{
        Q_OBJECT

    public:
        cgmath();
        virtual ~cgmath();

        // functions
        bool setVideoParameters(int vid_wd, int vid_ht, int binX, int binY);
        bool setGuiderParameters(double guider_aperture);

        bool setTargetPosition(double x, double y);
        bool getTargetPosition(double *x, double *y) const;

        void setStarDetectionAlgorithmIndex(int algorithmIndex);
        bool usingSEPMultiStar() const;

        GPG &getGPG()
        {
            return *gpg;
        }
        MountSpecificGuider *getAIGuider()
        {
            return m_AIGuider.get();
        }
        const MountSpecificGuider *getAIGuider() const
        {
            return m_AIGuider.get();
        }
        // True after start() when the AI algorithm was selected but the AI guider could not be
        // loaded (no/invalid weights); the internal guider uses this to abort instead of guiding.
        bool aiRequiredButUnavailable() const
        {
            return m_aiRequiredButUnavailable;
        }
        const cproc_out_params *getOutputParameters() const
        {
            return &out_params;
        }

        // Operations
        void start();
        bool reset();
        // Currently only relevant to SEP MultiStar.
        void abort();
        void suspend(bool mode);
        bool isSuspended() const;

        // Star tracking
        void getStarScreenPosition(double *dx, double *dy) const;
        GuiderUtils::Vector findLocalStarPosition(QSharedPointer < FITSData > &imageData,
                QSharedPointer < GuideView > &guideView, bool firstFrame);
        bool isStarLost() const;
        void setLostStar(bool is_lost);

        // Main processing function
        void performProcessing(Ekos::GuideState state,
                               QSharedPointer < FITSData > &imageData,
                               QSharedPointer < GuideView > &guideView,
                               const std::pair < Seconds, Seconds > &timeStep,
                               GuideLog *logger = nullptr);

        void performDarkGuiding(Ekos::GuideState state, const std::pair < Seconds, Seconds > &timeStep);

        bool calibrate1D(double start_x, double start_y, double end_x, double end_y, int RATotalPulse);
        bool calibrate2D(double start_ra_x, double start_ra_y, double end_ra_x, double end_ra_y,
                         double start_dec_x, double start_dec_y, double end_dec_x, double end_dec_y,
                         bool *swap_dec, int RATotalPulse, int DETotalPulse);

        const Calibration &getCalibration()
        {
            return calibration;
        }
        Calibration *getMutableCalibration()
        {
            return &calibration;
        }
        QVector3D selectGuideStar(const QSharedPointer < FITSData > &imageData);
        double getGuideStarSNR();

        const GuideStars &getGuideStars()
        {
            return guideStars;
        }

    Q_SIGNALS:
        void newAxisDelta(double delta_ra, double delta_dec);
        void newStarPosition(QVector3D, bool);

        // For Analyze.
        void guideStats(double raError, double decError, int raPulse, int decPulse,
                        double snr, double skyBg, int numStars);

        // Push visible UI messages to Guide log
        void newLog(const QString &text);

        // Emitted whenever the AI feed-forward lifecycle changes. state is AIGuideState cast to int.
        void newAIState(int state, double confidence);

    private:
        // Templated functions
        template < typename T >
        GuiderUtils::Vector findLocalStarPosition(void) const;

        void updateCircularBuffers(void);
        GuiderUtils::Vector point2arcsec(const GuiderUtils::Vector &p) const;
        void calculatePulses(Ekos::GuideState state, const std::pair < Seconds, Seconds > &timeStep);
        void calculateRmsError(void);

        // Old-stye Logging--deprecate.
        void createGuideLog();

        // For Analyze.
        void emitStats();

        uint32_t iterationCounter { 0 };

        /// Video frame width
        int video_width { -1 };
        /// Video frame height
        int video_height { -1 };
        double aperture { 0 };
        bool suspended { false };
        bool lost_star { false };

        /// Index of algorithm used
        int m_StarDetectionAlgorithm { SMART_THRESHOLD };

        // The latest guide star position (x,y on the image),
        // and target position we're trying to keep it aligned to.
        GuiderUtils::Vector starPosition;
        GuiderUtils::Vector targetPosition;

        // processing
        // Drift is a circular buffer of the arcsecond drift for the 2 channels.
        // It will be initialized with a double array of size 100 (default PHD2 value).
        // driftUpto points to the index of the next value to be written.
        static constexpr int CIRCULAR_BUFFER_SIZE = 100;
        double *drift[2];
        uint32_t driftUpto[2];

        double drift_integral[2];

        // overlays...
        cproc_in_params in_params;
        cproc_out_params out_params;

        // stat math...
        bool do_statistics { true };

        QFile logFile;
        QElapsedTimer logTime;

        GuideStars guideStars;

        std::unique_ptr < GPG > gpg;
        std::unique_ptr < LinearGuider > m_RALinearGuider;
        std::unique_ptr < LinearGuider > m_DECLinearGuider;
        std::unique_ptr < HysteresisGuider > m_RAHysteresisGuider;
        std::unique_ptr < HysteresisGuider > m_DECHysteresisGuider;

        std::unique_ptr < MountSpecificGuider > m_AIGuider;
        GuideOutput m_lastAIPrediction;
        double m_sessionStartTime { 0.0 };

        // Accumulate pulses sent between camera frames
        double m_accumulated_pulse_ra { 0.0 };
        double m_accumulated_pulse_dec { 0.0 };

        bool m_AILoggedActive { false };
        bool m_AILoggedFullConfidence { false };
        bool m_AILoggedWarmup { false };

        /// Current AI lifecycle state (DISABLED/WARMUP/SHADOW/ACTIVE/FALLBACK)
        AIGuideState m_aiState { AIGuideState::DISABLED };

        /// Set in start(): AI algorithm selected but the AI guider could not be loaded.
        bool m_aiRequiredButUnavailable { false };

        // AI debug CSV logger (per-session file)
        QFile *m_AIDebugFile { nullptr };
        bool m_AIDebugHeaderWritten { false };

        Calibration calibration;
        bool configureInParams(Ekos::GuideState state);
        void updateOutParams(int k, const double arcsecDrift, int pulseLength, GuideDirection pulseDirection,
                             bool accumulate = true);
        // Assigns m_aiState and emits newAIState() so the UI can mirror the AI lifecycle.
        void setAIState(AIGuideState s);
        void outputGuideLog();
        void processAxis(const int k, const bool dithering, const bool darkGuiding, const Seconds &timeStep, const QString &label);
};
