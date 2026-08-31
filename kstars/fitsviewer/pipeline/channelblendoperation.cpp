/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "channelblendoperation.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <wcs.h>

#include <cmath>
#include <vector>

bool ChannelBlendOperation::registerToReference(cv::Mat &image, const struct wcsprm *imageWcs,
        const struct wcsprm *refWcs, QString &error)
{
    if (!imageWcs || !refWcs || imageWcs == refWcs)
        return true; // nothing to do — no WCS to register with, or already the reference

    const int width = image.cols;
    const int height = image.rows;
    constexpr int gridSize = 5;
    std::vector<cv::Point2d> refPoints, imagePoints;

    for (int i = 0; i < gridSize; i++)
    {
        for (int j = 0; j < gridSize; j++)
        {
            const double px = static_cast<double>(i) * (width - 1.0) / (gridSize - 1);
            const double py = static_cast<double>(j) * (height - 1.0) / (gridSize - 1);

            double imgcrd[2], phi, theta, world[2], pixcrd[2];
            int stat[2];

            // Pixel (ref grid) -> World, using the reference session's own WCS
            double refPixFits[2] = { px + 1.0, py + 1.0 };
            if (wcsp2s(const_cast<struct wcsprm *>(refWcs), 1, 2, refPixFits, imgcrd, &phi, &theta, world, stat) != 0)
                continue;

            // World -> Pixel (this image), using this session's own WCS
            if (wcss2p(const_cast<struct wcsprm *>(imageWcs), 1, 2, world, &phi, &theta, imgcrd, pixcrd, stat) != 0)
                continue;

            refPoints.push_back(cv::Point2d(px, py));
            imagePoints.push_back(cv::Point2d(pixcrd[0] - 1.0, pixcrd[1] - 1.0));
        }
    }

    const unsigned int minPoints = (gridSize * gridSize / 2) + 1;
    if (refPoints.size() < minPoints)
    {
        error = QString("Not enough overlapping WCS points [%1] to register channel").arg(refPoints.size());
        return false;
    }

    cv::Mat inliers;
    cv::Mat affine = cv::estimateAffinePartial2D(imagePoints, refPoints, inliers, cv::RANSAC, 1.0);
    if (affine.empty())
    {
        error = QStringLiteral("Could not compute a registration transform between channels");
        return false;
    }

    const double inlierRatio = static_cast<double>(cv::countNonZero(inliers)) / refPoints.size();
    if (inlierRatio < 0.8)
    {
        error = QStringLiteral("Channels do not form a consistent rigid registration");
        return false;
    }

    // Same center-displacement check as calcWarpMatrix() — raw affine translation is
    // relative to the pixel-corner origin, so any rotation about the frame center
    // (plate-solved images commonly differ by a few degrees of sky rotation between
    // sessions) inflates it hugely even for a perfectly good registration.
    const double cx = width / 2.0;
    const double cy = height / 2.0;
    const double cxWarped = affine.at<double>(0, 0) * cx + affine.at<double>(0, 1) * cy + affine.at<double>(0, 2);
    const double cyWarped = affine.at<double>(1, 0) * cx + affine.at<double>(1, 1) * cy + affine.at<double>(1, 2);
    const double centerDisplacement = std::sqrt((cxWarped - cx) * (cxWarped - cx) + (cyWarped - cy) * (cyWarped - cy));
    const double maxCenterDisplacement = std::min(width, height) * 0.5;
    if (centerDisplacement > maxCenterDisplacement)
    {
        error = QString("Channel center displacement too large (%1 px, limit %2 px)")
                .arg(centerDisplacement).arg(maxCenterDisplacement);
        return false;
    }

    cv::Mat warped;
    cv::warpAffine(image, warped, affine, image.size(), cv::INTER_LINEAR);
    image = warped;
    return true;
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
    }
    return true;
}

bool ChannelBlendOperation::blendRGB(const QVector<WeightedInput> &red, const QVector<WeightedInput> &green,
                                     const QVector<WeightedInput> &blue, cv::Mat &outImage,
                                     const struct wcsprm *&outRefWcs, QString &error)
{
    // Register every WCS-carrying input onto a common reference grid before blending —
    // each was only independently plate-solved to its own align master, so two
    // channels sharing pixel dimensions is no guarantee they share a pixel grid. Pick
    // the first WCS encountered (red, then green, then blue) as the reference; inputs
    // with no WCS at all (alignMethod NONE) are left as-is, matching prior behavior.
    const struct wcsprm *refWcs = nullptr;
    for (const auto *inputs : { &red, &green, &blue })
    {
        for (const auto &input : *inputs)
        {
            if (input.wcs) { refWcs = input.wcs; break; }
        }
        if (refWcs) break;
    }
    outRefWcs = refWcs;

    QVector<WeightedInput> redReg = red, greenReg = green, blueReg = blue;
    if (refWcs)
    {
        for (auto *inputs : { &redReg, &greenReg, &blueReg })
        {
            for (auto &input : *inputs)
            {
                QString regError;
                if (!registerToReference(input.image, input.wcs, refWcs, regError))
                {
                    error = QString("Cross-channel registration failed: %1").arg(regError);
                    return false;
                }
            }
        }
    }

    cv::Mat r, g, b;
    if (!blendChannel(redReg, r, error) || !blendChannel(greenReg, g, error) || !blendChannel(blueReg, b, error))
        return false;

    if (r.size() != g.size() || g.size() != b.size())
    {
        error = QStringLiteral("Red/green/blue channels must all be the same size");
        return false;
    }

    std::vector<cv::Mat> channels { r, g, b };
    cv::merge(channels, outImage);
    return true;
}
