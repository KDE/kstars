/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <opencv2/core/core.hpp>

/**
 * @class SaturationOperation
 * @brief HSV saturation scaling.
 *
 * Requires a normalized [0,1] image (e.g. the output of AutoStretch/CurveOperation) —
 * OpenCV's cvtColor() float-image convention assumes [0,1]-range input for RGB<->HSV,
 * so running this on raw, un-stretched ADU-scale data would silently produce garbage;
 * this checks for that and fails with a clear error instead.
 */
class SaturationOperation
{
    public:
        /**
         * @brief Scale the saturation of `image` in place.
         * @param image a CV_32F, [0,1]-range image; 1 channel is a no-op (saturation
         * has no meaning for a mono/LUM-only image), 3 channels is R/G/B
         * @param amt multiplicative scale on the HSV S channel — 1.0 = unchanged,
         * 0.0 = grayscale, >1.0 = more saturated (clamped to a valid S range)
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool apply(cv::Mat &image, double amt, QString &error);
};
