/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVector>
#include <opencv2/core/core.hpp>

#include <functional>

struct wcsprm;

/**
 * @class ChannelBlendOperation
 * @brief Weighted linear combination of N independently-stacked mono images into an
 * RGB result — arbitrary narrowband palettes (HOO/SHO/bicolor/anything else), not just
 * a fixed one-filter-per-slot assignment.
 *
 * The engine's own R/G/B/L channel assignment (`FITSData::initStackChannels()`) is
 * positional and 1:1 — each input directory becomes exactly one output channel, no
 * blending, and it rejects anything other than 1, 3, or 4 directories (so a 2-filter
 * narrowband set, e.g. Ha+OIII, can't be submitted as a single multi-channel stack at
 * all). This is a separate, later step: stack each filter independently as its own
 * mono session first (trivially supported today — that's just `n==1`), then combine the
 * finished, already-stacked results here with arbitrary per-channel weights, e.g.
 * green = 0.7×OIII + 0.3×Ha for a custom HOO blend. No engine changes needed — this
 * operates purely on already-produced `cv::Mat` results.
 */
class ChannelBlendOperation
{
    public:
        // Reported after each registration/blend step of blendRGB() — current/total
        // count every per-channel-input registration pass plus the final per-channel
        // blend (3), and label is a short human-readable description, e.g.
        // "Registered green channel input 2/3" or "Blended red channel".
        using ProgressCallback = std::function<void(int current, int total, const QString &label)>;
        // Polled before each step — return true to abandon the blend as soon as
        // possible. blendRGB() then fails with outCancelled set (rather than error),
        // same convention as MasterBuilder::build().
        using CancelCallback = std::function<bool()>;

        struct WeightedInput
        {
            cv::Mat image;   // CV_32F, single channel — one already-stacked mono session
            double weight;
            // WCS this session was plate-solved to, if any (nullptr if it wasn't, e.g.
            // alignMethod NONE). Two independently-stacked sessions have no guarantee of
            // sharing the same pixel grid even at identical dimensions — each is only
            // self-consistently aligned to its own align master. When at least one input
            // across the whole blend carries a WCS, blendRGB() uses it to register every
            // other WCS-carrying input onto that same grid before summing; inputs with no
            // WCS are blended as-is (assumed already on a shared grid, the old behavior).
            const struct wcsprm *wcs = nullptr;
            // Robust sky-background level for this input, filled in by blendRGB() when
            // normalization is enabled (see robustBackground()). blendChannel() then
            // subtracts background*weight from the channel sum, so inputs whose sky levels
            // differ — routine for independently-stacked narrowband filters, where both the
            // sky brightness and the filter throughput differ — combine to a neutral
            // background instead of carrying their level ratio through as a colour cast.
            // Defaults to 0.0, which makes the subtraction a no-op (a literal weighted sum,
            // the pre-normalization behavior) for any caller that leaves it unset.
            double background = 0.0;
        };

        /**
         * @brief Compute one output channel as the weighted sum of its inputs.
         * @param inputs one or more single-channel CV_32F images, all the same size,
         * each with a weight (weights don't need to sum to 1 — e.g. weight 1.0 on a
         * single input is a plain passthrough, matching a traditional unweighted
         * R/G/B/L assignment)
         * @param outChannel receives the weighted sum, CV_32F, single channel
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        static bool blendChannel(const QVector<WeightedInput> &inputs, cv::Mat &outChannel, QString &error);

        /**
         * @brief Compute all three output channels and merge into one RGB image.
         * @param red / green / blue weighted inputs for each output channel (see
         * blendChannel()) — every input across all three must be the same size
         * @param outImage receives the merged CV_32FC3 result
         * @param outRefWcs receives the WCS every input was registered onto (the first
         * non-null WCS found scanning red, then green, then blue), or nullptr if none
         * of the inputs carried one. The caller does not own this pointer — it aliases
         * whichever input's `wcs` field was chosen, so it's only valid as long as that
         * input's WCS is; a caller that wants to keep it (e.g. to adopt the blended
         * result as a new session) must deep-copy it itself.
         * @param error receives a human-readable failure reason on failure
         * @param normalize when true (default), each named input is level-matched by its
         * own robust sky background before the weighted sum — so inputs whose sky levels
         * differ combine to a neutral background instead of carrying their level ratio as
         * a colour cast. Set false for the raw literal weighted sum.
         * @param onProgress optional — see ProgressCallback.
         * @param isCancelled optional — see CancelCallback.
         * @param outCancelled when non-null, set to true if the blend stopped because
         * isCancelled() returned true (vs. a genuine failure, reported via error).
         * @return success
         */
        static bool blendRGB(const QVector<WeightedInput> &red, const QVector<WeightedInput> &green,
                             const QVector<WeightedInput> &blue, cv::Mat &outImage,
                             const struct wcsprm * &outRefWcs, QString &error, bool normalize = true,
                             const ProgressCallback &onProgress = ProgressCallback(),
                             const CancelCallback &isCancelled = CancelCallback(), bool *outCancelled = nullptr);

    private:
        // Warps `image` (in place) from its own WCS (`imageWcs`) onto `refWcs`'s pixel
        // grid, by resampling through the two solutions directly: every destination
        // pixel is mapped ref pixel -> world (refWcs) -> source pixel (imageWcs) and the
        // result fed to cv::remap. That composite mapping is exact, so it carries each
        // solution's own distortion (wcslib applies SIP for us) and any shear in either
        // CD matrix.
        //
        // Deliberately NOT the fitted rigid/similarity transform FITSStack::calcWarpMatrix()
        // uses to align a sub to its align master. That works there because all the subs
        // in a stack share one optical train and one distortion model, so they really do
        // differ by a rigid transform. It does not hold here: these are separately
        // plate-solved masters, each carrying its own independently-fitted distortion
        // polynomial, and the two routinely disagree by several px toward the frame
        // border. A 4-DOF similarity cannot absorb that — it rejects good input, and
        // where it does pass it leaves the corners misregistered, which in a narrowband
        // blend shows up as colour fringing toward the edges.
        //
        // A no-op if imageWcs/refWcs are null or identical (already on the reference
        // grid). Returns false only on a genuine failure — a broken wcsprm, or a WCS
        // that is real-looking but wrong (too little overlap with the reference field,
        // or too large a center displacement).
        static bool registerToReference(cv::Mat &image, const struct wcsprm *imageWcs, const struct wcsprm *refWcs,
                                        QString &error);

        // Robust sky-background estimate for one single-channel CV_32F input: a low (25th)
        // percentile over a strided ~200k-pixel sample. Deliberately not the median — real
        // signal (stars, nebulosity) would bias a median up — and deliberately strided
        // rather than a full-resolution sort, so its cost is independent of frame size.
        // Used by blendRGB() to level-match inputs before the weighted sum.
        static float robustBackground(const cv::Mat &image);
};
