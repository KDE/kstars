/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bgeoperation.h"

#include <opencv2/imgproc/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

cv::Mat BGEOperation::boxBlur(const cv::Mat &src, int radius)
{
    if (radius < 1)
        return src.clone();

    cv::Mat out;
    src.copyTo(out);
    const cv::Size ksize(2 * radius + 1, 2 * radius + 1);
    for (int pass = 0; pass < 3; ++pass)
        cv::boxFilter(out, out, -1, ksize, cv::Point(-1, -1), true, cv::BORDER_REPLICATE);
    return out;
}

cv::Mat BGEOperation::inpaint(const cv::Mat &channel, const cv::Mat &mask, int radius)
{
    const int n = channel.rows * channel.cols;
    const int nkept = cv::countNonZero(mask);
    if (nkept == 0 || nkept == n)
        return boxBlur(channel, radius);

    const float knownMean = static_cast<float>(cv::mean(channel, mask)[0]);

    cv::Mat filled(channel.size(), CV_32F, cv::Scalar(knownMean));
    channel.copyTo(filled, mask);

    cv::Mat excluded;
    cv::bitwise_not(mask, excluded);

    for (int it = 0; it < 10; ++it)
    {
        cv::Mat blurred = boxBlur(filled, radius);
        blurred.copyTo(filled, excluded);
    }
    return boxBlur(filled, radius);
}

void BGEOperation::medianAndSigma(const cv::Mat &values, const cv::Mat &mask, float &median, float &sigma)
{
    std::vector<float> v;
    v.reserve(values.total());
    for (int y = 0; y < values.rows; ++y)
    {
        const float *row = values.ptr<float>(y);
        const uchar *mrow = mask.empty() ? nullptr : mask.ptr<uchar>(y);
        for (int x = 0; x < values.cols; ++x)
            if (!mrow || mrow[x])
                v.push_back(row[x]);
    }
    if (v.empty())
    {
        for (int y = 0; y < values.rows; ++y)
        {
            const float *row = values.ptr<float>(y);
            for (int x = 0; x < values.cols; ++x)
                v.push_back(row[x]);
        }
    }

    const size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    median = v[mid];

    std::vector<float> dev(v.size());
    for (size_t i = 0; i < v.size(); ++i)
        dev[i] = std::fabs(v[i] - median);
    std::nth_element(dev.begin(), dev.begin() + mid, dev.end());
    sigma = 1.4826f * dev[mid] + 1e-6f;
}

void BGEOperation::robustRange(const cv::Mat &small, float &lo, float &hi)
{
    std::vector<float> v;
    v.reserve(small.total());
    for (int y = 0; y < small.rows; ++y)
    {
        const float *row = small.ptr<float>(y);
        for (int x = 0; x < small.cols; ++x)
            v.push_back(row[x]);
    }
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    lo = v[static_cast<size_t>(0.005 * (n - 1))];
    hi = v[static_cast<size_t>(0.995 * (n - 1))];
}

cv::Mat BGEOperation::structureMask(const cv::Mat &residual, int radius, float threshold, float amount)
{
    cv::Mat det;
    cv::threshold(residual, det, threshold, 1.0, cv::THRESH_BINARY);
    if (cv::countNonZero(det) == 0)
        return cv::Mat::zeros(residual.size(), CV_8U);

    const int growRadius = std::max(1, static_cast<int>(std::lround(radius * (0.5 + amount))));
    cv::Mat grown = boxBlur(det, growRadius);

    const float cutoff = (1.0f - amount) * 0.5f + 1e-3f;
    cv::Mat mask;
    cv::threshold(grown, mask, cutoff, 255, cv::THRESH_BINARY);
    mask.convertTo(mask, CV_8U);
    return mask;
}

cv::Mat BGEOperation::processChannel(const cv::Mat &channel, double strength)
{
    const int w = channel.cols, h = channel.rows;

    // Work at a bounded, resolution-independent size — this is what keeps the fit
    // fast enough for a fast-response caller (e.g. an interactive curves preview),
    // regardless of how large the source frame is.
    const int f = std::clamp(static_cast<int>(std::lround(std::min(w, h) / 400.0)), 1, 8);
    cv::Mat small;
    if (f > 1 && w / f >= 2 && h / f >= 2)
        cv::resize(channel, small, cv::Size(w / f, h / f), 0, 0, cv::INTER_AREA);
    else
        small = channel.clone();

    const int sw = small.cols, sh = small.rows;
    if (sw < 2 || sh < 2)
        return channel.clone();

    float lo, hi;
    robustRange(small, lo, hi);
    if (hi - lo < 1e-6f)
        return channel.clone();

    // Normalize into a fixed working range so the thresholds below behave the same
    // regardless of the channel's own units/scale.
    cv::Mat norm = (small - lo) / (hi - lo);

    const int mind = std::min(sw, sh);
    // A smaller local radius (relative to the smoothness-only pass this used to
    // pair with) lets the model track the real gradient more precisely without
    // losing structure protection or smoothness — verified against a mature
    // reference implementation's background model on real data (see class
    // comment) to land on par with or ahead of it, on both nebula-heavy and
    // nebula-free frames.
    const int radius = std::max(1, static_cast<int>(std::lround(0.025 * mind)));
    const size_t n = static_cast<size_t>(sw) * sh;
    const size_t minKeep = std::max(static_cast<size_t>(0.02 * n), static_cast<size_t>(16));

    cv::Mat keep(small.size(), CV_8U, cv::Scalar(255));
    cv::Mat model = boxBlur(norm, radius);
    size_t prevKept = n;

    constexpr int kIterations = 20;
    constexpr float kHighK = 2.0f; // tighter upper bound: bright outliers are structure, reject them fast
    constexpr float kLowK = 4.0f;  // looser lower bound: dark noise dips are normal background variance

    for (int it = 0; it < kIterations; ++it)
    {
        cv::Mat residual = norm - model;
        float med, sigma;
        medianAndSigma(residual, keep, med, sigma);
        const float hiClip = med + kHighK * sigma;
        const float loClip = med - kLowK * sigma;

        cv::Mat newKeep;
        cv::inRange(residual, loClip, hiClip, newKeep);

        cv::Mat structMaskMat = structureMask(residual - med, radius, 0.05f, 0.5f);
        newKeep.setTo(0, structMaskMat);

        size_t kept = static_cast<size_t>(cv::countNonZero(newKeep));
        if (kept < minKeep)
        {
            // Never let the fit set collapse to nothing: fall back to keeping the
            // minKeep pixels with the smallest residual, however bad they are.
            std::vector<float> vals;
            vals.reserve(n);
            for (int y = 0; y < sh; ++y)
            {
                const float *row = residual.ptr<float>(y);
                for (int x = 0; x < sw; ++x)
                    vals.push_back(row[x]);
            }
            const size_t rank = std::min(minKeep, n - 1);
            std::nth_element(vals.begin(), vals.begin() + rank, vals.end());
            const float thr = vals[rank];
            cv::threshold(residual, newKeep, thr, 255, cv::THRESH_BINARY_INV);
            newKeep.convertTo(newKeep, CV_8U);
            kept = static_cast<size_t>(cv::countNonZero(newKeep));
        }

        model = inpaint(norm, newKeep, radius);

        const double change = std::fabs(static_cast<double>(kept) - static_cast<double>(prevKept)) / static_cast<double>(n);
        keep = newKeep;
        prevKept = kept;
        if (it > 0 && change < 1e-4)
            break;
    }

    // No extra smoothing pass here: the converged model is already a repeated
    // box-blur result (see inpaint()/boxBlur()), and an additional pass on top
    // measurably damped the correction's amplitude on real data without making
    // it any safer — smoothness now comes entirely from the radius above.
    cv::Mat modelDenorm = model * (hi - lo) + lo;
    cv::Mat bgFull;
    cv::resize(modelDenorm, bgFull, channel.size(), 0, 0, cv::INTER_LINEAR);

    float level, unusedSigma;
    medianAndSigma(bgFull, cv::Mat(), level, unusedSigma);

    cv::Mat corrected = channel - bgFull + level;
    return channel * (1.0 - strength) + corrected * strength;
}

bool BGEOperation::apply(cv::Mat &image, double strength, QString &error)
{
    if (image.empty())
    {
        error = QStringLiteral("No image to process");
        return false;
    }
    if (strength <= 0.0)
        return true;

    std::vector<cv::Mat> channels;
    cv::split(image, channels);
    for (auto &ch : channels)
        ch = processChannel(ch, strength);
    cv::merge(channels, image);
    return true;
}
