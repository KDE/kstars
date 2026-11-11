/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ekos/auxiliary/imageanalysis.h"
#include "ekos/auxiliary/diagnosis.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace Ekos
{

/**
 * @brief Why Align failed or was aborted. The values are stable: they go out on D-Bus and EkosLive.
 * Use alignFailReasonKey() for a stable machine name.
 */
enum class AlignFailReason
{
    None = 0,
    NoMountCoords,
    NoCamera,
    CameraDisconnected,
    MissingOptics,
    MissingPixelSize,
    FovOutOfBounds,
    FilterWheelDisconnected,
    FilterChangeFailed,
    BlobDisabled,
    DustCapError,
    CaptureError,
    CaptureTimeout,
    BusyTimeout,
    NoIndexFiles,
    SolverFailed,
    SolverTimeout,
    PahSolverFailed,
    RemoteSolverTimeout,
    SyncRejected,
    SyncAlert,
    SlewRejected,
    SlewAlert,
    MountParked,
    RotatorAlert,
    RotatorNotReached,
    RotatorTimeout,
    RotatorWrongDirection,
    AccuracyNotMet,
    UserAbort,
    // The mount ended Align's slew stopped instead of tracking, e.g. at a limit
    SlewEndedIdle
};

/** Stable machine name of a reason, e.g. "solver_failed". */
QString alignFailReasonKey(AlignFailReason reason);

/** One solver run, kept for the diagnosis. */
struct AlignAttempt
{
    bool timedOut {false};
    bool success {false};
    double elapsed {0};
    int starsFound {-1};
    bool usedScale {false};
    double scaleLow {0};
    double scaleHigh {0};
    bool usedPosition {false};
    double ra {0};
    double dec {0};
    /// "", "scale", "position", "scale_position" or "dynamic_threshold"
    QString blindPass;
    double dynamicThreshold {0};
    QString profile;
    int solverType {0};
    int binning {1};
    double exposure {0};
    double gain {-1};
};

/** Everything the Align rules read. Filled by Align::collectDiagnosisFacts(). */
struct AlignFacts
{
    int state {0};
    AlignFailReason reason {AlignFailReason::None};
    QString train;

    // Devices
    bool hasCamera {false};
    bool cameraConnected {false};
    QString cameraName;
    bool hasMount {false};
    bool mountConnected {false};
    bool mountCoordsValid {false};
    bool mountParked {false};
    bool canSync {false};
    int gotoMode {0};           ///< Align::GotoMode
    bool hasFilterWheel {false};
    bool filterWheelConnected {false};
    bool hasRotator {false};

    // Counters
    int captureErrors {0};
    int captureTimeouts {0};
    int driverRestarts {0};
    int slewErrors {0};

    // Optics and camera
    double focalLength {-1};
    double aperture {-1};
    double reducer {1};
    double pixelW {-1};
    double pixelH {-1};
    int width {0};
    int height {0};
    double fovW {0};            ///< arcmin
    double fovH {0};            ///< arcmin
    double exposure {0};
    double gain {-1};
    int binIndex {0};
    int binCount {1};
    bool canBin {false};

    // Solver
    bool localSolver {true};    ///< false for the remote INDI solver
    int solverType {0};         ///< SSolver::SolverType
    bool externalSolverMissing {false};
    QString profile;
    bool useScale {false};
    bool usePosition {false};
    double timeoutSec {0};
    QList<AlignAttempt> attempts;
    std::optional<ImageStats> image;

    // Index files
    bool indexSolver {false};   ///< the solver reads astrometry.net index files
    QList<int> requiredSizes;
    QList<int> installedSizes;

    // Mount limits and clock
    bool altLimitsEnabled {false};
    double altMin {0};
    double altMax {90};
    std::optional<double> targetAlt;
    /// Declination of the target or the mount, degrees, for the sidereal drift of trailed stars
    std::optional<double> declination;
    /// The time the mount last reported in TIME_UTC, and when it arrived by the clock of this computer.
    /// Most drivers report their time once, e.g. when KStars sends it, so the two are compared, not the
    /// reported time with the current time.
    std::optional<QDateTime> mountUtc;
    QDateTime mountUtcReceived;
    /// The time of the client device (EkosLive), if it sent one
    std::optional<QDateTime> clientUtc;
    /// The clock of this computer, the reference when there is no client device
    QDateTime systemUtc;
    QDateTime kstarsUtc;
    /// KStars syncs the time with INDI devices (Options::useTimeUpdate())
    bool timeUpdate {true};
    /// The master source of the time: "KStars", or a device that sets the KStars clock (Options::timeSource())
    QString timeSource {QStringLiteral("KStars")};

    // Rotator
    double rotatorThresholdArcmin {0};
};

/**
 * @class AlignDiagnosis
 * @short Turns AlignFacts into a Diagnosis::Report. Pure: no devices, no widgets, no options.
 */
class AlignDiagnosis
{
    public:
        /// Fewer stars than this on a failed frame is too few to solve.
        static constexpr int MIN_STARS = ImageAnalysis::MIN_STARS;
        /// Trails between these fractions of the sidereal drift mean the mount does not track at all.
        static constexpr double MIN_UNTRACKED_RATIO = 0.6;
        static constexpr double MAX_UNTRACKED_RATIO = 1.5;
        /// Clock difference in seconds above which a clock is wrong.
        static constexpr double MAX_CLOCK_DIFF_SEC = 120;
        /// Exposure suggested when too few stars were found.
        static constexpr double SUGGESTED_EXPOSURE = 5;
        /// Upper limit of the solver timeout item.
        static constexpr double MAX_SOLVER_TIMEOUT = 600;
        /// Narrow fields below this size in arcmin are hard to solve.
        static constexpr double NARROW_FOV_ARCMIN = 15;
        /// Name of the default solver profile.
        static QString defaultProfileName();

        /**
         * @brief diagnose Build the report of an Align failure.
         * @param facts what Align knew at the failure
         * @param tried item ids already applied in this failure chain; they are not offered again
         * @param dismissed item ids the user dismissed; they are not offered
         * @return the report, with id and created left empty for the caller to fill
         */
        static Diagnosis::Report diagnose(const AlignFacts &facts, const QStringList &tried = QStringList(),
                                          const QStringList &dismissed = QStringList());

        /**
         * @brief checkup Check the Align setup when nothing failed: devices, optics, index files, clocks and the
         * settings that make solving hard. Every item is a suggestion: it never retries Align by itself.
         * @param facts what Align knows now; the reason is ignored
         * @param dismissed item ids the user dismissed; they are not offered
         * @return the report, with the reason key "checkup" and no items when everything looks right
         */
        static Diagnosis::Report checkup(const AlignFacts &facts, const QStringList &dismissed = QStringList());

        /** True if a failure with this reason gets a diagnosis (not a user abort). */
        static bool isDiagnosable(AlignFailReason reason);
};

}
