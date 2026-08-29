/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "curveoperation.h"

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

bool CurveOperation::applyToChannel(cv::Mat &channel, const QVector<QPointF> &controlPoints, QString &error)
{
    std::vector<Segment> segments;
    if (!buildSegments(controlPoints, segments, error))
        return false;

    channel.forEach<float>([&segments](float &pixel, const int *)
    {
        pixel = evaluate(segments, pixel);
    });
    return true;
}

bool CurveOperation::apply(cv::Mat &image, const QVector<QPointF> &controlPoints, QString &error)
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

    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    for (auto &channel : channels)
    {
        if (!applyToChannel(channel, controlPoints, error))
            return false;
    }

    cv::merge(channels, image);
    return true;
}

bool CurveOperation::applyPerChannel(cv::Mat &image, const QVector<QVector<QPointF>> &channelPoints, QString &error)
{
    if (image.empty())
    {
        error = QStringLiteral("No image to apply curves to");
        return false;
    }
    if (image.depth() != CV_32F)
    {
        error = QStringLiteral("CurveOperation expects a CV_32F image");
        return false;
    }
    if (channelPoints.size() != image.channels())
    {
        error = QString("Got %1 per-channel curves for a %2-channel image").arg(channelPoints.size()).arg(
                    image.channels());
        return false;
    }

    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    for (int c = 0; c < static_cast<int>(channels.size()); c++)
    {
        if (!applyToChannel(channels[c], channelPoints[c], error))
            return false;
    }

    cv::merge(channels, image);
    return true;
}
