/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "histogrambuilder.h"

#include "autostretch.h"
#include "curveoperation.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>

void HistogramBuilder::binChannel(const cv::Mat &channel, int bins, float rangeMin, float invRange,
                                  std::vector<double> &counts)
{
    counts.assign(static_cast<size_t>(bins), 0.0);
    if (channel.empty() || channel.depth() != CV_32F || channel.channels() != 1)
        return;

    for (int y = 0; y < channel.rows; y++)
    {
        const float *row = channel.ptr<float>(y);
        for (int x = 0; x < channel.cols; x++)
        {
            float normalized = (row[x] - rangeMin) * invRange;
            // Written as explicit comparisons rather than std::clamp(): a NaN pixel (from a
            // degenerate upstream op) would slip through clamp's `v < lo` / `hi < v` tests
            // unchanged and then make the int cast below undefined.
            if (!(normalized > 0.0f))
                normalized = 0.0f;
            else if (normalized > 1.0f)
                normalized = 1.0f;

            int bin = static_cast<int>(normalized * bins);
            if (bin >= bins)
                bin = bins - 1;
            else if (bin < 0)
                bin = 0;

            counts[static_cast<size_t>(bin)] += 1.0;
        }
    }
}

bool HistogramBuilder::build(const cv::Mat &image, int bins,
                             bool haveInputRange, float inputMin, float inputMax,
                             bool projectAutoStretch, double targetBackground, double shadowsClipping,
                             bool linked, bool neutralizeBackground,
                             Result &result, QString &error)
{
    if (image.empty())
    {
        error = QStringLiteral("No image to build a histogram from");
        return false;
    }
    if (image.depth() != CV_32F)
    {
        error = QStringLiteral("HistogramBuilder expects a CV_32F image");
        return false;
    }
    if (image.channels() != 1 && image.channels() != 3)
    {
        error = QStringLiteral("HistogramBuilder expects a 1 or 3 channel image");
        return false;
    }
    if (bins < 2)
    {
        error = QStringLiteral("A histogram needs at least 2 bins");
        return false;
    }

    try
    {
        // Downscale first — a histogram only needs the distribution, not the resolution,
        // and this bounds the cost of everything after it regardless of the source. Same
        // 1024px convention as PreviewRenderer, deliberately: the histogram then reflects
        // the same data the accompanying JPEG preview was rendered from.
        cv::Mat small;
        const int longSide = std::max(image.cols, image.rows);
        if (longSide > kMaxDimension)
        {
            const double scale = static_cast<double>(kMaxDimension) / longSide;
            cv::resize(image, small, cv::Size(), scale, scale, cv::INTER_AREA);
        }
        else
            small = image.clone();

        // Project the autostretch the UI currently has configured. A curve that runs after
        // an autostretch is evaluated against the *post*-stretch data, so binning the
        // pre-stretch distribution here would put the user's curve on an axis it is never
        // evaluated on. Deliberately non-fatal: worst case the histogram is of
        // un-stretched data rather than the request failing outright, matching
        // PreviewRenderer's treatment of the same call.
        if (projectAutoStretch)
        {
            QString stretchError;
            AutoStretch::apply(small, stretchError, targetBackground, shadowsClipping, linked, neutralizeBackground);
        }

        // The axis the bins span. Resolution order mirrors FITSData::applyStretch()'s, so
        // the histogram shows exactly the range a curve was (or will be) applied over.
        float rangeMin = inputMin;
        float rangeMax = inputMax;
        if (!haveInputRange)
        {
            if (projectAutoStretch)
            {
                // AutoStretch::apply() normalizes to [0,1] by definition.
                rangeMin = 0.0f;
                rangeMax = 1.0f;
            }
            else
            {
                CurveOperation::deriveInputRange(small, rangeMin, rangeMax);
            }
        }
        if (!(rangeMax > rangeMin))
        {
            rangeMin = 0.0f;
            rangeMax = 1.0f;
        }

        const float invRange = 1.0f / (rangeMax - rangeMin);

        // Normalize each channel's bins against the largest bin so the UI can scale the
        // values straight into its plot without knowing the pixel count.
        const auto normalizeInto = [](const std::vector<double> &counts, QVector<float> &out)
        {
            double maxCount = 0.0;
            for (const double count : counts)
                maxCount = std::max(maxCount, count);

            out.clear();
            out.reserve(static_cast<int>(counts.size()));
            for (const double count : counts)
                out.append(maxCount > 0.0 ? static_cast<float>(count / maxCount) : 0.0f);
        };

        result.bins = bins;
        result.inputMin = rangeMin;
        result.inputMax = rangeMax;
        result.luminance.clear();
        result.red.clear();
        result.green.clear();
        result.blue.clear();

        std::vector<double> counts;

        if (small.channels() == 1)
        {
            binChannel(small, bins, rangeMin, invRange, counts);
            normalizeInto(counts, result.luminance);
        }
        else
        {
            std::vector<cv::Mat> channels;
            cv::split(small, channels);

            // Channel 0 is R throughout this pipeline (see CurveOperation::applyPerChannel,
            // whose per-channel curves arrive in R/G/B order), so the luma weights below are
            // the standard RGB ones.
            cv::Mat luma;
            cv::cvtColor(small, luma, cv::COLOR_RGB2GRAY);
            binChannel(luma, bins, rangeMin, invRange, counts);
            normalizeInto(counts, result.luminance);

            binChannel(channels[0], bins, rangeMin, invRange, counts);
            normalizeInto(counts, result.red);
            binChannel(channels[1], bins, rangeMin, invRange, counts);
            normalizeInto(counts, result.green);
            binChannel(channels[2], bins, rangeMin, invRange, counts);
            normalizeInto(counts, result.blue);
        }

        return true;
    }
    catch (const cv::Exception &ex)
    {
        error = QStringLiteral("Histogram build failed: %1").arg(ex.what());
        return false;
    }
}
