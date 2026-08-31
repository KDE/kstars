/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <opencv2/core/core.hpp>

/**
 * @class PreviewRenderer
 * @brief Fast, headless JPEG preview generation for the batch post-processing
 * pipeline — no FITSView/QImage/display-pixmap machinery involved at all (unlike
 * `Media::upload()`, which requires a real `FITSView` GUI widget; constructing one
 * headlessly was found to hang in an offscreen test environment this same session).
 * Pure OpenCV compute + encode, so it has no GUI-widget dependency to hang on.
 *
 * Meant to be cheap enough to call on every interactive adjustment (e.g. a caller
 * iterating on a curves control point), not just once per wizard step — the only way
 * to make that true regardless of the source image's resolution is to downscale
 * *first*, so every subsequent step (stretch statistics, encode) operates on the
 * small copy rather than the full-resolution source.
 */
class PreviewRenderer
{
    public:
        /**
         * @brief Render `image` to a base64-encoded JPEG preview.
         * @param image a CV_32F image (1 or 3 channels), any value range — linear
         * ADU-scale data, an already-stretched [0,1] image, or anything in between.
         * Not modified.
         * @param error receives a human-readable failure reason on failure
         * @param maxDimension the output's largest dimension in pixels; the source is
         * downscaled to this *before* any other processing, bounding the cost of
         * every later step regardless of the source resolution
         * @param jpegQuality 0-100, passed straight to the JPEG encoder
         * @return base64-encoded JPEG bytes, or an empty string on failure
         */
        static QString renderBase64Jpeg(const cv::Mat &image, QString &error, int maxDimension = 1024,
                                        int jpegQuality = 85);
};
