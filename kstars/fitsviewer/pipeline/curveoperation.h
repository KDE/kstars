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
         * @param image a CV_32F image (1 or 3 channels); must already be normalized to
         * [0,1] (e.g. after AutoStretch) — any input past the curve's last control point
         * clamps to that point's y, so still-linear data collapses to one flat value.
         * Fails (see SaturationOperation/ContrastOperation's identical guard) rather
         * than silently doing that; modified in place
         * @param controlPoints at least 2 points, sorted by x, distinct x values, each
         * in [0,1]x[0,1] — the caller is responsible for supplying the curve's actual
         * endpoints (e.g. (0,0) and (1,1) for an identity-anchored curve); this doesn't
         * assume or inject them
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool apply(cv::Mat &image, const QVector<QPointF> &controlPoints, QString &error);

        /**
         * @brief Apply the same curve identically to every channel, mapping the curve's
         * normalized x onto the image's own `[inputMin, inputMax]` value range.
         *
         * This is the overload that lets a curve *be* the stretch rather than only a
         * post-stretch refinement: on still-linear ADU-scale data the input range comes
         * from the data (see CurveOperation::deriveInputRange()), so `x=0` means "the
         * darkest level present" and `x=1` means "the brightest", and a curve that lifts
         * the low region stretches the image exactly as a PixInsight-style
         * CurvesTransformation would.
         *
         * Pixels outside `[inputMin, inputMax]` clamp to the range's ends before the
         * curve is evaluated — deliberately, and unlike the 3-argument overload, whose
         * clamping to the curve's endpoints is what made a still-linear image collapse
         * to one flat value. The image may be of any value range here; there is no
         * normalization precondition, so the full-image max scan that overload needs is
         * not performed.
         *
         * @param inputMin,inputMax the value range the curve's normalized x maps onto;
         * `inputMax` must be greater than `inputMin`
         * @param controlPoints at least 2 points, sorted by x, distinct x values, each
         * in [0,1]x[0,1] — the caller is responsible for supplying the curve's actual
         * endpoints (e.g. (0,0) and (1,1) for an identity-anchored curve)
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool apply(cv::Mat &image, const QVector<QPointF> &controlPoints,
                          float inputMin, float inputMax, QString &error);

        /**
         * @brief Apply independent curves per channel — per-channel color grading.
         * `channelPoints` must have exactly
         * as many entries as `image` has channels (1 for mono, 3 for R/G/B, in that
         * order); each entry follows the same rules as apply()'s controlPoints.
         */
        static bool applyPerChannel(cv::Mat &image, const QVector<QVector<QPointF>> &channelPoints, QString &error);

        /**
         * @brief applyPerChannel() against an explicit input range — see the
         * domain-aware apply() above for the mapping and for why there is no
         * normalization precondition.
         */
        static bool applyPerChannel(cv::Mat &image, const QVector<QVector<QPointF>> &channelPoints,
                                    float inputMin, float inputMax, QString &error);

        /**
         * @brief A robust input range for a still-linear image, used by the domain-aware
         * apply()/applyPerChannel() when the caller doesn't supply one: the 0.1% .. 99.9%
         * percentiles over a strided sample of the whole image.
         *
         * Percentiles rather than min/max because a handful of hot/cold pixels would
         * otherwise define the axis and squash the curve's useful range into a sliver of
         * it. Strided sampling (~200k pixels, same pattern as AutoStretch::robustBackground)
         * keeps this cheap on a full-size frame — no full-resolution sort, let alone a
         * full-resolution copy.
         *
         * @param image a CV_32F image (1 or 3 channels), any value range
         * @param inputMin,inputMax receives the derived range; `inputMax` is guaranteed
         * greater than `inputMin` (a degenerate/empty image yields [0,1])
         */
        static void deriveInputRange(const cv::Mat &image, float &inputMin, float &inputMax);

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
        /**
         * @brief Apply one curve to one single-channel image, remapping each pixel from
         * the `inputMin`-anchored range onto the curve's normalized x first.
         *
         * `invRange` is passed precomputed (1 / (inputMax - inputMin)) because this runs
         * per pixel; the normalized overloads pass `0.0f, 1.0f`, which is an exact
         * identity — subtracting 0 and multiplying by 1 — so they keep their original
         * behavior bit-for-bit.
         */
        static bool applyToChannel(cv::Mat &channel, const QVector<QPointF> &controlPoints,
                                   float inputMin, float invRange, QString &error);

        // Shared precondition checks: non-empty and CV_32F.
        static bool checkImage(const cv::Mat &image, QString &error);
        // The legacy normalization guard — see the 3-argument apply().
        static bool checkNormalized(const cv::Mat &image, QString &error);
        // The domain-aware precondition: inputMax must exceed inputMin.
        static bool checkDomain(float inputMin, float inputMax, QString &error);
};
