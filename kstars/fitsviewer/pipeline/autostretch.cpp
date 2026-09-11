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
