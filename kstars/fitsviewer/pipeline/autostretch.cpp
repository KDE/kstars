/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "autostretch.h"

#include <algorithm>
#include <cmath>

AutoStretch::ChannelParams AutoStretch::computeParams(const std::vector<const cv::Mat *> &channels, float maxInput,
        float targetBackground, float shadowsClipping)
{
    std::vector<float> samples;
    for (const cv::Mat *channel : channels)
        samples.insert(samples.end(), channel->begin<float>(), channel->end<float>());

    std::vector<float> sorted = samples;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const float medianSample = sorted[sorted.size() / 2];

    std::vector<float> deviations(samples.size());
    for (size_t i = 0; i < samples.size(); i++)
        deviations[i] = std::abs(samples[i] - medianSample);
    std::nth_element(deviations.begin(), deviations.begin() + deviations.size() / 2, deviations.end());
    const float medDev = deviations[deviations.size() / 2];

    const float normalizedMedian = medianSample / maxInput;
    const float MADN = 1.4826f * medDev / maxInput;
    const bool upperHalf = normalizedMedian > 0.5f;
    const float B = targetBackground;
    const float C = shadowsClipping;

    const float shadows = (upperHalf || MADN == 0.0f) ? 0.0f
                          : std::clamp(normalizedMedian - C * MADN, 0.0f, 1.0f);
    const float highlights = (!upperHalf || MADN == 0.0f) ? 1.0f
                             : std::clamp(normalizedMedian + C * MADN, 0.0f, 1.0f);

    float X, M;
    if (!upperHalf)
    {
        X = normalizedMedian - shadows;
        M = B;
    }
    else
    {
        X = B;
        M = highlights - normalizedMedian;
    }

    float midtones;
    if (X == 0.0f) midtones = 0.0f;
    else if (X == M) midtones = 0.5f;
    else if (X == 1.0f) midtones = 1.0f;
    else midtones = ((M - 1) * X) / ((2 * M - 1) * X - M);

    return { shadows, midtones, highlights };
}

void AutoStretch::applyMTF(cv::Mat &channel, const ChannelParams &params, float maxInput)
{
    constexpr float maxOutput = 1.0f;

    const float nativeShadows = params.shadows * maxInput;
    const float nativeHighlights = params.highlights * maxInput;
    const float hsRangeFactor = (params.highlights == params.shadows) ? 1.0f
                                : 1.0f / (params.highlights - params.shadows);
    const float k1 = (params.midtones - 1) * hsRangeFactor * maxOutput / maxInput;
    const float k2 = ((2 * params.midtones) - 1) * hsRangeFactor / maxInput;
    const float midtones = params.midtones;

    channel.forEach<float>([ = ](float &pixel, const int *)
    {
        if (pixel < nativeShadows)
            pixel = 0.0f;
        else if (pixel >= nativeHighlights)
            pixel = maxOutput;
        else
        {
            const float inputFloored = pixel - nativeShadows;
            pixel = (inputFloored * k1) / (inputFloored * k2 - midtones);
        }
    });
}

bool AutoStretch::apply(cv::Mat &image, QString &error, double targetBackground, double shadowsClipping,
                        bool linked, bool neutralizeBackground)
{
    if (image.empty())
    {
        error = QStringLiteral("No image to stretch");
        return false;
    }
    if (image.depth() != CV_32F)
    {
        error = QStringLiteral("AutoStretch expects a CV_32F image");
        return false;
    }

    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    // Mirrors Stretch::recalculateInputRange()'s auto-detection: data already
    // normalized to [0,1] is treated as such, otherwise assume a 16-bit-scale ADU
    // origin (matches Stretch::getRange(TFLOAT) == 64*1024).
    double minVal, maxVal;
    cv::minMaxLoc(image.reshape(1), &minVal, &maxVal);
    const float maxInput = (maxVal <= 1.01) ? 1.0f : 65536.0f;

    // Optional background neutralization: subtract each channel's own sky background (down
    // to the lowest of the channels, so nothing is driven negative) before stretching, so a
    // *linked* stretch — one shared curve — lands the background neutral. This reaches the
    // same background neutrality `linked: false` achieves with per-channel curves, but
    // without those per-channel curve differences re-tinting stars that were already
    // colour-calibrated. Offset subtraction only (not a scale), so each channel's
    // above-background signal — the stars' colour — is left exactly as it was.
    if (neutralizeBackground && channels.size() > 1)
    {
        std::vector<float> background(channels.size());
        background[0] = robustBackground(channels[0]);
        float lowest = background[0];
        for (size_t i = 1; i < channels.size(); i++)
        {
            background[i] = robustBackground(channels[i]);
            lowest = std::min(lowest, background[i]);
        }
        for (size_t i = 0; i < channels.size(); i++)
            if (background[i] > lowest)
                channels[i] -= (background[i] - lowest);
    }

    if (linked && channels.size() > 1)
    {
        std::vector<const cv::Mat *> allChannels;
        for (const auto &channel : channels)
            allChannels.push_back(&channel);
        const ChannelParams params = computeParams(allChannels, maxInput,
                                     static_cast<float>(targetBackground), static_cast<float>(shadowsClipping));
        for (auto &channel : channels)
            applyMTF(channel, params, maxInput);
    }
    else
    {
        for (auto &channel : channels)
        {
            const ChannelParams params = computeParams({ &channel }, maxInput,
                                         static_cast<float>(targetBackground), static_cast<float>(shadowsClipping));
            applyMTF(channel, params, maxInput);
        }
    }

    cv::merge(channels, image);
    return true;
}

float AutoStretch::robustBackground(const cv::Mat &channel)
{
    if (channel.empty() || channel.channels() != 1 || channel.depth() != CV_32F)
        return 0.0f;

    // Strided sample to ~200k pixels: enough for a stable low percentile, independent of
    // frame resolution, and cheap even on a full-size frame (no full-resolution sort).
    constexpr size_t targetSamples = 200000;
    const int step = std::max(1, static_cast<int>(std::lround(
                                  std::sqrt(static_cast<double>(channel.total()) / targetSamples))));

    std::vector<float> samples;
    samples.reserve(targetSamples + 1);
    for (int y = 0; y < channel.rows; y += step)
    {
        const float *row = channel.ptr<float>(y);
        for (int x = 0; x < channel.cols; x += step)
            samples.push_back(row[x]);
    }
    if (samples.empty())
        return 0.0f;

    const size_t k = samples.size() / 4; // 25th percentile
    std::nth_element(samples.begin(), samples.begin() + k, samples.end());
    return std::max(0.0f, samples[k]);
}

bool AutoStretch::equivalentCurve(const cv::Mat &image, float inputMin, float inputMax, int controlPoints,
                                  bool linked, QVector<QVector<QPointF>> &out, QString &error,
                                  double targetBackground, double shadowsClipping)
{
    out.clear();
    if (image.empty())
    {
        error = QStringLiteral("No image to derive a stretch curve from");
        return false;
    }
    if (image.depth() != CV_32F)
    {
        error = QStringLiteral("AutoStretch expects a CV_32F image");
        return false;
    }
    if (!(inputMax > inputMin))
    {
        error = QStringLiteral("A stretch curve needs a non-empty input range");
        return false;
    }
    if (controlPoints < 2)
    {
        error = QStringLiteral("A curve needs at least 2 control points");
        return false;
    }

    std::vector<cv::Mat> channels;
    cv::split(image, channels);
    if (channels.empty())
    {
        error = QStringLiteral("No channels to derive a stretch curve from");
        return false;
    }

    // Same detection apply() uses, so these curves reproduce what apply() would do to this
    // image rather than merely resembling it.
    double minVal, maxVal;
    cv::minMaxLoc(image.reshape(1), &minVal, &maxVal);
    const float maxInput = (maxVal <= 1.01) ? 1.0f : 65536.0f;

    // Linked pools every channel into one parameter set, exactly as apply() does; unlinked
    // gives each its own. Either way there is one params set per channel below.
    std::vector<ChannelParams> params(channels.size());
    if (linked && channels.size() > 1)
    {
        std::vector<const cv::Mat *> all;
        for (const auto &channel : channels)
            all.push_back(&channel);
        const ChannelParams pooled = computeParams(all, maxInput, static_cast<float>(targetBackground),
                                      static_cast<float>(shadowsClipping));
        std::fill(params.begin(), params.end(), pooled);
    }
    else
    {
        for (size_t i = 0; i < channels.size(); i++)
            params[i] = computeParams({ &channels[i] }, maxInput, static_cast<float>(targetBackground),
                                      static_cast<float>(shadowsClipping));
    }

    const auto sample = [&](const ChannelParams &p)
    {
        QVector<QPointF> points;
        points.reserve(controlPoints);

        // Log-spaced x, deliberately not uniform. On still-linear data the sky background
        // sits a fraction of a percent up the derived range, and the MTF's entire usable
        // transition happens below a few percent of x — so uniformly spaced points sample
        // the empty top of the axis and miss where every pixel actually is. Measured against
        // synthetic linear frames (sky background + nebulosity + a bright tail), uniform
        // 5-point sampling is off by ~0.23 output units on average and up to ~0.49 across
        // the pixels — i.e. the seed renders the image roughly half as bright as the
        // autostretch it is meant to reproduce — while log spacing lands within ~0.002.
        //
        // kLowestX is how far down the axis the sampling reaches; 1e-3 sits below the
        // background for every realistic frame, including already-normalized data.
        constexpr float kLowestX = 1e-3f;
        for (int k = 0; k < controlPoints; k++)
        {
            float x;
            if (k == 0)
                x = 0.0f;
            else if (k == controlPoints - 1)
                x = 1.0f;
            else
                x = std::pow(kLowestX, 1.0f - static_cast<float>(k) / static_cast<float>(controlPoints - 1));

            const float value = inputMin + x * (inputMax - inputMin);

            // applyMTF()'s transfer function, in normalized terms: clip at the
            // shadow/highlight points, then the midtones transfer function itself. The
            // clipping is part of the transfer function, not an afterthought — it is where
            // the black and white points come from.
            const float normalized = value / maxInput;
            float y;
            if (p.highlights <= p.shadows)
            {
                // Degenerate (a flat frame collapses shadows onto highlights): no tone
                // mapping is defined, so stay linear rather than dividing by zero.
                y = normalized;
            }
            else if (normalized <= p.shadows)
                y = 0.0f;
            else if (normalized >= p.highlights)
                y = 1.0f;
            else
            {
                const float t = (normalized - p.shadows) / (p.highlights - p.shadows);
                const float denominator = ((2.0f * p.midtones) - 1.0f) * t - p.midtones;
                // 't' at the midtones pole maps to white.
                y = (denominator == 0.0f) ? 1.0f : ((p.midtones - 1.0f) * t) / denominator;
            }

            // Explicit comparisons rather than std::clamp: a NaN would slip straight through
            // clamp's tests and into the curve.
            if (!(y > 0.0f))
                y = 0.0f;
            else if (y > 1.0f)
                y = 1.0f;

            points << QPointF(x, y);
        }
        return points;
    };

    out.reserve(static_cast<int>(channels.size()));
    for (size_t i = 0; i < channels.size(); i++)
        out.push_back(sample(params[i]));
    return true;
}
