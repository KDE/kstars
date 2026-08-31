/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <opencv2/core/core.hpp>

/**
 * @class BGEOperation
 * @brief Background/gradient extraction as its own independent, composable
 * post-combine step (see DenoiseOperation's class comment for why that matters).
 *
 * Second rebuild of this operation. The first attempt fit a sparse thin-plate-spline
 * surface through a grid of sampled points; on real data it produced visible dark
 * blotches wherever the local sample density was low, because a global spline has no
 * way to stay locally bounded — a handful of bad samples can pull the whole surface
 * far from the true background between them.
 *
 * This version instead builds a dense, per-pixel model: every pixel in a downsampled
 * copy of the channel participates, and the model is a heavily low-passed (repeated
 * box-blur) version of the pixels currently believed to be background. Regions
 * excluded as structure or outliers are filled in by diffusion from their surrounding
 * kept neighbors rather than by a spline extrapolating across them, which keeps the
 * model locally bounded everywhere — it can't swing further than the real pixels
 * feeding the blur around it. On top of that:
 *
 * - Iterative robust fit: model -> residual -> asymmetric sigma-clip (tighter on the
 *   bright side, since bright outliers are signal, not noise) -> refit, repeated with
 *   an early-exit once the kept-pixel set stops changing.
 * - Real, frame-wide structure protection: any pixel sitting meaningfully above the
 *   *current* model is flagged and grown outward (so a star's/nebula's faint halo is
 *   excluded too, not just its bright core), recomputed every iteration — not a fixed
 *   region of the frame.
 * - A floor on how few pixels the fit is allowed to keep, so a run of bad luck early
 *   on can never collapse the fit set to nothing.
 */
class BGEOperation
{
    public:
        /**
         * @brief Apply background/gradient extraction to `image` in place.
         * @param image a CV_32F image (1 or 3 channels); modified in place
         * @param strength [0,1]; how much of the fitted model to remove. 0 is a no-op.
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool apply(cv::Mat &image, double strength, QString &error);

    private:
        // Three passes of a box blur of the given radius: a fast, locally-bounded
        // approximation of Gaussian smoothing (finite support, so it can't ring or
        // overshoot the way a global fit can). radius < 1 is a no-op (copy).
        static cv::Mat boxBlur(const cv::Mat &src, int radius);

        // Harmonic-style inpainting: fills the pixels `mask` excludes (0) by
        // repeatedly blurring and restoring the kept pixels, then returns a final
        // blur of the whole result. `mask` is CV_8U, non-zero = kept.
        static cv::Mat inpaint(const cv::Mat &channel, const cv::Mat &mask, int radius);

        // Robust median and MAD-based sigma of `values`, restricted to where `mask`
        // is non-zero (or the whole buffer if `mask` is empty).
        static void medianAndSigma(const cv::Mat &values, const cv::Mat &mask, float &median, float &sigma);

        // Low/high robust range of `small`'s values (0.5th/99.5th percentile), used
        // to normalize a channel into a scale-independent [0,1]-ish working range
        // before applying the fixed thresholds below.
        static void robustRange(const cv::Mat &small, float &lo, float &hi);

        // Spatially-coherent mask (CV_8U, non-zero = exclude) of pixels sitting more
        // than `threshold` above the current model, grown outward by `amount`.
        static cv::Mat structureMask(const cv::Mat &residual, int radius, float threshold, float amount);

        // One channel's full iterative background model + correction, blended by
        // `strength`.
        static cv::Mat processChannel(const cv::Mat &channel, double strength);
};
