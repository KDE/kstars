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
        GuiderUtils::Vector findLocalStarPosition(QSharedPointer<FITSData> &imageData,
                QSharedPointer<GuideView> &guideView, bool firstFrame);
        bool isStarLost() const;
        void setLostStar(bool is_lost);

        // Main processing function
        void performProcessing(Ekos::GuideState state,
                               QSharedPointer<FITSData> &imageData,
                               QSharedPointer<GuideView> &guideView,
                               const std::pair<Seconds, Seconds> &timeStep,
                               GuideLog *logger = nullptr);

        void performDarkGuiding(Ekos::GuideState state, const std::pair<Seconds, Seconds> &timeStep);

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
        QVector3D selectGuideStar(const QSharedPointer<FITSData> &imageData);
        double getGuideStarSNR();

        const GuideStars &getGuideStars()
        {
            return guideStars;
        }

    signals:
        void newAxisDelta(double delta_ra, double delta_dec);
        void newStarPosition(QVector3D, bool);

        // For Analyze.
        void guideStats(double raError, double decError, int raPulse, int decPulse,
                        double snr, double skyBg, int numStars);

    private:
        // Templated functions
        template <typename T>
        GuiderUtils::Vector findLocalStarPosition(void) const;

        void updateCircularBuffers(void);
        GuiderUtils::Vector point2arcsec(const GuiderUtils::Vector &p) const;
        void calculatePulses(Ekos::GuideState state, const std::pair<Seconds, Seconds> &timeStep);
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

        std::unique_ptr<GPG> gpg;
        std::unique_ptr<LinearGuider> m_RALinearGuider;
        std::unique_ptr<LinearGuider> m_DECLinearGuider;
        std::unique_ptr<HysteresisGuider> m_RAHysteresisGuider;
        std::unique_ptr<HysteresisGuider> m_DECHysteresisGuider;

        Calibration calibration;
        bool configureInParams(Ekos::GuideState state);
        void updateOutParams(int k, const double arcsecDrift, int pulseLength, GuideDirection pulseDirection);
        void outputGuideLog();
        void processAxis(const int k, const bool dithering, const bool darkGuiding, const Seconds &timeStep, const QString &label);
};
