/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRect>
#include <QString>
#include <opencv2/core/core.hpp>

struct wcsprm;

/**
 * @class CropOperation
 * @brief cv::Mat ROI extraction plus the matching WCS reference-pixel adjustment, as a
 * pure function with no FITSData/FITSStack dependency.
 *
 * Split out from `FITSData::cropStack()` (the thin wrapper that actually gets called by
 * the pipeline — it knows which cv::Mat/wcsprm belong to the current stacked image, this
 * doesn't) specifically so the crop+WCS math can be unit-tested directly: build a
 * synthetic wcsprm with plain wcslib calls, no plate solve or live-stack session needed.
 */
class CropOperation
{
    public:
        /**
         * @brief Crop `image` to `roi` in place, and if `wcs` is non-null, shift its
         * reference pixel (CRPIX) by the crop offset and re-run wcsset() so sky
         * coordinates at any given pixel are unchanged by the crop.
         * @param image cropped in place (replaced with the ROI sub-image)
         * @param roi region to keep, in 0-indexed pixel coordinates against `image`'s
         * current (pre-crop) size
         * @param wcs adjusted in place if non-null; pass nullptr if there's no WCS to
         * preserve (e.g. an unsolved stack)
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool apply(cv::Mat &image, const QRect &roi, struct wcsprm *wcs, QString &error);
};
