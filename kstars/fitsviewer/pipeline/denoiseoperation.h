/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitsviewer/fitscommon.h"

#include <QString>
#include <opencv2/core/core.hpp>

/**
 * @class DenoiseOperation
 * @brief Noise reduction as its own independent, composable post-combine step —
 * unlike `FITSStack::postProcessImage()`'s bundled gradient/deconv/sharpen/denoise
 * blob (which always runs pre-combine, per-channel, as part of `postprocess_start`/
 * `postprocess_redo_postprocess`, and can't be applied on its own without re-running
 * everything else), this operates on the current working image in place, exactly like
 * `CropOperation`/`CurveOperation`/`SaturationOperation`/`ContrastOperation` — so it
 * composes correctly in a Crop -> BGE -> Color -> Denoise -> Stretch sequence, each
 * step building on the last rather than resetting to some earlier buffer.
 *
 * Two independent passes, both optional:
 * - Luminance denoise: per-channel 3-level Gaussian-pyramid decomposition (successive
 *   blurs at sigma=0.8/1.6/3.2, differenced into 3 detail layers + a residual), noise
 *   suppressed in the two finest detail layers via an adaptive MAD-based threshold
 *   (each layer's own robust noise sigma, not a fixed constant that only makes sense
 *   at one specific pixel-value scale). `method` chooses HARD (binary keep/kill) or
 *   SOFT (Donoho-style shrinkage) thresholding.
 * - Chroma denoise: targets only inter-channel color noise (uncorrelated per-channel
 *   shot/read noise on an OSC sensor) by decomposing into luma and scale-agnostic
 *   color-difference planes (Cr=R-Y, Cb=B-Y — not cv::COLOR_BGR2YCrCb, whose baked-in
 *   0.5 midpoint offset only round-trips for [0,1]-normalized data), Gaussian-blurring
 *   only the color-difference planes, and reconstructing — so luminance detail stays
 *   sharp while only color-noise grain gets smoothed. 3-channel images only.
 *
 * This is the same algorithm previously embedded directly in
 * `FITSStack::postProcessImage()`, extracted here as the single source of truth —
 * `postProcessImage()` now calls into this too, rather than duplicating the logic.
 */
class DenoiseOperation
{
    public:
        /**
         * @brief Apply luminance and/or chroma denoise to `image` in place.
         * @param image a CV_32F image (1 or 3 channels); modified in place
         * @param amt luminance denoise strength, [0,1]; <= 0 skips this pass
         * @param method HARD or SOFT thresholding for the luminance pass
         * @param chromaAmt chroma denoise strength, [0,1]; <= 0 skips this pass.
         * Ignored (no-op) on a 1-channel image.
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool apply(cv::Mat &image, double amt, DenoiseMethod method, double chromaAmt, QString &error);

        // Robust (MAD-based) noise-sigma estimate for a zero-mean detail/residual
        // layer, calibrated to a Gaussian-equivalent standard deviation (1.4826x
        // median of |d|). Public so other pipeline code needing the same robust-sigma
        // estimator (e.g. a background-extraction step) doesn't have to duplicate it.
        static float robustSigma(const cv::Mat &d);

    private:
        static cv::Mat luminanceDenoise(const cv::Mat &image, double amt, DenoiseMethod method);
        static cv::Mat chromaDenoise(const cv::Mat &image, double amt);
};
