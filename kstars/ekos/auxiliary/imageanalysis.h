/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRect>
#include <QString>

class FITSData;

namespace Ekos
{

/** What is wrong with a frame, if anything, judged from the shapes of its stars. */
enum class ImageVerdict
{
    Unknown = 0,    ///< Not measured, or no star could be measured
    Good,           ///< Enough round, focused stars: the image is not the problem
    FewStars,       ///< Too few stars: clouds, cap, short exposure
    OutOfFocus,     ///< Round stars with the flat profile, or the hole, of defocused stars
    Trailed,        ///< Stars smeared in one direction: tracking
    DoubleImages    ///< Every star doubled with the same offset: a mechanical jump during the exposure
};

/** Stable machine name of a verdict, e.g. "trailed". */
QString imageVerdictKey(ImageVerdict verdict);

/** Statistics of the stars of a frame. Sizes are in pixels of the binned frame. */
struct ImageStats
{
    ImageVerdict verdict {ImageVerdict::Unknown};
    /// Stars in the whole frame, estimated from the measured region when only part of the frame was measured
    int stars {0};
    /// Stars found in the measured region
    int measuredStars {0};
    /// Bright, isolated stars whose profile was measured, at most MAX_SHAPE_STARS
    int profiledStars {0};
    /// Fraction of the frame area that was measured, see measurementRegion()
    double measuredFraction {1};
    double hfr {-1};            ///< median half flux radius, -1 if not measured
    /// Median R80 / R20, the radii holding 80% and 20% of the light: about 2.7 for a Gaussian star and 3 or more
    /// for a seeing-limited one, 2.0 for the flat disc of a defocused star. -1 if not measured.
    double concentration {-1};
    /// Median surface brightness of the centre of a star against its brightest ring: about 1 for a filled star,
    /// lower for the donut of a defocused telescope with a central obstruction.
    double centreDip {1};
    double eccentricity {-1};   ///< median eccentricity
    double elongation {1};      ///< median a/b
    double coherence {0};       ///< 0 when the major axes point anywhere, 1 when they are all parallel
    double angle {0};           ///< mean direction of the major axes in degrees, 0 to 180
    double trailLength {0};     ///< median trail length in pixels, sqrt(12 (a^2 - b^2))
    double doubleOffset {0};    ///< distance between the two images of a doubled star, pixels
    double doubleFraction {0};  ///< fraction of the bright stars that have a twin at that offset
    double median {0};          ///< median pixel value of the frame
    double max {0};             ///< maximum pixel value of the frame
};

/**
 * @namespace Ekos::ImageAnalysis
 * @short Tells what is wrong with a frame from the shapes of its stars, e.g. why it failed to solve, or why
 * focus or guiding failed on it.
 *
 * Stars are detected with OpenCV on the frame without its background. The profile of the brightest isolated,
 * unsaturated stars is measured from all their pixels against a local background, with no detection
 * threshold: a growth curve for the concentration and the half flux radius, the centre against the rings for
 * donuts, and the second moments for elongation and direction. The medians of ImageStats weigh each star by the
 * square root of its flux, so that a few bright stars outweigh many faint detections such as hot pixels.
 * Doubled stars are found as pairs of peaks at the same offset all over the frame.
 */
namespace ImageAnalysis
{

/// Fewer stars than this are too few to solve, or to measure the shapes reliably
constexpr int MIN_STARS = 10;
/// Most stars whose profile is measured, the brightest isolated ones
constexpr int MAX_SHAPE_STARS = 30;
/// Fewest measured profiles to tell the shape of the stars of a frame
constexpr int MIN_SHAPE_STARS = 3;
/// Median R80 / R20 below which round stars are out of focus. A focused star is at least as concentrated as a
/// Gaussian, 2.69; defocused discs are 2.0 to 2.4.
constexpr double MIN_FOCUSED_CONCENTRATION = 2.5;
/// Median centre dip below which round stars are defocused donuts
constexpr double MAX_FOCUSED_CENTRE_DIP = 0.85;
/// Median half flux radius below which stars are too undersampled to tell their profile
constexpr double MIN_PROFILE_HFR = 1.2;
/// Median a/b from which stars are trailed, if they are also parallel. The second moments include the faint
/// wings of the stars, so a trail reads less elongated than with a detection threshold.
constexpr double MIN_TRAIL_ELONGATION = 1.4;
/// Coherence of the star directions from which elongated stars are trailed rather than distorted by the optics
constexpr double MIN_TRAIL_COHERENCE = 0.6;
/// Fraction of the bright stars with a twin at the same offset from which the frame has double images
constexpr double MIN_DOUBLE_FRACTION = 0.5;
/// Sidereal rate in arcseconds per second of time
constexpr double SIDEREAL_RATE = 15.041;
/// Largest width and height measured. A larger frame is measured in its centre, which has the least coma and
/// tilt, and enough stars to tell the shape of the stars of the whole frame.
constexpr int MAX_MEASURED_SIZE = 2048;

/**
 * @brief measurementRegion The region of a frame to measure: its centre, at most MAX_MEASURED_SIZE wide and high.
 * @return the whole frame if it is small enough, an empty rectangle if the frame has no size
 */
QRect measurementRegion(int width, int height);

/**
 * @brief analyse Measure the stars of a frame and tell what is wrong with it.
 * It takes about 0.06 s for a 2048 x 2048 region: call it from a worker thread, e.g. with QtConcurrent::run().
 * The frame must not change meanwhile.
 * @param data the frame. Colour frames are measured on the mean of their channels.
 * @param region the region to measure, in frame pixels; an empty rectangle, the default, measures
 * measurementRegion(). The star count of the whole frame is estimated from it.
 */
ImageStats analyse(const FITSData &data, const QRect &region = QRect());

/**
 * @brief siderealDrift How far a star drifts across the frame when the mount does not track.
 * @param exposure exposure in seconds
 * @param declination declination in degrees
 * @param pixelScale arcseconds per binned pixel
 * @return the drift in binned pixels, 0 if the scale is unknown
 */
double siderealDrift(double exposure, double declination, double pixelScale);

}
}
