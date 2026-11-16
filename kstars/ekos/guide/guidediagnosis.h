/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "guidefailreason.h"
#include "ekos/auxiliary/imageanalysis.h"
#include "ekos/auxiliary/diagnosis.h"

#include <QString>

#include <optional>

namespace Ekos
{

/** Everything the Guide rules read. Filled by Guide::collectDiagnosisFacts(). */
struct GuideFacts
{
    int state {0};                  ///< Ekos::GuideState at the failure
    GuideFailReason reason {GuideFailReason::None};
    /// Detail the guider gave, e.g. the reason PHD2 reported
    QString detail;
    QString train;
    int guiderType {0};             ///< Guide::GuiderType: 0 internal, 1 PHD2, 2 LinGuider

    // Calibration at the failure
    int failPulse {0};              ///< ms, 0 if unknown
    double failDrift {-1};          ///< pixels the star moved, -1 if unknown

    // Devices
    bool hasCamera {false};
    bool cameraConnected {false};
    QString cameraName;
    bool canBin {false};
    bool hasGuider {false};         ///< the optical train has a device to send the pulses to (Guide Via)
    bool guiderConnected {false};
    QString guiderName;
    /// The pulses go to the camera, which passes them on to the mount through an ST4 cable
    bool guideViaCamera {false};
    bool hasMount {false};
    bool mountConnected {false};

    // Optics
    double focalLength {0};         ///< mm
    double aperture {0};            ///< mm
    double pixelW {0};              ///< um
    double pixelH {0};              ///< um

    // Settings
    double exposure {0};            ///< s
    int binIndex {0};
    int binCount {1};
    int squareSize {0};             ///< px, 0 if unknown
    int largerSquareSize {0};       ///< the next square size offered, 0 if none
    int calibrationPulse {0};       ///< ms, Options::calibrationPulseDuration()
    bool decBacklashRemoval {false}; ///< Options::guideCalibrationBacklash()
    double lostStarTimeout {0};     ///< s
    int ditherMaxIterations {0};
    bool ditherFailAborts {true};

    // The last calibration of the internal guider, guide rates in arcsec per second, 0 if not calibrated
    double raRate {0};
    double decRate {0};

    std::optional<ImageStats> image;
};

/**
 * @class GuideDiagnosis
 * @short Turns GuideFacts into a Diagnosis::Report. Pure: no devices, no widgets, no options.
 *
 * Calibration failures point at the mount first: balance, clutches, cables and backlash stop the star from moving
 * far more often than a setting does.
 */
class GuideDiagnosis
{
    public:
        /// Same values as Guide::GuiderType
        static constexpr int GUIDER_INTERNAL = 0;
        static constexpr int GUIDER_PHD2 = 1;
        /// Calibration pulses are not raised beyond this, ms
        static constexpr int MAX_CALIBRATION_PULSE = 3000;
        /// Nor lowered below this, ms
        static constexpr int MIN_CALIBRATION_PULSE = 100;
        /// Guide exposures are not raised beyond this, s
        static constexpr double MAX_GUIDE_EXPOSURE = 4;
        /// The lost star timeout is not raised beyond this, s
        static constexpr double MAX_LOST_STAR_TIMEOUT = 120;
        /// Dither iterations are not raised beyond this
        static constexpr int MAX_DITHER_ITERATIONS = 30;
        /// The sidereal rate, arcsec per second
        static constexpr double SIDEREAL_RATE = 15.04;
        /// A guide rate below this fraction of sidereal barely moves the star
        static constexpr double MIN_GUIDE_RATE = 0.25;

        /** True if a failure with this reason gets a diagnosis. */
        static bool isDiagnosable(GuideFailReason reason);

        /** True for failures that the guide frame can explain, so it is worth measuring. */
        static bool isFrameReason(GuideFailReason reason);

        /** The calibration pulse the pulse item sets: twice the current one, at most MAX_CALIBRATION_PULSE. 0 if it can't grow. */
        static int raisedPulse(int pulse);
        /** Half the current one, at least MIN_CALIBRATION_PULSE. 0 if it can't shrink. */
        static int loweredPulse(int pulse);
        /** Twice the current exposure, at least 1 s, at most MAX_GUIDE_EXPOSURE. 0 if it can't grow. */
        static double raisedExposure(double exposure);
        /** Twice the current timeout, at least 30 s, at most MAX_LOST_STAR_TIMEOUT. 0 if it can't grow. */
        static double raisedLostStarTimeout(double timeout);
        /** Twice the current iterations, at most MAX_DITHER_ITERATIONS. 0 if it can't grow. */
        static int raisedDitherIterations(int iterations);

        /**
         * @brief diagnose Build the report of a calibration, guiding or dither failure.
         * @param facts what Guide knew at the failure
         * @param tried item ids already applied in this failure chain; they are not offered again
         * @param dismissed item ids the user dismissed; they are not offered
         */
        static Diagnosis::Report diagnose(const GuideFacts &facts, const QStringList &tried = QStringList(),
                                          const QStringList &dismissed = QStringList());

        /**
         * @brief checkup Check the Guide setup when nothing failed: the camera, the device the pulses go to, the
         * mount, the optics, and the guide rate the last calibration measured. Its items never start guiding.
         */
        static Diagnosis::Report checkup(const GuideFacts &facts, const QStringList &dismissed = QStringList());
};

}
