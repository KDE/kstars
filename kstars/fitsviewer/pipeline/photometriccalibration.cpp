/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "photometriccalibration.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

std::vector<PhotometricCalibrationOperation::DetectedStar> PhotometricCalibrationOperation::detectStars(
    const cv::Mat &image, QString &error)
{
    std::vector<DetectedStar> stars;

    if (image.empty() || image.depth() != CV_32F || image.channels() != 3)
    {
        error = QStringLiteral("PhotometricCalibrationOperation::detectStars expects a CV_32FC3 image");
        return stars;
    }

    // Channel order is R,G,B here (see ChannelBlendOperation::blendRGB()'s
    // cv::merge({r,g,b},...) and SaturationOperation's cv::COLOR_RGB2HSV) — NOT the
    // B,G,R DenoiseOperation labels its split() result with, which only happens to be
    // harmless there because that transform is used internally/losslessly regardless
    // of which physical channel it calls "B" vs "R". This operation needs the real
    // channel identities to match a target color to the right physical channel.
    std::vector<cv::Mat> rgb;
    cv::split(image, rgb);
    const cv::Mat lum = 0.299f * rgb[0] + 0.587f * rgb[1] + 0.114f * rgb[2];

    // Percentile threshold via the same strided-sampling technique as
    // DenoiseOperation::robustSigma() — bounds the sort to a few hundred thousand
    // samples on a full-resolution image without biasing the estimate via averaging.
    const int stride = std::max(1, lum.rows / 750);
    const cv::Mat view(lum.rows / stride, lum.cols, lum.type(), lum.data, lum.step[0] * stride);
    cv::Mat flat = view.clone().reshape(1, 1);
    cv::Mat sorted;
    cv::sort(flat, sorted, cv::SORT_ASCENDING);
    // 99.5th, not 99.9th: the tighter threshold only caught the few hundred brightest
    // blobs in a real field, leaving the vast majority of visible stars (which a real
    // field packs in by the thousands) never even considered for correction — most of
    // what a viewer sees stayed at whatever chroma bias the upstream channel-combine/
    // denoise/saturation steps left it with. Confirmed on real data: raising coverage
    // this way roughly triples the detected count.
    const int p995 = static_cast<int>(sorted.cols * 0.995);
    const float threshold = sorted.at<float>(0, std::min(p995, sorted.cols - 1));

    cv::Mat mask;
    cv::compare(lum, threshold, mask, cv::CMP_GT);

    cv::Mat labels, stats, centroids;
    const int numLabels = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);

    for (int i = 1; i < numLabels; i++) // label 0 is the background
    {
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area < 3) // a single hot pixel or two — not a real star blob
            continue;

        const int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
        const int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
        const double aspect = static_cast<double>(std::max(w, h)) / std::max(1, std::min(w, h));
        if (aspect > 3.0) // elongated — a diffraction spike or nebula knot, not a star
            continue;

        DetectedStar star;
        star.pixel = cv::Point2f(static_cast<float>(centroids.at<double>(i, 0)),
                                 static_cast<float>(centroids.at<double>(i, 1)));
        star.radiusPx = std::sqrt(static_cast<float>(area) / static_cast<float>(M_PI));
        stars.push_back(star);
    }

    return stars;
}

bool PhotometricCalibrationOperation::apply(cv::Mat &image, const std::vector<StarMatch> &matches, double strength,
        QString &error)
{
    if (image.empty() || image.depth() != CV_32F || image.channels() != 3)
    {
        error = QStringLiteral("PhotometricCalibrationOperation::apply expects a CV_32FC3 image");
        return false;
    }

    const double clampedStrength = std::clamp(strength, 0.0, 1.0);
    if (clampedStrength <= 0.0 || matches.empty())
        return true; // no-op

    // Channel order is R,G,B (see detectStars()'s comment above for why this matters
    // and where that's established) — index 0 is Red, not Blue.
    std::vector<cv::Mat> rgb;
    cv::split(image, rgb);
    // Read from a frozen copy of the original channels throughout — measurements and
    // the final blend below must all see the same pre-correction data, since a real
    // field packs enough stars that many correction radii overlap; each star's
    // measured color must reflect what was actually there, not an already-corrected
    // neighbor's result.
    const cv::Mat originalR = rgb[0].clone(), originalG = rgb[1].clone(), originalB = rgb[2].clone();

    // Accumulate every star's (weight, weight*(gain-1)) into these instead of
    // multiplying the image in place per star: applying each star's correction
    // sequentially would let overlapping radii compound multiplicatively (a pixel
    // touched by two stars gets gain1*gain2, not a blend of the two) — in a rich
    // field where correction disks routinely overlap, that produced a visibly wrong,
    // strongly color-shifted result on real data, confirmed against the actual saved
    // FITS bytes. Accumulating first and applying once at the end blends overlapping
    // corrections as a weight-capped average instead.
    cv::Mat sumWeight = cv::Mat::zeros(image.size(), CV_32F);
    cv::Mat sumWeightedGainR = cv::Mat::zeros(image.size(), CV_32F);
    cv::Mat sumWeightedGainG = cv::Mat::zeros(image.size(), CV_32F);
    cv::Mat sumWeightedGainB = cv::Mat::zeros(image.size(), CV_32F);

    const cv::Rect bounds(0, 0, image.cols, image.rows);

    // A saturated/clipped star's core reads as flat, artificially-neutral white (every
    // channel pinned at the sensor's ceiling) regardless of its true color — clipping
    // has already destroyed the very information a color correction needs. Computing
    // a gain from that flat measurement and spreading it across a correction radius
    // that also scales up with the star's (falsely bloated, because it's clipped)
    // apparent size produced a real, visible artifact on real data: a wide, fake
    // orange glow around exactly the brightest stars. Per-channel ceilings, computed
    // once, let each match be checked against "is this pixel actually near this
    // channel's max" before being trusted.
    double ceilingR, ceilingG, ceilingB;
    cv::minMaxLoc(originalR, nullptr, &ceilingR);
    cv::minMaxLoc(originalG, nullptr, &ceilingG);
    cv::minMaxLoc(originalB, nullptr, &ceilingB);

    for (const auto &match : matches)
    {
        // Detect a saturated core: sample the peak (not the aperture mean, which
        // would dilute a small clipped core with unclipped surrounding pixels) at the
        // star's exact center. If every channel there sits within 3% of its own
        // frame-wide ceiling, the core is clipped and its true catalog-implied color
        // is unrecoverable from it — but that's not a reason to leave the star
        // untouched. These are typically the biggest, brightest, most visually
        // prominent stars in the frame (confirmed on real data: this is exactly what
        // was still showing the pre-existing chroma bias after correction, since
        // skipping them outright meant the most eye-catching stars in the image were
        // the only ones never pulled toward neutral). Fall back to a neutral target
        // for these, same reasoning as an unmatched star with no catalog color at
        // all: an unknown true color is better rendered as neutral gray than left
        // with whatever bias the upstream pipeline gave it.
        const int cx = std::clamp(static_cast<int>(std::round(match.pixel.x)), 0, image.cols - 1);
        const int cy = std::clamp(static_cast<int>(std::round(match.pixel.y)), 0, image.rows - 1);
        const float peakR = originalR.at<float>(cy, cx);
        const float peakG = originalG.at<float>(cy, cx);
        const float peakB = originalB.at<float>(cy, cx);
        constexpr double kClipTolerance = 0.03;
        const bool saturated = peakR >= ceilingR * (1.0 - kClipTolerance)
                               && peakG >= ceilingG * (1.0 - kClipTolerance)
                               && peakB >= ceilingB * (1.0 - kClipTolerance);
        constexpr float neutral = 1.0f / 3.0f;
        const float effectiveTargetR = saturated ? neutral : match.targetR;
        const float effectiveTargetG = saturated ? neutral : match.targetG;
        const float effectiveTargetB = saturated ? neutral : match.targetB;

        // Measured color: the mean over a small aperture at the star's own core,
        // robust to a slightly-off centroid (the detected blob's centroid, not a
        // re-fit PSF peak).
        const int apertureRadius = std::max(1, static_cast<int>(std::round(match.radiusPx * 0.6)));
        const cv::Rect apertureRect(static_cast<int>(match.pixel.x) - apertureRadius,
                                    static_cast<int>(match.pixel.y) - apertureRadius,
                                    2 * apertureRadius + 1, 2 * apertureRadius + 1);
        const cv::Rect clippedAperture = apertureRect & bounds;
        if (clippedAperture.width <= 0 || clippedAperture.height <= 0)
            continue;

        const double measuredB = cv::mean(originalB(clippedAperture))[0];
        const double measuredG = cv::mean(originalG(clippedAperture))[0];
        const double measuredR = cv::mean(originalR(clippedAperture))[0];
        const double measuredSum = measuredR + measuredG + measuredB;
        if (measuredSum <= 0.0)
            continue;

        // Per-channel gain: target/measured normalized ratio, clamped against a bad
        // catalog match or a noisy/near-zero measurement blowing the correction up.
        const double gainR = std::clamp(effectiveTargetR / (measuredR / measuredSum), 0.3, 3.0);
        const double gainG = std::clamp(effectiveTargetG / (measuredG / measuredSum), 0.3, 3.0);
        const double gainB = std::clamp(effectiveTargetB / (measuredB / measuredSum), 0.3, 3.0);

        // Gaussian-weighted radial falloff centered on the star — full correction at
        // the core, tapering to a no-op (gain 1.0) a couple of radii out, so only the
        // star and its immediate halo are touched.
        // Capped defense-in-depth against a borderline-but-not-quite-clipped bright
        // star's blob detection reporting a bloated apparent radius (PSF wings/bloom
        // near saturation inflate the detected area) — without this, such a star
        // could still spread its correction across an oversized, visually obtrusive
        // halo even though its core passed the saturation check above.
        const double fallOffSigma = std::max(1.5, std::min(match.radiusPx, 8.0f) * 1.5);
        const int patchRadius = static_cast<int>(std::ceil(fallOffSigma * 3.0));
        const cv::Rect patchRect(static_cast<int>(match.pixel.x) - patchRadius,
                                 static_cast<int>(match.pixel.y) - patchRadius,
                                 2 * patchRadius + 1, 2 * patchRadius + 1);
        const cv::Rect clippedPatch = patchRect & bounds;
        if (clippedPatch.width <= 0 || clippedPatch.height <= 0)
            continue;

        const cv::Mat origRPatch = originalR(clippedPatch);
        const cv::Mat origGPatch = originalG(clippedPatch);
        const cv::Mat origBPatch = originalB(clippedPatch);

        // The Gaussian falloff above only bounds *how far* a correction can reach —
        // it says nothing about *what's actually there*. patchRadius is typically
        // several times the aperture that produced the gain (e.g. a 47x47 patch for
        // a 7x7 measurement) — that outer zone is the star's faint PSF wings and
        // surrounding background/nebula, not what was measured, and applying a
        // core-derived gain there produced a real, visible artifact on real data: a
        // colored ring around many stars. Gate each pixel's weight by how much of it
        // is actually still starlight, using a *local* background/noise floor (not a
        // flat fraction of peak, which fails outright for a star sitting on
        // non-trivial nebula background — the background can already be a large
        // fraction of the peak there) sampled from this same patch's own outer
        // annulus, so it reflects what's actually under this star rather than a
        // whole-frame average.
        const double innerAnnulus2 = std::pow(patchRadius * 0.8, 2);
        std::vector<float> annulusLum;
        annulusLum.reserve(static_cast<size_t>(clippedPatch.width) * clippedPatch.height / 3);
        for (int y = 0; y < origRPatch.rows; y++)
        {
            const float *rRow = origRPatch.ptr<float>(y);
            const float *gRow = origGPatch.ptr<float>(y);
            const float *bRow = origBPatch.ptr<float>(y);
            const double dy = (clippedPatch.y + y) - match.pixel.y;
            for (int x = 0; x < origRPatch.cols; x++)
            {
                const double dx = (clippedPatch.x + x) - match.pixel.x;
                if (dx * dx + dy * dy >= innerAnnulus2)
                    annulusLum.push_back(0.299f * rRow[x] + 0.587f * gRow[x] + 0.114f * bRow[x]);
            }
        }

        // Too few annulus samples (patch clipped hard against an image edge) —
        // degrade gracefully to no brightness gating for this one star rather than
        // trust a near-empty sample.
        double localFloor = 0.0;
        if (annulusLum.size() >= 8)
        {
            const size_t mid = annulusLum.size() / 2;
            std::nth_element(annulusLum.begin(), annulusLum.begin() + mid, annulusLum.end());
            const double median = annulusLum[mid];
            std::vector<float> absDev(annulusLum.size());
            for (size_t i = 0; i < annulusLum.size(); i++)
                absDev[i] = std::abs(annulusLum[i] - static_cast<float>(median));
            std::nth_element(absDev.begin(), absDev.begin() + mid, absDev.end());
            // 1.4826x converts MAD to a sigma-equivalent for Gaussian noise (the same
            // "robust sigma" convention this codebase already uses elsewhere, e.g.
            // DenoiseOperation::robustSigma()) — a 1.5-sigma margin above the local
            // background median so ordinary noise fluctuations don't get partial
            // weight just from sitting on the positive side of the median.
            localFloor = median + 1.5 * 1.4826 * absDev[mid];
        }

        const double peakLum = 0.299 * peakR + 0.587 * peakG + 0.114 * peakB;
        if (peakLum <= localFloor)
            continue; // doesn't clear its own local background — not a usable match

        cv::Mat weightPatch = sumWeight(clippedPatch);
        cv::Mat wgRPatch = sumWeightedGainR(clippedPatch);
        cv::Mat wgGPatch = sumWeightedGainG(clippedPatch);
        cv::Mat wgBPatch = sumWeightedGainB(clippedPatch);

        for (int y = 0; y < weightPatch.rows; y++)
        {
            float *wRow = weightPatch.ptr<float>(y);
            float *wgRRow = wgRPatch.ptr<float>(y);
            float *wgGRow = wgGPatch.ptr<float>(y);
            float *wgBRow = wgBPatch.ptr<float>(y);
            const float *rRow = origRPatch.ptr<float>(y);
            const float *gRow = origGPatch.ptr<float>(y);
            const float *bRow = origBPatch.ptr<float>(y);
            const double dy = (clippedPatch.y + y) - match.pixel.y;
            for (int x = 0; x < weightPatch.cols; x++)
            {
                const double dx = (clippedPatch.x + x) - match.pixel.x;
                const double dist2 = dx * dx + dy * dy;
                const double lum = 0.299 * rRow[x] + 0.587 * gRow[x] + 0.114 * bRow[x];
                const double brightnessWeight = std::clamp((lum - localFloor) / std::max(peakLum - localFloor, 1e-6),
                                                0.0, 1.0);
                const double weight = clampedStrength
                                      * std::exp(-dist2 / (2.0 * fallOffSigma * fallOffSigma))
                                      * brightnessWeight;
                wRow[x] += static_cast<float>(weight);
                wgRRow[x] += static_cast<float>(weight * (gainR - 1.0));
                wgGRow[x] += static_cast<float>(weight * (gainG - 1.0));
                wgBRow[x] += static_cast<float>(weight * (gainB - 1.0));
            }
        }
    }

    // Single final pass: blend every pixel's accumulated corrections against its
    // original value. Dividing by max(sumWeight, 1.0) rather than sumWeight itself
    // means one star at full weight (1.0) applies its own gain exactly, while a pixel
    // touched by several overlapping stars gets a proportionally damped average
    // instead of an unbounded pileup.
    for (int y = 0; y < image.rows; y++)
    {
        const float *wRow = sumWeight.ptr<float>(y);
        const float *wgRRow = sumWeightedGainR.ptr<float>(y);
        const float *wgGRow = sumWeightedGainG.ptr<float>(y);
        const float *wgBRow = sumWeightedGainB.ptr<float>(y);
        const float *origRRow = originalR.ptr<float>(y);
        const float *origGRow = originalG.ptr<float>(y);
        const float *origBRow = originalB.ptr<float>(y);
        float *outRRow = rgb[0].ptr<float>(y);
        float *outGRow = rgb[1].ptr<float>(y);
        float *outBRow = rgb[2].ptr<float>(y);

        for (int x = 0; x < image.cols; x++)
        {
            if (wRow[x] <= 0.0f)
                continue;
            const double denom = std::max(static_cast<double>(wRow[x]), 1.0);
            outRRow[x] = static_cast<float>(origRRow[x] * (1.0 + wgRRow[x] / denom));
            outGRow[x] = static_cast<float>(origGRow[x] * (1.0 + wgGRow[x] / denom));
            outBRow[x] = static_cast<float>(origBRow[x] * (1.0 + wgBRow[x] / denom));
        }
    }

    cv::merge(rgb, image);
    return true;
}

namespace
{

// Real CIE 1931 2-degree standard observer color-matching functions, 380-780nm at
// 5nm spacing — sourced from the colour-science project's published dataset
// (colour/colorimetry/datasets/cmfs.py, itself citing the standard CIE tables), not
// recalled from memory: an earlier version of this function modeled R/G/B as
// independent, non-overlapping ~100nm bands, which produced visibly oversaturated,
// cartoonish star colors (confirmed against a real reference image) because it
// ignored how much real overlap exists between the eye's (and any real camera's)
// color channels. Integrating a blackbody spectrum against these real,
// standardized, heavily-overlapping response curves is what actually reproduces the
// pale, mostly-white tints real astrophotos show, without any arbitrary damping
// factor needed on top.
struct CIEEntry
{
    int wavelengthNm;
    float xBar, yBar, zBar;
};

constexpr CIEEntry kCIE1931_5nm[] =
{
    {380, 0.001368f, 0.000039f, 0.006450f},
    {385, 0.002236f, 0.000064f, 0.010550f},
    {390, 0.004243f, 0.000120f, 0.020050f},
    {395, 0.007650f, 0.000217f, 0.036210f},
    {400, 0.014310f, 0.000396f, 0.067850f},
    {405, 0.023190f, 0.000640f, 0.110200f},
    {410, 0.043510f, 0.001210f, 0.207400f},
    {415, 0.077630f, 0.002180f, 0.371300f},
    {420, 0.134380f, 0.004000f, 0.645600f},
    {425, 0.214770f, 0.007300f, 1.039050f},
    {430, 0.283900f, 0.011600f, 1.385600f},
    {435, 0.328500f, 0.016840f, 1.622960f},
    {440, 0.348280f, 0.023000f, 1.747060f},
    {445, 0.348060f, 0.029800f, 1.782600f},
    {450, 0.336200f, 0.038000f, 1.772110f},
    {455, 0.318700f, 0.048000f, 1.744100f},
    {460, 0.290800f, 0.060000f, 1.669200f},
    {465, 0.251100f, 0.073900f, 1.528100f},
    {470, 0.195360f, 0.090980f, 1.287640f},
    {475, 0.142100f, 0.112600f, 1.041900f},
    {480, 0.095640f, 0.139020f, 0.812950f},
    {485, 0.057950f, 0.169300f, 0.616200f},
    {490, 0.032010f, 0.208020f, 0.465180f},
    {495, 0.014700f, 0.258600f, 0.353300f},
    {500, 0.004900f, 0.323000f, 0.272000f},
    {505, 0.002400f, 0.407300f, 0.212300f},
    {510, 0.009300f, 0.503000f, 0.158200f},
    {515, 0.029100f, 0.608200f, 0.111700f},
    {520, 0.063270f, 0.710000f, 0.078250f},
    {525, 0.109600f, 0.793200f, 0.057250f},
    {530, 0.165500f, 0.862000f, 0.042160f},
    {535, 0.225750f, 0.914850f, 0.029840f},
    {540, 0.290400f, 0.954000f, 0.020300f},
    {545, 0.359700f, 0.980300f, 0.013400f},
    {550, 0.433450f, 0.994950f, 0.008750f},
    {555, 0.512050f, 1.000000f, 0.005750f},
    {560, 0.594500f, 0.995000f, 0.003900f},
    {565, 0.678400f, 0.978600f, 0.002750f},
    {570, 0.762100f, 0.952000f, 0.002100f},
    {575, 0.842500f, 0.915400f, 0.001800f},
    {580, 0.916300f, 0.870000f, 0.001650f},
    {585, 0.978600f, 0.816300f, 0.001400f},
    {590, 1.026300f, 0.757000f, 0.001100f},
    {595, 1.056700f, 0.694900f, 0.001000f},
    {600, 1.062200f, 0.631000f, 0.000800f},
    {605, 1.045600f, 0.566800f, 0.000600f},
    {610, 1.002600f, 0.503000f, 0.000340f},
    {615, 0.938400f, 0.441200f, 0.000240f},
    {620, 0.854450f, 0.381000f, 0.000190f},
    {625, 0.751400f, 0.321000f, 0.000100f},
    {630, 0.642400f, 0.265000f, 0.000050f},
    {635, 0.541900f, 0.217000f, 0.000030f},
    {640, 0.447900f, 0.175000f, 0.000020f},
    {645, 0.360800f, 0.138200f, 0.000010f},
    {650, 0.283500f, 0.107000f, 0.000000f},
    {655, 0.218700f, 0.081600f, 0.000000f},
    {660, 0.164900f, 0.061000f, 0.000000f},
    {665, 0.121200f, 0.044580f, 0.000000f},
    {670, 0.087400f, 0.032000f, 0.000000f},
    {675, 0.063600f, 0.023200f, 0.000000f},
    {680, 0.046770f, 0.017000f, 0.000000f},
    {685, 0.032900f, 0.011920f, 0.000000f},
    {690, 0.022700f, 0.008210f, 0.000000f},
    {695, 0.015840f, 0.005723f, 0.000000f},
    {700, 0.011359f, 0.004102f, 0.000000f},
    {705, 0.008111f, 0.002929f, 0.000000f},
    {710, 0.005790f, 0.002091f, 0.000000f},
    {715, 0.004109f, 0.001484f, 0.000000f},
    {720, 0.002899f, 0.001047f, 0.000000f},
    {725, 0.002049f, 0.000740f, 0.000000f},
    {730, 0.001440f, 0.000520f, 0.000000f},
    {735, 0.001000f, 0.000361f, 0.000000f},
    {740, 0.000690f, 0.000249f, 0.000000f},
    {745, 0.000476f, 0.000172f, 0.000000f},
    {750, 0.000332f, 0.000120f, 0.000000f},
    {755, 0.000235f, 0.000085f, 0.000000f},
    {760, 0.000166f, 0.000060f, 0.000000f},
    {765, 0.000117f, 0.000042f, 0.000000f},
    {770, 0.000083f, 0.000030f, 0.000000f},
    {775, 0.000059f, 0.000021f, 0.000000f},
    {780, 0.000042f, 0.000015f, 0.000000f},
};

} // namespace

void PhotometricCalibrationOperation::colorFromBVIndex(float bvIndex, float &r, float &g, float &b)
{
    // Ballesteros' formula: effective temperature from B-V color index. Clamped to a
    // sane range so pathological/erroneous B-V data (nothing in a real catalog should
    // be outside this) can't drive a denominator to zero or negative.
    const double bv = std::clamp(static_cast<double>(bvIndex), -0.4, 2.0);
    const double temperatureK = 4600.0 * (1.0 / (0.92 * bv + 1.7) + 1.0 / (0.92 * bv + 0.62));

    constexpr double h = 6.62607015e-34; // Planck constant, J*s
    constexpr double c = 2.99792458e8;   // speed of light, m/s
    constexpr double kB = 1.380649e-23;  // Boltzmann constant, J/K

    auto planckRadiance = [&](double wavelengthMeters)
    {
        const double exponent = (h * c) / (wavelengthMeters * kB * temperatureK);
        return (2.0 * h * c * c) / (std::pow(wavelengthMeters, 5) * (std::exp(exponent) - 1.0));
    };

    // Integrate the blackbody's spectral radiance against the real CIE color-matching
    // functions (a simple Riemann sum is enough at 5nm spacing over a smoothly-varying
    // integrand; the constant step size cancels out once R+G+B is normalized to 1
    // below, so it's omitted).
    double X = 0.0, Y = 0.0, Z = 0.0;
    for (const auto &entry : kCIE1931_5nm)
    {
        const double radiance = planckRadiance(entry.wavelengthNm * 1e-9);
        X += radiance * entry.xBar;
        Y += radiance * entry.yBar;
        Z += radiance * entry.zBar;
    }

    // Standard CIE XYZ -> linear sRGB (D65) matrix (IEC 61966-2-1), verified against
    // the same colour-science source as the CMF table above.
    double red   =  3.2406 * X - 1.5372 * Y - 0.4986 * Z;
    double green = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
    double blue  =  0.0557 * X - 0.2040 * Y + 1.0570 * Z;

    // A blackbody spectrum integrated this way can land slightly outside the sRGB
    // gamut (a negative component) at temperature extremes; clip to non-negative
    // before normalizing rather than let a negative throw the R+G+B=1 normalization
    // off.
    red = std::max(red, 0.0);
    green = std::max(green, 0.0);
    blue = std::max(blue, 0.0);

    const double sum = red + green + blue;
    r = static_cast<float>(red / sum);
    g = static_cast<float>(green / sum);
    b = static_cast<float>(blue / sum);
}
