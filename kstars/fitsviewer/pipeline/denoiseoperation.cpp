/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "denoiseoperation.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>

float DenoiseOperation::robustSigma(const cv::Mat &d)
{
    // Row-strided VIEW (no copy, no averaging) to bound the sort below to a few
    // hundred thousand samples on a full-resolution image, while every sampled value
    // stays a genuine, unmodified pixel — averaging (e.g. a naive resize) would bias
    // the estimate low, since averaging independent noise samples reduces their
    // apparent amplitude.
    const int stride = std::max(1, d.rows / 750);
    const cv::Mat view(d.rows / stride, d.cols, d.type(), d.data, d.step[0] * stride);
    const cv::Mat absView = cv::abs(view);
    cv::Mat flat = absView.reshape(1, 1);
    cv::Mat sorted;
    cv::sort(flat, sorted, cv::SORT_ASCENDING);
    return 1.4826f * sorted.at<float>(0, sorted.cols / 2);
}

cv::Mat DenoiseOperation::luminanceDenoise(const cv::Mat &image, double amt, DenoiseMethod method)
{
    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    // Map the user-facing [0,1] strength to a significance multiplier in units of the
    // layer's own sigma, rather than an absolute value — this is what makes amt mean
    // the same thing regardless of the input's pixel-value scale (raw ADU counts, a
    // normalized [0,1] image, an 8-bit preview, ...).
    const float kSigma = 0.5f + (float)amt * 4.5f; // ~0.5 sigma (mild) .. 5 sigma (aggressive)
    const bool soft = (method == DenoiseMethod::SOFT);

    for (auto &ch : channels)
    {
        CV_Assert(ch.type() == CV_32F);

        cv::Mat low1, low2, low3;
        cv::GaussianBlur(ch, low1, cv::Size(3, 3), 0.8);
        cv::GaussianBlur(low1, low2, cv::Size(5, 5), 1.6);
        cv::GaussianBlur(low2, low3, cv::Size(9, 9), 3.2);

        cv::Mat d1 = ch - low1;
        cv::Mat d2 = low1 - low2;
        cv::Mat d3 = low2 - low3;

        const float t1 = kSigma * robustSigma(d1);
        const float t2 = kSigma * robustSigma(d2);

        cv::Mat d1_shrink, d2_shrink;
        if (soft)
        {
            // Donoho-style shrinkage: sign(x)*max(|x|-t, 0), computed without an
            // explicit sign extraction via the identity
            // soft(x,t) = max(x-t, 0) - max(-x-t, 0). Every coefficient above
            // threshold is pulled toward zero by t rather than passed through
            // unchanged, which avoids hard thresholding's blotchy, hard-edged look.
            cv::Mat pos1, neg1, pos2, neg2;
            cv::max(d1 - t1, 0.0f, pos1);
            cv::max(-d1 - t1, 0.0f, neg1);
            d1_shrink = pos1 - neg1;

            cv::max(d2 - t2, 0.0f, pos2);
            cv::max(-d2 - t2, 0.0f, neg2);
            d2_shrink = pos2 - neg2;
        }
        else
        {
            // Hard threshold: binary keep-above/zero-below.
            cv::Mat mask1, mask2;
            cv::compare(cv::abs(d1), t1, mask1, cv::CmpTypes::CMP_GT);
            cv::compare(cv::abs(d2), t2, mask2, cv::CmpTypes::CMP_GT);

            // cv::Mat::copyTo(mask) only writes pixels where the mask is non-zero — it
            // does NOT zero the rest of a freshly allocated destination, it leaves
            // them as uninitialized memory. Below-threshold coefficients must become
            // exactly zero, so d1_shrink/d2_shrink must be explicitly zeroed first, or
            // most of the image ends up mixed with garbage instead of being denoised.
            d1_shrink = cv::Mat::zeros(d1.size(), d1.type());
            d2_shrink = cv::Mat::zeros(d2.size(), d2.type());
            d1.copyTo(d1_shrink, mask1);
            d2.copyTo(d2_shrink, mask2);
        }

        ch = low3 + d3 + d2_shrink + d1_shrink;
    }

    cv::Mat result;
    cv::merge(channels, result);
    return result;
}

cv::Mat DenoiseOperation::chromaDenoise(const cv::Mat &image, double amt)
{
    std::vector<cv::Mat> bgr;
    cv::split(image, bgr);
    const cv::Mat &B = bgr[0], &G = bgr[1], &R = bgr[2];

    // Luma/color-difference decomposition, deliberately not cv::COLOR_BGR2YCrCb:
    // OpenCV's YCrCb conversion bakes in a fixed midpoint offset (0.5 for float input)
    // that only round-trips correctly for [0,1]-normalized data, but this can run on
    // the pipeline's native linear image, which can be at raw ADU scale (tens of
    // thousands). Cr = R-Y / Cb = B-Y needs no offset at all, so it stays exact at any
    // scale.
    cv::Mat Y = 0.114f * B + 0.587f * G + 0.299f * R;
    cv::Mat Cr = R - Y;
    cv::Mat Cb = B - Y;

    // Protect bright/saturated pixels (star cores) from bleeding their own chroma
    // into the blur below. Clipping breaks the R:G:B ratio non-uniformly per channel
    // (independently-stacked filters saturate at different signal levels), leaving an
    // extreme, unrepresentative Cr/Cb exactly at the clipped core. A plain Gaussian
    // blur spreads that extreme value outward; since newG below is reconstructed as a
    // *residual* rather than blurred directly, the spread shows up as a dark ring in
    // luminance around every bright star once amt is large enough for the blur radius
    // to matter (confirmed on real data: invisible at low chromaDenoiseAmt, a clear
    // dark ring at chromaDenoiseAmt 1.0). Replacing each bright pixel's chroma with a
    // value inpainted from its non-bright neighborhood — before the main blur — keeps
    // a saturated core's own chroma from ever being available to spread.
    // A fraction of the single frame-wide maximum is the wrong threshold here: one
    // exceptionally bright star (or hot pixel) sets the bar so high that every other
    // genuinely bright star — which triggers the exact same clipping-mismatch
    // mechanism — falls under it and stays unprotected (confirmed on real data: fixed
    // the one star tested against, left a whole field of other bright stars still
    // ringed). Use a robust, percentile-based cutoff instead, so it scales with how
    // many bright sources are actually in the frame rather than with its single
    // brightest pixel. Sampled the same strided way as robustSigma() above, for the
    // same reason: bound the sort to a few hundred thousand samples on a full-
    // resolution image without biasing the estimate via averaging.
    const int strideY = std::max(1, Y.rows / 750);
    const cv::Mat yView(Y.rows / strideY, Y.cols, Y.type(), Y.data, Y.step[0] * strideY);
    // .reshape() requires a continuous Mat, which the strided view above isn't —
    // .clone() first (robustSigma() above gets this for free since cv::abs() happens
    // to always return a continuous copy of its input).
    cv::Mat yFlat = yView.clone().reshape(1, 1);
    cv::Mat ySorted;
    cv::sort(yFlat, ySorted, cv::SORT_ASCENDING);
    const int p995 = static_cast<int>(ySorted.cols * 0.995);
    const float brightThreshold = ySorted.at<float>(0, std::min(p995, ySorted.cols - 1));

    cv::Mat brightMask;
    cv::compare(Y, brightThreshold, brightMask, cv::CMP_GT);
    if (cv::countNonZero(brightMask) > 0)
    {
        constexpr double kFillSigma = 15.0;
        auto inpaintBright = [&](cv::Mat & plane)
        {
            cv::Mat filled = plane.clone();
            filled.setTo(0.0f, brightMask);
            cv::Mat weight = cv::Mat::ones(plane.size(), CV_32F);
            weight.setTo(0.0f, brightMask);
            cv::GaussianBlur(filled, filled, cv::Size(0, 0), kFillSigma);
            cv::GaussianBlur(weight, weight, cv::Size(0, 0), kFillSigma);
            cv::Mat replacement = filled / cv::max(weight, 1e-6f);
            replacement.copyTo(plane, brightMask);
        };
        inpaintBright(Cr);
        inpaintBright(Cb);
    }

    // [0,1] strength -> a 1-8px Gaussian blur radius on the chroma planes only.
    const double sigma = 1.0 + std::clamp(amt, 0.0, 1.0) * 7.0;
    cv::GaussianBlur(Cr, Cr, cv::Size(0, 0), sigma);
    cv::GaussianBlur(Cb, Cb, cv::Size(0, 0), sigma);

    cv::Mat newR = Cr + Y;
    cv::Mat newB = Cb + Y;
    cv::Mat newG = (Y - 0.299f * newR - 0.114f * newB) / 0.587f;

    cv::Mat result;
    std::vector<cv::Mat> outChannels { newB, newG, newR };
    cv::merge(outChannels, result);
    return result;
}

bool DenoiseOperation::apply(cv::Mat &image, double amt, DenoiseMethod method, double chromaAmt, QString &error)
{
    if (image.empty())
    {
        error = QStringLiteral("No image to denoise");
        return false;
    }
    if (image.depth() != CV_32F)
    {
        error = QStringLiteral("DenoiseOperation expects a CV_32F image");
        return false;
    }

    try
    {
        if (amt > 0.0)
            image = luminanceDenoise(image, amt, method);

        if (chromaAmt > 0.0 && image.channels() == 3)
            image = chromaDenoise(image, chromaAmt);

        return true;
    }
    catch (const cv::Exception &ex)
    {
        error = QString("OpenCV exception in DenoiseOperation::apply: %1").arg(ex.what());
        return false;
    }
}
