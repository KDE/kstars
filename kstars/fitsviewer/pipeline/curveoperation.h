/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPointF>
#include <QString>
#include <QVector>
#include <opencv2/core/core.hpp>

/**
 * @class CurveOperation
 * @brief Control-point tone curve, backend only — no editor widget. Confirmed nothing
 * in the codebase to build on: `fitshistogram.h` is a levels tool (per-channel min/max
 * clip), not a curve, and there's no control-point/spline math anywhere in
 * `fitsviewer/`.
 *
 * Control points are `(x,y)` pairs in normalized `[0,1]x[0,1]` (input level -> output
 * level), sorted by `x` with distinct `x` values, interpolated with a cubic Hermite
 * spline using Catmull-Rom tangents (finite differences between neighboring points) —
 * the standard approach most tone-curve editors use under the hood. This evaluates
 * the curve directly per pixel (closed-form Hermite formula, cheap
 * given curves typically have a handful of control points) rather than through a
 * generated LUT — avoids quantization error in a value that gets baked into the saved
 * science data, and control points are few enough that a per-pixel segment lookup costs
 * about the same as a LUT read anyway.
 */
class CurveOperation
{
    public:
        /**
         * @brief Apply the same curve identically to every channel of `image` (the
         * common case — a single tone curve, not per-channel color grading).
         * @param image a CV_32F image (1 or 3 channels) in [0,1]; modified in place
         * @param controlPoints at least 2 points, sorted by x, distinct x values, each
         * in [0,1]x[0,1] — the caller is responsible for supplying the curve's actual
         * endpoints (e.g. (0,0) and (1,1) for an identity-anchored curve); this doesn't
         * assume or inject them
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool apply(cv::Mat &image, const QVector<QPointF> &controlPoints, QString &error);

        /**
         * @brief Apply independent curves per channel — per-channel color grading.
         * `channelPoints` must have exactly
         * as many entries as `image` has channels (1 for mono, 3 for R/G/B, in that
         * order); each entry follows the same rules as apply()'s controlPoints.
         */
        static bool applyPerChannel(cv::Mat &image, const QVector<QVector<QPointF>> &channelPoints, QString &error);

    private:
        // One cubic Hermite segment between two consecutive control points, with
        // precomputed tangents — cheap to evaluate, built once per apply() call.
        struct Segment
        {
            float x0, y0, m0;
            float x1, y1, m1;
        };

        static bool buildSegments(const QVector<QPointF> &controlPoints, std::vector<Segment> &segments,
                                  QString &error);
        static float evaluate(const std::vector<Segment> &segments, float x);
        static bool applyToChannel(cv::Mat &channel, const QVector<QPointF> &controlPoints, QString &error);
};
