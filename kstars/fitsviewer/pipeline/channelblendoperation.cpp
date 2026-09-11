/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "channelblendoperation.h"

#include <opencv2/imgproc.hpp>
#include <wcs.h>

#include <algorithm>
#include <cmath>
#include <vector>

bool ChannelBlendOperation::registerToReference(cv::Mat &image, const struct wcsprm *imageWcs,
        const struct wcsprm *refWcs, QString &error)
{
    if (!imageWcs || !refWcs || imageWcs == refWcs)
        return true; // nothing to do — no WCS to register with, or already the reference

    const int width = image.cols;
    const int height = image.rows;

    // Map every destination (reference-grid) pixel back to its source pixel in `image`
    // by going through the sky: ref pixel -> world (refWcs) -> image pixel (imageWcs).
    // That composite mapping is exactly what the two solutions say it is — including
    // each one's own SIP distortion, which wcslib applies for us, and any shear in
    // either CD matrix. Deliberately NOT approximated by a fitted rigid/similarity
    // transform first: two independently plate-solved masters routinely disagree by a
    // few px toward the frame border (each solve fits its own distortion polynomial to
    // its own noise), which no 4-DOF similarity can absorb. Fitting one would both
    // reject good input and, where it did pass, leave the corners misregistered —
    // visible as colour fringing toward the edges of a narrowband blend.
    cv::Mat map(height, width, CV_32FC2);

    // Chunked so the wcslib scratch buffers stay a few MB rather than scaling with the
    // frame (a full 3000x2000 pass at once would need ~400MB of doubles).
    constexpr int targetChunkPoints = 1 << 18;
    const int rowsPerChunk = std::max(1, std::min(height, targetChunkPoints / std::max(1, width)));
    const size_t maxPoints = static_cast<size_t>(rowsPerChunk) * width;

    std::vector<double> pixcrd(2 * maxPoints), imgcrd(2 * maxPoints), world(2 * maxPoints);
    std::vector<double> phi(maxPoints), theta(maxPoints);
    std::vector<int> stat(maxPoints);

    size_t insideCount = 0;
    for (int y0 = 0; y0 < height; y0 += rowsPerChunk)
    {
        const int rows = std::min(rowsPerChunk, height - y0);
        const int nPoints = rows * width;

        for (int r = 0; r < rows; r++)
        {
            for (int x = 0; x < width; x++)
            {
                const size_t i = static_cast<size_t>(r) * width + x;
                pixcrd[2 * i]     = x + 1.0;      // wcslib is 1-based
                pixcrd[2 * i + 1] = y0 + r + 1.0;
            }
        }

        // WCSERR_BAD_PIX / WCSERR_BAD_WORLD are per-point and reported via stat[] — a
        // frame corner that projects off the sphere is normal and just leaves that
        // pixel unmapped. Any other status is a broken wcsprm, which is fatal.
        int status = wcsp2s(const_cast<struct wcsprm *>(refWcs), nPoints, 2, pixcrd.data(), imgcrd.data(),
                            phi.data(), theta.data(), world.data(), stat.data());
        if (status != WCSERR_SUCCESS && status != WCSERR_BAD_PIX)
        {
            error = QString("Reference WCS could not be evaluated (wcsp2s error %1: %2)")
                    .arg(status).arg(wcs_errmsg[status]);
            return false;
        }
        const bool refHadBadPoints = (status == WCSERR_BAD_PIX);
        std::vector<int> refStat;
        if (refHadBadPoints)
            refStat.assign(stat.begin(), stat.begin() + nPoints);

        status = wcss2p(const_cast<struct wcsprm *>(imageWcs), nPoints, 2, world.data(), phi.data(), theta.data(),
                        imgcrd.data(), pixcrd.data(), stat.data());
        if (status != WCSERR_SUCCESS && status != WCSERR_BAD_WORLD)
        {
            error = QString("Channel WCS could not be evaluated (wcss2p error %1: %2)")
                    .arg(status).arg(wcs_errmsg[status]);
            return false;
        }

        for (int r = 0; r < rows; r++)
        {
            auto *row = map.ptr<cv::Vec2f>(y0 + r);
            for (int x = 0; x < width; x++)
            {
                const size_t i = static_cast<size_t>(r) * width + x;
                if (stat[i] != 0 || (refHadBadPoints && refStat[i] != 0))
                {
                    // Unmappable — steer remap outside the source so it fills the
                    // border value rather than sampling an arbitrary pixel.
                    row[x] = cv::Vec2f(-1.0f, -1.0f);
                    continue;
                }
                const float sx = static_cast<float>(pixcrd[2 * i] - 1.0);
                const float sy = static_cast<float>(pixcrd[2 * i + 1] - 1.0);
                row[x] = cv::Vec2f(sx, sy);
                if (sx >= 0.0f && sy >= 0.0f && sx <= width - 1.0f && sy <= height - 1.0f)
                    insideCount++;
            }
        }
    }

    // The rigid-fit consistency test is gone (the mapping is exact, so there is nothing
    // to be inconsistent with), but the two sanity checks it also provided are not: a
    // WCS that is real-looking yet wrong still has to be caught rather than silently
    // warping the channel into nonsense.
    const double overlap = static_cast<double>(insideCount) / (static_cast<double>(width) * height);
    constexpr double minOverlap = 0.5;
    if (overlap < minOverlap)
    {
        error = QString("Channel does not overlap the reference frame (only %1% of the reference grid maps "
                        "into this channel, need %2%) — check that both images are solved to the same field")
                .arg(overlap * 100.0, 0, 'f', 1).arg(minOverlap * 100.0, 0, 'g', 2);
        return false;
    }

    const int cx = width / 2;
    const int cy = height / 2;
    const cv::Vec2f center = map.at<cv::Vec2f>(cy, cx);
    const double centerDisplacement = std::sqrt((center[0] - cx) * (center[0] - cx)
                                     + (center[1] - cy) * (center[1] - cy));
    const double maxCenterDisplacement = std::min(width, height) * 0.5;
    if (center[0] < 0.0f || centerDisplacement > maxCenterDisplacement)
    {
        error = QString("Channel center displacement too large (%1 px, limit %2 px)")
                .arg(centerDisplacement).arg(maxCenterDisplacement);
        return false;
    }

    // Lanczos-4 rather than bilinear: better preserves stellar PSF shape and avoids
    // softening compared to a plain bilinear resample — worth the extra cost here
    // given registration only runs once per blend input, not per-sub.
    cv::Mat warped;
    cv::remap(image, warped, map, cv::noArray(), cv::INTER_LANCZOS4, cv::BORDER_CONSTANT, cv::Scalar(0));
    image = warped;
    return true;
}

float ChannelBlendOperation::robustBackground(const cv::Mat &image)
{
    if (image.empty() || image.channels() != 1 || image.depth() != CV_32F)
        return 0.0f;

    // Strided sample to a few hundred thousand pixels: enough for a stable low
    // percentile, independent of frame resolution, and cheap even on a full-size frame
    // (no full-resolution sort). A low percentile rather than the median so real signal
    // (stars, nebulosity) can't bias the sky level upward.
    constexpr size_t targetSamples = 200000;
    const int step = std::max(1, static_cast<int>(std::lround(
                                  std::sqrt(static_cast<double>(image.total()) / targetSamples))));

    std::vector<float> samples;
    samples.reserve(targetSamples + 1);
    for (int y = 0; y < image.rows; y += step)
    {
        const float *row = image.ptr<float>(y);
        for (int x = 0; x < image.cols; x += step)
            samples.push_back(row[x]);
    }
    if (samples.empty())
        return 0.0f;

    const size_t k = samples.size() / 4; // 25th percentile
    std::nth_element(samples.begin(), samples.begin() + k, samples.end());
    return std::max(0.0f, samples[k]);
}

bool ChannelBlendOperation::blendChannel(const QVector<WeightedInput> &inputs, cv::Mat &outChannel, QString &error)
{
    if (inputs.isEmpty())
    {
        error = QStringLiteral("A channel needs at least one weighted input");
        return false;
    }

    const cv::Mat &first = inputs.front().image;
    if (first.empty())
    {
        error = QStringLiteral("Empty input image");
        return false;
    }
    if (first.depth() != CV_32F || first.channels() != 1)
    {
        error = QStringLiteral("ChannelBlendOperation expects single-channel CV_32F inputs "
                               "(one already-stacked mono session per input, not a combined RGB result)");
        return false;
    }

    outChannel = cv::Mat::zeros(first.size(), CV_32F);
    for (const auto &input : inputs)
    {
        if (input.image.empty() || input.image.size() != first.size() || input.image.channels() != 1
                || input.image.depth() != CV_32F)
        {
            error = QStringLiteral("All inputs to a blended channel must be the same size and a single-channel "
                                   "CV_32F image");
            return false;
        }
        outChannel += input.image * static_cast<float>(input.weight);
        // Normalization (see WeightedInput::background): subtract this input's own sky
        // level, scaled by its weight, so inputs at different levels combine to a neutral
        // background. A scalar subtract — no extra full-frame temporary. background
        // defaults to 0.0, making this a no-op for an un-normalized caller.
        if (input.background != 0.0)
            outChannel -= static_cast<float>(input.background * input.weight);
    }
    return true;
}

bool ChannelBlendOperation::blendRGB(const QVector<WeightedInput> &red, const QVector<WeightedInput> &green,
                                     const QVector<WeightedInput> &blue, cv::Mat &outImage,
                                     const struct wcsprm * &outRefWcs, QString &error, bool normalize,
                                     const ProgressCallback &onProgress, const CancelCallback &isCancelled,
                                     bool *outCancelled)
{
    if (outCancelled)
        *outCancelled = false;
    auto cancelled = [&]()
    {
        if (!isCancelled || !isCancelled())
            return false;
        error = QStringLiteral("Blend cancelled");
        if (outCancelled)
            *outCancelled = true;
        return true;
    };

    // Register every WCS-carrying input onto a common reference grid before blending —
    // each was only independently plate-solved to its own align master, so two
    // channels sharing pixel dimensions is no guarantee they share a pixel grid. Pick
    // the first WCS encountered (red, then green, then blue) as the reference; inputs
    // with no WCS at all (alignMethod NONE) are left as-is, matching prior behavior.
    const struct wcsprm *refWcs = nullptr;
    // Named so a registration failure can say what everything was being registered
    // *onto* — which input became the reference is otherwise invisible to the caller,
    // and it matters: if the reference is the input with the bad WCS, every other
    // channel fails against it and the first one reported is not the culprit.
    QString refLabel;
    const struct
    {
        const QVector<WeightedInput> *inputs;
        const char *name;
    } scan[] = { { &red, "red" }, { &green, "green" }, { &blue, "blue" } };
    for (const auto &group : scan)
    {
        for (int i = 0; i < group.inputs->size(); i++)
        {
            if (group.inputs->at(i).wcs)
            {
                refWcs = group.inputs->at(i).wcs;
                refLabel = QString("%1 input %2/%3").arg(group.name).arg(i + 1).arg(group.inputs->size());
                break;
            }
        }
        if (refWcs) break;
    }
    outRefWcs = refWcs;

    QVector<WeightedInput> redReg = red, greenReg = green, blueReg = blue;

    // Level-match the inputs before combining (see WeightedInput::background). Estimated
    // on each input's *original* image, before registerToReference() warps it onto the
    // reference grid — a warp pads the non-overlapping border with 0, which would drag a
    // low-percentile estimate toward zero for a partially-overlapping channel. Cheap: a
    // strided ~200k-sample percentile per input, no full-frame copy.
    if (normalize)
    {
        for (auto *group : { &redReg, &greenReg, &blueReg })
            for (auto &input : *group)
                input.background = robustBackground(input.image);
    }

    // Every registration pass plus the final per-channel blend (3) — see
    // ProgressCallback's doc comment.
    const int registrationTotal = refWcs ? (red.size() + green.size() + blue.size()) : 0;
    const int total = registrationTotal + 3;
    int current = 0;

    if (refWcs)
    {
        struct ChannelGroup
        {
            QVector<WeightedInput> *inputs;
            const char *name;
        };
        for (auto &group :
                {
                    ChannelGroup{&redReg, "red"}, ChannelGroup{&greenReg, "green"}, ChannelGroup{&blueReg, "blue"}
                })
        {
            for (int i = 0; i < group.inputs->size(); i++)
            {
                if (cancelled())
                    return false;

                QString regError;
                if (!registerToReference((*group.inputs)[i].image, (*group.inputs)[i].wcs, refWcs, regError))
                {
                    error = QString("Cross-channel registration failed for %1 input %2/%3 (reference: %4): %5")
                            .arg(group.name).arg(i + 1).arg(group.inputs->size()).arg(refLabel).arg(regError);
                    return false;
                }
                current++;
                if (onProgress)
                    onProgress(current, total,
                              QString("Registered %1 channel input %2/%3").arg(group.name).arg(i + 1).arg(group.inputs->size()));
            }
        }
    }

    if (cancelled())
        return false;
    cv::Mat r, g, b;
    if (!blendChannel(redReg, r, error))
        return false;
    current++;
    if (onProgress)
        onProgress(current, total, QStringLiteral("Blended red channel"));

    if (cancelled())
        return false;
    if (!blendChannel(greenReg, g, error))
        return false;
    current++;
    if (onProgress)
        onProgress(current, total, QStringLiteral("Blended green channel"));

    if (cancelled())
        return false;
    if (!blendChannel(blueReg, b, error))
        return false;
    current++;
    if (onProgress)
        onProgress(current, total, QStringLiteral("Blended blue channel"));

    if (r.size() != g.size() || g.size() != b.size())
    {
        error = QStringLiteral("Red/green/blue channels must all be the same size");
        return false;
    }

    std::vector<cv::Mat> channels { r, g, b };
    cv::merge(channels, outImage);
    return true;
}
