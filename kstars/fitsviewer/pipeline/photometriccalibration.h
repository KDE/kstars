/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <opencv2/core/core.hpp>
#include <vector>

/**
 * @class PhotometricCalibrationOperation
 * @brief Local, per-star color correction — nudges each star's own color toward a
 * caller-supplied target (e.g. one derived from its catalog B-V index) without
 * touching anything else in the frame.
 *
 * Deliberately has no WCS/catalog awareness of its own (unlike the caller that drives
 * it, FITSData::applyPhotometricCalibration()) — same reasoning as every other
 * operation in this directory: a plain cv::Mat in, cv::Mat out contract keeps it
 * testable in isolation via a standalone harness, without needing a live KStarsData/
 * StarComponent/WCS-solved image to exercise it.
 *
 * The color-mixing defect this exists to correct isn't any one operation's fault:
 * channel blending, chroma denoising and saturation all operate on the whole frame
 * uniformly, so a small per-channel imbalance a star picked up from being combined out
 * of independently-stacked filters (most visibly in a narrowband composite, where the
 * channels are emission lines with no inherent relationship to the star's real color at
 * all) gets carried through and then exaggerated by the final saturation boost. Nebula
 * pixels have no independently-known "correct" color to calibrate against, but a
 * catalogued star does — so this only ever touches pixels the caller identifies as
 * belonging to one.
 */
class PhotometricCalibrationOperation
{
    public:
        struct DetectedStar
        {
            cv::Point2f pixel; // centroid, image pixel coordinates
            float radiusPx;    // approximate blob radius, from its detected pixel count
        };

        /**
         * @brief Find star-like blobs in `image`.
         *
         * Percentile-threshold bright mask (same strided-sampling technique as
         * DenoiseOperation::robustSigma()/chromaDenoise()'s brightThreshold, so it
         * scales with how many bright sources are actually in the frame rather than a
         * single global max), then cv::connectedComponentsWithStats() to get individual
         * blob centroids/areas. Filtered by a minimum area (rejects single-pixel hot-
         * pixel noise) and a bounding-box aspect-ratio check (rejects elongated
         * diffraction spikes/nebula knots, which aren't stars and have no catalog
         * counterpart to match against anyway).
         *
         * @param image CV_32FC3
         * @param error receives a human-readable failure reason on failure
         */
        static std::vector<DetectedStar> detectStars(const cv::Mat &image, QString &error);

        struct StarMatch
        {
            cv::Point2f pixel;                 // centroid, image pixel coordinates
            float radiusPx;                    // from the matching DetectedStar
            float targetR, targetG, targetB;   // catalog-implied color, normalized (sum to 1)
        };

        /**
         * @brief Nudge each matched star's local color toward its target, in place.
         *
         * For each match: samples the current measured color in a small aperture at
         * its centroid, computes a per-channel gain (target/measured ratio, clamped to
         * [0.3, 3.0] against a bad match or a noisy/undersaturated measurement blowing
         * the correction up), and applies that gain through a Gaussian-weighted radial
         * multiplier field centered on the star — full strength at the core, tapering
         * to a no-op (gain 1.0) beyond a few times its detected radius — scaled overall
         * by `strength`. Everything outside every star's falloff radius is untouched,
         * by construction.
         *
         * @param image CV_32FC3, modified in place
         * @param matches stars to correct (from detectStars() + a caller-side catalog
         * cross-match); anything not represented here is left alone
         * @param strength [0,1]; 0 is a no-op, 1 applies the full computed correction
         * @param error receives a human-readable failure reason on failure
         */
        static bool apply(cv::Mat &image, const std::vector<StarMatch> &matches, double strength, QString &error);

        /**
         * @brief Approximate the normalized (sum-to-1) RGB color of a star with the
         * given B-V color index.
         *
         * B-V (not spectral type) is the field to key off: it's populated for
         * essentially every catalog star, bright or faint, while a resolved MK
         * spectral type is only available for bright/named ones (see
         * StarObject::getBVIndex()/spchar()). Nothing reusable for this conversion
         * exists elsewhere in the codebase — SkyQPainter::initStarImages() only has a
         * coarse 7-bucket O/B/A/F/G/K/M lookup table, not a continuous function.
         *
         * Converts B-V to an effective temperature via Ballesteros' formula, then
         * approximates the resulting blackbody's color by sampling Planck's law at one
         * representative wavelength per channel (600/550/450nm) rather than a full CIE
         * tristimulus integration — this only needs to produce the right *ratio*
         * between channels (the normalized color a star of this temperature should
         * read as, for nudging a measured color toward it), not a colorimetrically
         * exact render.
         * @param bvIndex B-V color index (see StarObject::getBVIndex())
         */
        static void colorFromBVIndex(float bvIndex, float &r, float &g, float &b);
};
