/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "curveoperation.h"

#include <algorithm>
#include <cmath>

bool CurveOperation::buildSegments(const QVector<QPointF> &controlPoints, std::vector<Segment> &segments,
                                   QString &error)
{
    if (controlPoints.size() < 2)
    {
        error = QStringLiteral("A curve needs at least 2 control points");
        return false;
    }

    for (int i = 1; i < controlPoints.size(); i++)
    {
        if (controlPoints[i].x() <= controlPoints[i - 1].x())
        {
            error = QStringLiteral("Curve control points must be sorted with strictly increasing x");
            return false;
        }
    }

    const int n = controlPoints.size();
    // Catmull-Rom tangents: each interior point's tangent is the slope between its
    // neighbors; boundary points fall back to the one-sided secant slope (no
    // duplicated virtual endpoint needed).
    QVector<float> tangents(n);
    for (int i = 0; i < n; i++)
    {
        if (i == 0)
            tangents[i] = (controlPoints[1].y() - controlPoints[0].y())
                          / (controlPoints[1].x() - controlPoints[0].x());
        else if (i == n - 1)
            tangents[i] = (controlPoints[n - 1].y() - controlPoints[n - 2].y())
                          / (controlPoints[n - 1].x() - controlPoints[n - 2].x());
        else
            tangents[i] = (controlPoints[i + 1].y() - controlPoints[i - 1].y())
                          / (controlPoints[i + 1].x() - controlPoints[i - 1].x());
    }

    segments.clear();
    for (int i = 0; i < n - 1; i++)
    {
        segments.push_back(
        {
            static_cast<float>(controlPoints[i].x()), static_cast<float>(controlPoints[i].y()), tangents[i],
            static_cast<float>(controlPoints[i + 1].x()), static_cast<float>(controlPoints[i + 1].y()), tangents[i + 1]
        });
    }
    return true;
}

float CurveOperation::evaluate(const std::vector<Segment> &segments, float x)
{
    if (x <= segments.front().x0)
        return segments.front().y0;
    if (x >= segments.back().x1)
        return segments.back().y1;

    // Segments are few in practice (a handful of control points) — linear scan is fine.
    const Segment *seg = &segments.front();
    for (const auto &s : segments)
    {
        if (x <= s.x1)
        {
            seg = &s;
            break;
        }
    }

    const float h = seg->x1 - seg->x0;
    const float t = (x - seg->x0) / h;
    const float t2 = t * t;
    const float t3 = t2 * t;

    const float h00 = 2 * t3 - 3 * t2 + 1;
    const float h10 = t3 - 2 * t2 + t;
    const float h01 = -2 * t3 + 3 * t2;
    const float h11 = t3 - t2;

    return h00 * seg->y0 + h10 * h * seg->m0 + h01 * seg->y1 + h11 * h * seg->m1;
}

bool CurveOperation::applyToChannel(cv::Mat &channel, const QVector<QPointF> &controlPoints,
                                    float inputMin, float invRange, QString &error)
{
    std::vector<Segment> segments;
    if (!buildSegments(controlPoints, segments, error))
        return false;

    // Remap the pixel's value onto the curve's normalized x before evaluating, so the
    // curve's own range is decoupled from the data's range. A pixel outside the range
    // clamps to its end — see the domain-aware apply()'s doc comment for why that is the
    // whole point here, rather than the accident it was on still-linear data. The
    // normalized callers pass inputMin=0 / invRange=1, an exact identity, so they keep
    // their original per-pixel behavior bit-for-bit.
    channel.forEach<float>([&segments, inputMin, invRange](float &pixel, const int *)
    {
        const float normalized = std::clamp((pixel - inputMin) * invRange, 0.0f, 1.0f);
        pixel = evaluate(segments, normalized);
    });
    return true;
}

bool CurveOperation::checkImage(const cv::Mat &image, QString &error)
{
    if (image.empty())
    {
        error = QStringLiteral("No image to apply a curve to");
        return false;
    }
    if (image.depth() != CV_32F)
    {
        error = QStringLiteral("CurveOperation expects a CV_32F image");
        return false;
    }
    return true;
}

bool CurveOperation::checkNormalized(const cv::Mat &image, QString &error)
{
    // evaluate() clamps any x past the curve's last control point to that point's y —
    // on still-linear/un-stretched data (background in the hundreds-thousands,
    // saturation ~65535) every pixel sits past x1, so the whole image collapses to one
    // flat value (typically 1.0/solid white for an identity-anchored curve). Refuse
    // instead, matching SaturationOperation/ContrastOperation's guard for the same
    // precondition. A caller that *wants* a curve against un-normalized data declares the
    // range via the domain-aware overloads instead (see deriveInputRange()).
    double minVal, maxVal;
    cv::minMaxLoc(image.reshape(1), &minVal, &maxVal);
    if (maxVal > 1.5)
    {
        error = QString("CurveOperation expects a normalized [0,1] image (e.g. after "
                        "AutoStretch) — got values up to %1").arg(maxVal);
        return false;
    }
    return true;
}

bool CurveOperation::checkDomain(float inputMin, float inputMax, QString &error)
{
    if (!(inputMax > inputMin))
    {
        error = QString("CurveOperation needs a non-empty input range — got [%1, %2]")
                .arg(static_cast<double>(inputMin)).arg(static_cast<double>(inputMax));
        return false;
    }
    return true;
}

bool CurveOperation::apply(cv::Mat &image, const QVector<QPointF> &controlPoints, QString &error)
{
    if (!checkImage(image, error) || !checkNormalized(image, error))
        return false;

    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    for (auto &channel : channels)
    {
        if (!applyToChannel(channel, controlPoints, 0.0f, 1.0f, error))
            return false;
    }

    cv::merge(channels, image);
    return true;
}

bool CurveOperation::apply(cv::Mat &image, const QVector<QPointF> &controlPoints,
                           float inputMin, float inputMax, QString &error)
{
    if (!checkImage(image, error) || !checkDomain(inputMin, inputMax, error))
        return false;

    const float invRange = 1.0f / (inputMax - inputMin);

    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    for (auto &channel : channels)
    {
        if (!applyToChannel(channel, controlPoints, inputMin, invRange, error))
            return false;
    }

    cv::merge(channels, image);
    return true;
}

bool CurveOperation::applyPerChannel(cv::Mat &image, const QVector<QVector<QPointF>> &channelPoints, QString &error)
{
    if (!checkImage(image, error))
        return false;
    if (channelPoints.size() != image.channels())
    {
        error = QString("Got %1 per-channel curves for a %2-channel image").arg(channelPoints.size()).arg(
                    image.channels());
        return false;
    }
    if (!checkNormalized(image, error))
        return false;

    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    for (int c = 0; c < static_cast<int>(channels.size()); c++)
    {
        if (!applyToChannel(channels[c], channelPoints[c], 0.0f, 1.0f, error))
            return false;
    }

    cv::merge(channels, image);
    return true;
}

bool CurveOperation::applyPerChannel(cv::Mat &image, const QVector<QVector<QPointF>> &channelPoints,
                                     float inputMin, float inputMax, QString &error)
{
    if (!checkImage(image, error))
        return false;
    if (channelPoints.size() != image.channels())
    {
        error = QString("Got %1 per-channel curves for a %2-channel image").arg(channelPoints.size()).arg(
                    image.channels());
        return false;
    }
    if (!checkDomain(inputMin, inputMax, error))
        return false;

    const float invRange = 1.0f / (inputMax - inputMin);

    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    for (int c = 0; c < static_cast<int>(channels.size()); c++)
    {
        if (!applyToChannel(channels[c], channelPoints[c], inputMin, invRange, error))
            return false;
    }

    cv::merge(channels, image);
    return true;
}

void CurveOperation::deriveInputRange(const cv::Mat &image, float &inputMin, float &inputMax)
{
    inputMin = 0.0f;
    inputMax = 1.0f;
    if (image.empty() || image.depth() != CV_32F || image.channels() > 3)
        return;

    // Strided sample to ~200k pixels: enough for a stable 0.1%/99.9% percentile,
    // independent of frame resolution, and cheap even on a full-size frame — no
    // full-resolution sort and no full-resolution copy. Same pattern as
    // AutoStretch::robustBackground().
    constexpr size_t targetSamples = 200000;
    const int step = std::max(1, static_cast<int>(std::lround(
                                  std::sqrt(static_cast<double>(image.total()) / targetSamples))));

    std::vector<float> samples;
    samples.reserve(targetSamples + 4);
    for (int y = 0; y < image.rows; y += step)
    {
        for (int x = 0; x < image.cols; x += step)
        {
            const float *pixel = image.ptr<float>(y, x);
            for (int c = 0; c < image.channels(); c++)
                samples.push_back(pixel[c]);
        }
    }
    if (samples.size() < 2)
        return;

    // 0.1% .. 99.9% rather than min/max: a handful of hot/cold pixels would otherwise
    // define the axis and squash everything interesting into a sliver of it.
    const size_t lowIndex = samples.size() / 1000;
    const size_t highIndex = samples.size() - 1 - samples.size() / 1000;
    std::nth_element(samples.begin(), samples.begin() + lowIndex, samples.end());
    const float low = samples[lowIndex];
    std::nth_element(samples.begin(), samples.begin() + highIndex, samples.end());
    const float high = samples[highIndex];

    if (high > low)
    {
        inputMin = low;
        inputMax = high;
        return;
    }

    // Percentiles collapsed (e.g. a synthetic flat frame) — fall back to the true
    // extremes, and to [0,1] if even those collapse.
    double minVal, maxVal;
    cv::minMaxLoc(image.reshape(1), &minVal, &maxVal);
    inputMin = static_cast<float>(minVal);
    inputMax = static_cast<float>(maxVal);
    if (!(inputMax > inputMin))
    {
        inputMin = 0.0f;
        inputMax = 1.0f;
    }
}
