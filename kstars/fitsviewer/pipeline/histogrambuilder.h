/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVector>
#include <opencv2/core/core.hpp>
#include <vector>

/**
 * @class HistogramBuilder
 * @brief Bins a working image into a display histogram for the app's curve editor.
 *
 * Exists because the curve editor is drawn against a histogram of the *image the curve
 * will be applied to*, and that image is not necessarily the current working buffer: a
 * curve that runs after an autostretch is evaluated against the post-stretch data, which
 * only exists once the stretch has been applied. Drawing the user's curve against the
 * pre-stretch distribution would put the curve on a different axis than the one it is
 * evaluated on. Hence the optional projection below, which mirrors what PreviewRenderer
 * already does for the JPEG preview (downscale, then autostretch) so the histogram and
 * the preview the user is looking at agree.
 *
 * Cost is bounded the same way as PreviewRenderer: the image is downscaled before any
 * other work, then optionally stretched, then binned — so this stays cheap enough to
 * serve on the interactive path regardless of the source resolution.
 */
class HistogramBuilder
{
    public:
        /**
         * @brief One channel's (or the luminance's) distribution, normalized to [0,1]
         * against the largest bin so the caller can scale it straight into a plot.
         */
        struct Result
        {
            // Bin count and the value range those bins span. The range is what the caller
            // needs in hand to draw the same axis the curve was evaluated against.
            int bins { 0 };
            float inputMin { 0.0f };
            float inputMax { 1.0f };
            // Always populated. Luminance is what the single shared-curve editor draws
            // behind its curve.
            QVector<float> luminance;
            // Populated only for a 3-channel image, in R/G/B order — what the per-channel
            // (RGB) curve editor draws.
            QVector<float> red;
            QVector<float> green;
            QVector<float> blue;
        };

        /**
         * @brief Bin `image` into a display histogram.
         * @param image a CV_32F image with 1 or 3 channels, any value range; not modified
         * @param bins number of bins (at least 2)
         * @param haveInputRange when true, bin over [inputMin, inputMax] instead of
         * deriving the range from the image — the caller passes the range it just got
         * back from a curve application, so the histogram shows exactly that axis
         * @param inputMin,inputMax the range to bin over when `haveInputRange` is true
         * @param projectAutoStretch when true, apply a throwaway autostretch to the
         * downscaled copy before binning, so the histogram reflects the data *after* the
         * autostretch a following curve would be applied to
         * @param targetBackground,shadowsClipping,linked,neutralizeBackground the
         * autostretch parameters to project with; ignored when `projectAutoStretch` is
         * false
         * @param result receives the binned (and normalized) distribution
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool build(const cv::Mat &image, int bins,
                          bool haveInputRange, float inputMin, float inputMax,
                          bool projectAutoStretch, double targetBackground, double shadowsClipping,
                          bool linked, bool neutralizeBackground,
                          Result &result, QString &error);

    private:
        // Longest-side cap for the working copy, matching PreviewRenderer's own default so
        // the histogram and the preview it accompanies are computed from the same data.
        static constexpr int kMaxDimension = 1024;

        // Bin one single-channel float image into `counts` (sized `bins`), over
        // [rangeMin, rangeMin + 1/invRange].
        static void binChannel(const cv::Mat &channel, int bins, float rangeMin, float invRange,
                               std::vector<double> &counts);
};
