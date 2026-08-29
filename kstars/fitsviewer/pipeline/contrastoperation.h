/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <opencv2/core/core.hpp>

/**
 * @class ContrastOperation
 * @brief Simple pivot-and-scale contrast, matching most editors' basic contrast
 * slider. §4b's curves remain available for anyone who wants finer control than a
 * single slider gives — this is deliberately just the common case.
 */
class ContrastOperation
{
    public:
        /**
         * @brief Scale contrast of `image` in place, pivoted on the image's own mean
         * (a single scalar shared across all channels, not computed per-channel) so
         * contrast adjustment doesn't shift color balance the way an independent
         * per-channel pivot could.
         * @param image a CV_32F image, 1 or 3 channels; expected in [0,1] (e.g. after
         * AutoStretch) since the result is clamped to [0,1] — this is a "final touches"
         * step, meant to run after tone mapping, not on raw linear ADU data
         * @param amt contrast scale — 1.0 = unchanged, 0.0 = flat at the pivot,
         * >1.0 = more contrast
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool apply(cv::Mat &image, double amt, QString &error);
};
