/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "focusutils.h"
#include "ekos/auxiliary/imageanalysis.h"
#include "ekos/auxiliary/diagnosis.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace Ekos
{

/**
 * Stable machine name of an autofocus failure reason, e.g. "no_stars". The keys go out on D-Bus and EkosLive.
 */
QString focusFailReasonKey(AutofocusFailReason reason);

/** One measurement of an autofocus run. */
struct FocusPoint
{
    int position {0};
    double value {0};               ///< the star measure, e.g. HFR in pixels
    bool outlier {false};           ///< rejected by the curve fit, e.g. a donut
};

/** What the points of a finished autofocus run say about where focus is. */
struct FocusRunCheck
{
    int points {0};                 ///< points used: valid, and not outliers when enough are left
    double lowest {-1};             ///< smallest measure of the run
    int lowestPosition {0};
    int lowPosition {0};            ///< range of the run
    int highPosition {0};
    double reference {-1};          ///< in-focus measure expected for this run, -1 if unknown
    double ratio {0};               ///< lowest / reference, 0 without a reference
    bool r2Failed {false};          ///< the curve fit was poor
    double slopeT {0};              ///< t statistic of the straight-line slope through the points
    /// The measure falls significantly toward one end of the run, and that end has not risen again above the smallest
    /// value
    bool oneSided {false};
    /// The smallest measure is inside the run and both ends are clearly larger: the run went through focus
    bool hasV {false};
    /// The lower of the two ends of the run (each the mean of its two outermost points) over the smallest measure
    double endRise {0};
    /// Where focus lies: -1 below the run, +1 above it, 0 unknown
    int direction {0};
    /// The run never reached focus: its smallest stars are far larger than in focus, and it has no V
    bool noMinimum {false};
};

/** The in-focus HFR of one successful autofocus run, the reference for later runs of the same optical train. */
struct FocusReference
{
    double hfr {0};                 ///< HFR at the solution, in the run's binned pixels
    int binning {1};                ///< binning of the run: a reference serves runs of the same binning only
    double altitude {-1};           ///< altitude of the target in degrees, -1 if unknown
    QDateTime when;
    QString filter;                 ///< filter of the run, empty if none. Kept for references per filter later.
};

/** Everything the Focus rules read. Filled by Focus::collectDiagnosisFacts(). */
struct FocusFacts
{
    int state {0};
    AutofocusFailReason reason {FOCUS_FAIL_NONE};
    /// Detail Focus gave with the reason, e.g. "R2=0.52 < Limit=0.80"
    QString reasonInfo;
    QString train;
    /// Autofocus ran for a capture sequence or the scheduler, which handle the failure themselves
    bool inSequence {false};

    // Devices
    bool hasCamera {false};
    bool cameraConnected {false};
    QString cameraName;
    bool canBin {false};
    bool hasFocuser {false};
    bool focuserConnected {false};
    QString focuserName;
    bool canAbsMove {false};
    bool canRelMove {false};
    bool canTimerMove {false};
    /// Range of an absolute focuser, steps
    double focuserMin {0};
    double focuserMax {0};
    /// Position at the start of the autofocus run, -1 if unknown
    int startPosition {-1};
    int position {0};
    bool hasFilterWheel {false};
    bool filterWheelConnected {false};
    bool hasDustCap {false};

    // Settings
    double exposure {0};        ///< seconds
    double gain {-1};           ///< -1 if the camera has no gain
    int binIndex {0};
    int binCount {1};
    int algorithm {0};          ///< Focus::Algorithm
    int stepSize {0};           ///< initial step size, steps or ms
    double maxTravel {0};       ///< steps
    int maxSingleStep {0};      ///< steps, 0 if not limited
    bool donutBuster {false};

    int outSteps {0};           ///< Linear 1 Pass: steps out from the start position
    int afOverscan {0};         ///< steps
    bool driverBacklashSupported {false};
    int driverBacklash {0};     ///< steps, the backlash compensation of the focuser driver

    // The failed frame
    int stars {-1};             ///< stars Focus found in the last frame, -1 if unknown
    double hfr {-1};            ///< last measured HFR, -1 if none
    std::optional<ImageStats> image;

    /// The check of the run's points, when Focus made one
    std::optional<FocusRunCheck> run;

    /// Where the last successful autofocus with the current filter ended, -1 if unknown
    int lastGoodPosition {-1};
    QString filter;
    double lastGoodTemperature {-1000};  ///< -1000 if unknown
    double lastGoodAltitude {-1};        ///< degrees, -1 if unknown
    /// Now, for the Adaptive Focus model of the filter
    double temperature {-1000};         ///< -1000 if unknown
    double altitude {-1};               ///< degrees, -1 if unknown
    double ticksPerTemp {0};            ///< focuser steps per degree C, 0 if not set
    double ticksPerAlt {0};             ///< focuser steps per degree of altitude, 0 if not set
};

/**
 * @class FocusDiagnosis
 * @short Turns FocusFacts into a Diagnosis::Report. Pure: no devices, no widgets, no options.
 */
class FocusDiagnosis
{
    public:
        /// Same values as Focus::Algorithm
        static constexpr int ALGORITHM_LINEAR1PASS = 3;
        /// The exposure items stop raising the exposure here, seconds.
        static constexpr double MAX_SUGGESTED_EXPOSURE = 10;
        /// The step the pulse item sets for a focuser that moves by time, ms, as Focus suggests when it fails.
        static constexpr int SUGGESTED_PULSE_STEP = FOCUS_MINIMUM_PULSE_TIMER * 5;

        /** True if a failure with this reason gets a diagnosis: not an abort, a rerun or a skipped run. */
        static bool isDiagnosable(AutofocusFailReason reason);

        /** True for failures that the frame can explain, so it is worth measuring. */
        static bool isFrameReason(AutofocusFailReason reason);

        /**
         * @brief reportedReason The reason to report for an autofocus run that ended with failCode. After its retries
         * a run ends with FOCUS_FAIL_FORCE_ABORT, which says nothing about the cause: the reason of the last failed
         * attempt is reported instead.
         * @param failCode the reason the run ended with
         * @param attemptReason the reason the last failed attempt of the run ended with, FOCUS_FAIL_NONE if none
         */
        static AutofocusFailReason reportedReason(AutofocusFailReason failCode, AutofocusFailReason attemptReason);

        /** The exposure the exposure item sets: twice the current one, at least 1 s, at most MAX_SUGGESTED_EXPOSURE. */
        static double suggestedExposure(double exposure);

        /** The step size the step item sets: twice the current one, within the largest single step. 0 if it can't grow. */
        static int suggestedStepSize(const FocusFacts &facts);

        /** The maximum travel the travel item sets: twice the current one, within the focuser range. 0 if it can't grow. */
        static double suggestedMaxTravel(const FocusFacts &facts);

        /// A run with a poor fit whose smallest stars are this many times the reference never got near focus
        static constexpr double NO_MINIMUM_RATIO = 2.0;
        /// The same for a run with a good fit that only falls toward one end: focus near one end of a run on a night
        /// of bad seeing looks alike, so the margin is larger
        static constexpr double ONE_SIDED_RATIO = 2.5;
        /// A slope this many standard errors from zero is a real trend, not noise
        static constexpr double SIGNIFICANT_SLOPE_T = 3.0;
        /// Both ends of a run through focus are at least this many times its smallest measure
        static constexpr double V_RISE = 1.3;
        /// Both ends of a run kept as a reference are at least this many times its smallest measure. Lower than V_RISE:
        /// a good run with a modest step only rises this much.
        static constexpr double REFERENCE_RISE = 1.15;
        /// Fewer points than this say nothing about the shape of a run
        static constexpr int MIN_RUN_POINTS = 5;
        /// The reference is the median of this many recent runs
        static constexpr int REFERENCE_RUNS = 5;
        /// Older runs are no reference: the optics may have changed
        static constexpr int REFERENCE_MAX_AGE_DAYS = 30;
        /// Runs within this many degrees of altitude serve as they were measured, without a model of the seeing
        static constexpr double NEAR_ALTITUDE = 15;

        /**
         * @brief checkRun Tell whether a finished run reached focus. Cheap: a straight line through a dozen points.
         * It is only a failure when two independent things agree: the smallest stars are far larger than the
         * reference (NO_MINIMUM_RATIO with a poor or failed fit, ONE_SIDED_RATIO with a good fit and a slope toward
         * one end), and the run has no V. A run whose ends both rise clearly above its smallest stars is never one.
         * @param points the measurements of the run, without the final one at the solution
         * @param reference in-focus measure expected for this run (see referenceFor()), -1 if unknown
         * @param r2Failed the curve fit did not pass its R2 limit
         */
        static FocusRunCheck checkRun(const QList<FocusPoint> &points, double reference, bool r2Failed);

        /**
         * @brief isReferenceRun Whether the in-focus HFR of a successful run may become a reference: a good fit, no
         * slope toward one end, the smallest stars inside the run and both ends above them. A run that stopped away
         * from focus must never be one, or it would hide the next one.
         */
        static bool isReferenceRun(const FocusRunCheck &check);

        /** Airmass for an altitude in degrees, from 10 degrees up. */
        static double airmass(double altitude);

        /**
         * @brief referenceFor The in-focus HFR to expect for a run: the median of the recent references with the
         * run's binning. References within NEAR_ALTITUDE of the run's altitude count as measured. Without any, all
         * count, each scaled up for a lower altitude (airmass to the power 0.6, the most seeing can do) but never down:
         * a setup whose star size is set by its optics or pixels has stars as large high in the sky as low, and on the
         * sky a refractor showed just that. Binned stars cover a few pixels only, so an HFR does not convert from
         * one binning to another: on the sky an HFR of 0.95 at 2x2 was 1.35 at 1x1, not 1.9.
         * @param history recent successful runs of this optical train
         * @param altitude altitude of this run in degrees, -1 if unknown (no scaling then)
         * @param binning binning of this run: only references with it count
         * @param now references older than REFERENCE_MAX_AGE_DAYS before this are ignored
         * @return the reference in binned pixels, -1 if there is none
         */
        static double referenceFor(const QList<FocusReference> &history, double altitude, int binning, const QDateTime &now);

        /**
         * @brief lastGoodTarget Where focus should be now for the filter: the last good position, moved by the
         * filter's Adaptive Focus coefficients for the change of temperature and altitude since, as Adaptive Focus
         * does. A coefficient that is not set, or a reading that is unknown, moves nothing. Within the focuser range.
         * @return the position, -1 if no last good position is known
         */
        static int lastGoodTarget(const FocusFacts &facts);

        /** The history with a new run added, keeping the REFERENCE_RUNS most recent ones. */
        static QList<FocusReference> addReference(QList<FocusReference> history, const FocusReference &reference);

        /** Max Travel a Linear 1 Pass run needs: out steps times the step size, plus the overscan. */
        static int neededMaxTravel(const FocusFacts &facts);

        /**
         * @brief diagnose Build the report of an autofocus failure.
         * @param facts what Focus knew at the failure
         * @param tried item ids already applied in this failure chain; they are not offered again
         * @param dismissed item ids the user dismissed; they are not offered
         * @return the report, with id and created left empty for the caller to fill
         */
        static Diagnosis::Report diagnose(const FocusFacts &facts, const QStringList &tried = QStringList(),
                                          const QStringList &dismissed = QStringList());

        /**
         * @brief checkup Check the Focus setup when nothing failed: the camera, the focuser, the filter wheel and
         * the step size of a focuser that moves by time. Its items never retry Focus by themselves.
         * @param facts what Focus knows now; the reason is ignored
         * @param dismissed item ids the user dismissed; they are not offered
         * @return the report, with the reason key "checkup" and no items when everything looks right
         */
        static Diagnosis::Report checkup(const FocusFacts &facts, const QStringList &dismissed = QStringList());
};

}
