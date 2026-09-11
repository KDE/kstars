/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <opencv2/core/core.hpp>
#include <vector>

/**
 * @class AutoStretch
 * @brief One-shot midtones-transfer-function (MTF) autostretch, baked permanently into
 * the image rather than applied only for display.
 *
 * `stretch.{h,cpp}` already implements this exact algorithm (median + MADN robust
 * statistics feeding the XISF-spec MTF curve — see `computeParamsOneChannel()`,
 * which cites XISF spec section 8.5.7) — but it's architecturally a display path: it
 * quantizes to 8-bit and writes into a `QImage`, and its parameter-computation helpers
 * are anonymous-namespace internals of `stretch.cpp`, not linkable from outside. Baking
 * a stretch into the saved output needs to preserve float precision, so this replicates
 * the same formula (B/C in `stretch.cpp`'s terms, renamed below to describe what they
 * actually do) rather than reusing `Stretch` directly.
 */
class AutoStretch
{
    public:
        /**
         * @brief Stretch every channel of `image` in place.
         * @param image a CV_32F image (1 or 3 channels); replaced with the stretched
         * result, normalized to [0,1] regardless of the input's original ADU scale
         * @param error receives a human-readable failure reason on failure
         * @param targetBackground where the background level lands post-stretch, in
         * [0,1]; higher pulls the background brighter
         * @param shadowsClipping how many MADN (robust sigma) units below/above the
         * median to clip the shadow/highlight point; higher clips less aggressively
         * @param linked when true (default), one shadows/midtones/highlights curve is
         * computed from all channels pooled together and applied identically to each —
         * this keeps per-pixel R/G/B noise that's neutral pre-stretch neutral post-stretch.
         * When false, each channel gets its own curve from its own median/MADN, which lets
         * independent per-channel noise (OSC sensors have uncorrelated shot/read noise per
         * Bayer color, plus different debayer sample density) get stretched to a different
         * black/white point per channel — turning faint neutral background noise into
         * visible red/green/blue speckle. Keep false only for deliberate per-channel work
         * (e.g. manual color balance correction).
         * @param neutralizeBackground when true, each channel's own robust sky background is
         * subtracted (down to the lowest of the channels) before stretching, so the
         * background comes out neutral regardless of a per-channel cast — without the
         * per-channel *curve* differences that `linked: false` uses to achieve the same
         * background neutrality, which re-tint the stars. Combine with `linked: true` to get
         * a neutral background and preserved (calibrated) star colors in one step. Off by
         * default; a no-op on a mono image.
         * @return success
         */
        static bool apply(cv::Mat &image, QString &error, double targetBackground = 0.25,
                          double shadowsClipping = 2.8, bool linked = true, bool neutralizeBackground = false);

    private:
        struct ChannelParams
        {
            float shadows, midtones, highlights;
        };

        // Median + MADN robust statistics -> shadows/midtones/highlights, replicating
        // computeParamsOneChannel()'s algorithm exactly (same XISF-spec formula). Pools
        // samples from all of `channels` together, so passing all three channels yields
        // linked (shared) parameters, and passing just one yields unlinked (per-channel).
        static ChannelParams computeParams(const std::vector<const cv::Mat *> &channels, float maxInput,
                                           float targetBackground, float shadowsClipping);

        // Robust sky-background level for one channel: a low (25th) percentile over a
        // strided ~200k-pixel sample. Strided (not a full-resolution sort) so its cost is
        // independent of frame size, and a low percentile rather than the median so real
        // signal (stars, nebulosity) can't bias the sky level upward. Used by
        // apply()'s neutralizeBackground path.
        static float robustBackground(const cv::Mat &channel);

        // Applies the MTF curve to one channel in place, float output (no 8-bit
        // quantization) normalized to [0,1] — replicating stretchOneChannel()'s formula
        // with maxOutput=1.0 instead of 255.
        static void applyMTF(cv::Mat &channel, const ChannelParams &params, float maxInput);
};
