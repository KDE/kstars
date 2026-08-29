/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVector>
#include <opencv2/core/core.hpp>

/**
 * @class ChannelBlendOperation
 * @brief Weighted linear combination of N independently-stacked mono images into an
 * RGB result — arbitrary narrowband palettes (HOO/SHO/bicolor/anything else), not just
 * a fixed one-filter-per-slot assignment.
 *
 * The engine's own R/G/B/L channel assignment (`FITSData::initStackChannels()`) is
 * positional and 1:1 — each input directory becomes exactly one output channel, no
 * blending, and it rejects anything other than 1, 3, or 4 directories (so a 2-filter
 * narrowband set, e.g. Ha+OIII, can't be submitted as a single multi-channel stack at
 * all). This is a separate, later step: stack each filter independently as its own
 * mono session first (trivially supported today — that's just `n==1`), then combine the
 * finished, already-stacked results here with arbitrary per-channel weights, e.g.
 * green = 0.7×OIII + 0.3×Ha for a custom HOO blend. No engine changes needed — this
 * operates purely on already-produced `cv::Mat` results.
 */
class ChannelBlendOperation
{
    public:
        struct WeightedInput
        {
            cv::Mat image;   // CV_32F, single channel — one already-stacked mono session
            double weight;
        };

        /**
         * @brief Compute one output channel as the weighted sum of its inputs.
         * @param inputs one or more single-channel CV_32F images, all the same size,
         * each with a weight (weights don't need to sum to 1 — e.g. weight 1.0 on a
         * single input is a plain passthrough, matching a traditional unweighted
         * R/G/B/L assignment)
         * @param outChannel receives the weighted sum, CV_32F, single channel
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool blendChannel(const QVector<WeightedInput> &inputs, cv::Mat &outChannel, QString &error);

        /**
         * @brief Compute all three output channels and merge into one RGB image.
         * @param red / green / blue weighted inputs for each output channel (see
         * blendChannel()) — every input across all three must be the same size
         * @param outImage receives the merged CV_32FC3 result
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool blendRGB(const QVector<WeightedInput> &red, const QVector<WeightedInput> &green,
                             const QVector<WeightedInput> &blue, cv::Mat &outImage, QString &error);
};
