/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imageanalysis.h"

#include "fitsviewer/fitsdata.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace Ekos
{

QString imageVerdictKey(ImageVerdict verdict)
{
    switch (verdict)
    {
        case ImageVerdict::Good:
            return QStringLiteral("good");
        case ImageVerdict::FewStars:
            return QStringLiteral("few_stars");
        case ImageVerdict::OutOfFocus:
            return QStringLiteral("out_of_focus");
        case ImageVerdict::Trailed:
            return QStringLiteral("trailed");
        case ImageVerdict::DoubleImages:
            return QStringLiteral("double_images");
        case ImageVerdict::Unknown:
            break;
    }
    return QStringLiteral("unknown");
}

namespace ImageAnalysis
{

namespace
{
/// Smallest connected area above the detection threshold that counts as a star, in pixels
constexpr int MIN_STAR_AREA = 5;
/// Detection threshold, in standard deviations of the noise of the smoothed frame
constexpr double DETECTION_SIGMA = 5;
/// Width of the cells of the background map, pixels
constexpr int BACKGROUND_CELL = 64;
/// Brightest peaks searched for a twin, and the peaks they may pair with
constexpr int DOUBLE_PEAKS = 100;
constexpr int DOUBLE_NEIGHBOURS = 300;
/// The light between two images of a doubled star drops below this share of the fainter one. A trail has no
/// such dip between the noise peaks along it.
constexpr double DOUBLE_SADDLE = 0.7;
/// Closest twin, pixels: nearer peaks are the same star
constexpr double MIN_DOUBLE_OFFSET = 3;
/// Largest aperture radius of a star, pixels
constexpr double MAX_APERTURE = 100;

double median(std::vector<double> values)
{
    if (values.empty())
        return -1;
    const auto middle = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}

/** The value below which half the total weight lies. */
double weightedMedian(const std::vector<double> &values, const std::vector<double> &weights)
{
    if (values.empty())
        return -1;
    std::vector<std::pair<double, double>> pairs;
    double total = 0;
    for (size_t i = 0; i < values.size(); i++)
    {
        pairs.emplace_back(values[i], weights[i]);
        total += weights[i];
    }
    std::sort(pairs.begin(), pairs.end());
    double sum = 0;
    for (const auto &pair : pairs)
    {
        sum += pair.second;
        if (sum >= total / 2)
            return pair.first;
    }
    return pairs.back().first;
}

/** Standard deviation of a frame from the median absolute deviation, which ignores the stars. */
double robustSigma(const cv::Mat &image)
{
    std::vector<double> samples;
    const int step = std::max(1, static_cast<int>(image.total() / 200000));
    samples.reserve(image.total() / step + 1);
    for (int y = 0; y < image.rows; y++)
    {
        const float *row = image.ptr<float>(y);
        for (int x = (y * image.cols) % step; x < image.cols; x += step)
            samples.push_back(row[x]);
    }
    const double middle = median(samples);
    for (auto &sample : samples)
        sample = std::abs(sample - middle);
    return 1.4826 * median(samples);
}

template <typename T>
void addChannel(const uint8_t *channel, int width, const QRect &region, cv::Mat &out)
{
    const T *pixels = reinterpret_cast<const T *>(channel);
    for (int y = 0; y < out.rows; y++)
    {
        const T *row = pixels + static_cast<size_t>(region.y() + y) * width + region.x();
        float *target = out.ptr<float>(y);
        for (int x = 0; x < out.cols; x++)
            target[x] += static_cast<float>(row[x]);
    }
}

/** The region of the frame as one float channel, the mean of its colour channels. */
cv::Mat regionToFloat(const FITSData &data, const QRect &region)
{
    const auto &statistics = data.getStatistics();
    const uint8_t *buffer = data.getImageBuffer();
    if (!buffer)
        return cv::Mat();
    const int channels = std::max(1, static_cast<int>(statistics.channels));
    const size_t plane = static_cast<size_t>(statistics.samples_per_channel) * statistics.bytesPerPixel;
    cv::Mat out = cv::Mat::zeros(region.height(), region.width(), CV_32F);
    for (int c = 0; c < channels; c++)
    {
        const uint8_t *channel = buffer + c * plane;
        switch (statistics.dataType)
        {
            case TBYTE:
                addChannel<uint8_t>(channel, statistics.width, region, out);
                break;
            case TSHORT:
                addChannel<int16_t>(channel, statistics.width, region, out);
                break;
            case TUSHORT:
                addChannel<uint16_t>(channel, statistics.width, region, out);
                break;
            case TLONG:
                addChannel<int32_t>(channel, statistics.width, region, out);
                break;
            case TULONG:
                addChannel<uint32_t>(channel, statistics.width, region, out);
                break;
            case TFLOAT:
                addChannel<float>(channel, statistics.width, region, out);
                break;
            case TLONGLONG:
                addChannel<int64_t>(channel, statistics.width, region, out);
                break;
            case TDOUBLE:
                addChannel<double>(channel, statistics.width, region, out);
                break;
            default:
                return cv::Mat();
        }
    }
    if (channels > 1)
        out /= channels;
    return out;
}

/** The profile of one star. */
struct Profile
{
    double hfr {0};
    double concentration {0};
    double centreDip {1};
    double a {0};
    double b {0};
    double theta {0};   ///< radians
};

/**
 * Measure the profile of the star near (cx, cy) from every pixel within the aperture, against the median of an
 * annulus around it. No detection threshold: the faint wings count, so that faint focused stars keep their peaked
 * profile. The image must extend at least aperture + 10 pixels around the star.
 * @return false for a hot pixel or cosmic ray, or no light
 */
bool measureProfile(const cv::Mat &image, double cx, double cy, double aperture, Profile &profile)
{
    // Local background
    std::vector<double> annulus;
    const int outer = static_cast<int>(std::ceil(aperture + 9));
    for (int dy = -outer; dy <= outer; dy++)
        for (int dx = -outer; dx <= outer; dx++)
        {
            const double r = std::hypot(dx, dy);
            if (r >= aperture + 3 && r <= aperture + 9)
                annulus.push_back(image.at<float>(static_cast<int>(cy) + dy, static_cast<int>(cx) + dx));
        }
    const double background = median(annulus);
    const int reach = static_cast<int>(std::ceil(aperture));

    // Centroid of the light above the background
    for (int iteration = 0; iteration < 3; iteration++)
    {
        double sum = 0, sumX = 0, sumY = 0;
        const int x0 = static_cast<int>(std::lround(cx)), y0 = static_cast<int>(std::lround(cy));
        for (int dy = -reach; dy <= reach; dy++)
            for (int dx = -reach; dx <= reach; dx++)
            {
                const int x = x0 + dx, y = y0 + dy;
                if (std::hypot(x - cx, y - cy) > aperture)
                    continue;
                const double weight = std::max(0.0, image.at<float>(y, x) - background);
                sum += weight;
                sumX += weight * x;
                sumY += weight * y;
            }
        if (sum <= 0)
            return false;
        cx = sumX / sum;
        cy = sumY / sum;
    }

    // Growth curve on subpixels: the light of each pixel spread evenly over it. Small stars need finer steps.
    const int subpixels = std::clamp(static_cast<int>(40 / aperture), 1, 4);
    std::vector<std::pair<double, double>> samples;
    const int x0 = static_cast<int>(std::lround(cx)), y0 = static_cast<int>(std::lround(cy));
    for (int dy = -reach - 1; dy <= reach + 1; dy++)
        for (int dx = -reach - 1; dx <= reach + 1; dx++)
        {
            const int x = x0 + dx, y = y0 + dy;
            const double value = (image.at<float>(y, x) - background) / (subpixels * subpixels);
            for (int sy = 0; sy < subpixels; sy++)
                for (int sx = 0; sx < subpixels; sx++)
                {
                    const double px = x - 0.5 + (sx + 0.5) / subpixels, py = y - 0.5 + (sy + 0.5) / subpixels;
                    const double r = std::hypot(px - cx, py - cy);
                    if (r <= aperture)
                        samples.emplace_back(r, value);
                }
        }
    std::sort(samples.begin(), samples.end());
    double total = 0;
    for (const auto &sample : samples)
        total += sample.second;
    if (total <= 0)
        return false;
    auto radius = [&](double share)
    {
        const double target = share * total;
        double sum = 0;
        for (size_t i = 0; i < samples.size(); i++)
        {
            const double next = sum + samples[i].second;
            if (next >= target)
            {
                const double previous = i ? samples[i - 1].first : 0;
                return previous + (samples[i].first - previous) * (target - sum) / std::max(samples[i].second, 1e-12);
            }
            sum = next;
        }
        return aperture;
    };
    const double r20 = radius(0.2), r50 = radius(0.5), r80 = radius(0.8);
    // A hot pixel or a cosmic ray, not a star
    if (r20 <= 0 || r50 < 0.7)
        return false;

    // Donut: surface brightness of the centre against the brightest ring
    double centre = 0;
    int centreSamples = 0;
    std::map<int, std::pair<double, int>> rings;
    for (const auto &sample : samples)
    {
        if (sample.first < std::max(0.35, 0.3 * r50))
        {
            centre += sample.second;
            centreSamples++;
        }
        else if (sample.first <= 1.5 * r50)
        {
            auto &ring = rings[static_cast<int>(sample.first / 0.5)];
            ring.first += sample.second;
            ring.second++;
        }
    }
    double brightestRing = 0;
    for (const auto &ring : rings)
        brightestRing = std::max(brightestRing, ring.second.first / ring.second.second);
    const double dip = (centreSamples > 0 && brightestRing > 0) ? std::min(1.5,
                       (centre / centreSamples) / brightestRing) : 1.0;

    // Second moments of every pixel of the star, not only those above a threshold
    double sum = 0, m20 = 0, m02 = 0, m11 = 0;
    const double momentRadius = std::min(aperture, std::max(2.0, 2 * r50));
    for (int dy = -reach; dy <= reach; dy++)
        for (int dx = -reach; dx <= reach; dx++)
        {
            const int x = x0 + dx, y = y0 + dy;
            const double ddx = x - cx, ddy = y - cy;
            if (std::hypot(ddx, ddy) > momentRadius)
                continue;
            const double weight = std::max(0.0, image.at<float>(y, x) - background);
            sum += weight;
            m20 += weight * ddx * ddx;
            m02 += weight * ddy * ddy;
            m11 += weight * ddx * ddy;
        }
    if (sum <= 0)
        return false;
    m20 /= sum;
    m02 /= sum;
    m11 /= sum;
    const double trace = m20 + m02, determinant = m20 * m02 - m11 * m11;
    const double spread = std::sqrt(std::max(0.0, trace * trace / 4 - determinant));

    profile.hfr = r50;
    profile.concentration = r80 / r20;
    profile.centreDip = dip;
    profile.a = std::sqrt(trace / 2 + spread);
    profile.b = std::sqrt(std::max(1e-6, trace / 2 - spread));
    profile.theta = 0.5 * std::atan2(2 * m11, m20 - m02);
    return true;
}

struct Peak
{
    int x {0};
    int y {0};
    float value {0};
};

/**
 * Double images: the brightest peaks each have a twin at the same offset, with a dip in the light between them.
 * Sets the offset and the fraction of the brightest peaks that share it.
 */
void findDoubles(const cv::Mat &detection, const cv::Mat &mask, double maxDistance, ImageStats &stats)
{
    cv::Mat dilated;
    cv::dilate(detection, dilated, cv::Mat::ones(5, 5, CV_8U));
    std::vector<Peak> peaks;
    for (int y = 0; y < detection.rows; y++)
    {
        const float *row = detection.ptr<float>(y);
        const float *maximum = dilated.ptr<float>(y);
        const uchar *inside = mask.ptr<uchar>(y);
        for (int x = 0; x < detection.cols; x++)
            if (inside[x] && row[x] >= maximum[x])
                peaks.push_back({x, y, row[x]});
    }
    dilated.release();
    std::sort(peaks.begin(), peaks.end(), [](const Peak & a, const Peak & b)
    {
        return a.value > b.value;
    });
    const int subjects = std::min<int>(peaks.size(), DOUBLE_PEAKS);
    const int neighbours = std::min<int>(peaks.size(), DOUBLE_NEIGHBOURS);
    if (subjects < 2 * MIN_STARS)
        return;

    // Lowest light on the line between two peaks
    auto saddle = [&detection](const Peak & a, const Peak & b)
    {
        const double distance = std::hypot(b.x - a.x, b.y - a.y);
        const int steps = std::max(2, static_cast<int>(std::ceil(distance)));
        float lowest = std::min(a.value, b.value);
        for (int i = 1; i < steps; i++)
        {
            const double t = static_cast<double>(i) / steps;
            const int x = static_cast<int>(std::lround(a.x + t * (b.x - a.x)));
            const int y = static_cast<int>(std::lround(a.y + t * (b.y - a.y)));
            lowest = std::min(lowest, detection.at<float>(y, x));
        }
        return lowest;
    };

    // Offset from each bright peak to its nearest separate neighbour, folded into one half plane so that the two
    // images of a doubled star give the same offset
    std::vector<cv::Point2d> offsets;
    for (int i = 0; i < subjects; i++)
    {
        int nearest = -1;
        double best = maxDistance;
        for (int j = 0; j < neighbours; j++)
        {
            const double distance = std::hypot(peaks[j].x - peaks[i].x, peaks[j].y - peaks[i].y);
            if (j != i && distance >= MIN_DOUBLE_OFFSET && distance < best)
            {
                best = distance;
                nearest = j;
            }
        }
        if (nearest < 0 || saddle(peaks[i], peaks[nearest]) > DOUBLE_SADDLE * std::min(peaks[i].value, peaks[nearest].value))
            continue;
        cv::Point2d offset(peaks[nearest].x - peaks[i].x, peaks[nearest].y - peaks[i].y);
        if (offset.y < 0 || (offset.y == 0 && offset.x < 0))
            offset = -offset;
        offsets.push_back(offset);
    }

    // The most common offset
    int count = 0;
    cv::Point2d mode;
    for (const auto &candidate : offsets)
    {
        const double tolerance = std::max(1.5, 0.15 * std::hypot(candidate.x, candidate.y));
        int members = 0;
        cv::Point2d sum;
        for (const auto &other : offsets)
        {
            if (std::hypot(other.x - candidate.x, other.y - candidate.y) <= tolerance)
            {
                members++;
                sum += other;
            }
        }
        if (members > count)
        {
            count = members;
            mode = sum / members;
        }
    }
    if (count > 0)
    {
        stats.doubleOffset = std::hypot(mode.x, mode.y);
        stats.doubleFraction = static_cast<double>(count) / subjects;
    }
}
}

QRect measurementRegion(int width, int height)
{
    if (width <= 0 || height <= 0)
        return QRect();
    const int w = std::min(width, MAX_MEASURED_SIZE);
    const int h = std::min(height, MAX_MEASURED_SIZE);
    return QRect((width - w) / 2, (height - h) / 2, w, h);
}

ImageStats analyse(const FITSData &data, const QRect &requested)
{
    ImageStats stats;
    stats.median = data.getMedian();
    stats.max = data.getMax();

    const QRect frame(0, 0, data.width(), data.height());
    const QRect region = (requested.isValid() ? requested : measurementRegion(frame.width(), frame.height()))
                         .intersected(frame);
    // Too small to hold a star with its background annulus
    if (region.width() < 64 || region.height() < 64)
        return stats;
    stats.measuredFraction = static_cast<double>(region.width()) * region.height() /
                             (static_cast<double>(frame.width()) * frame.height());

    cv::Mat image = regionToFloat(data, region);
    if (image.empty())
        return stats;
    double lowest = 0, saturation = 0;
    cv::minMaxLoc(image, &lowest, &saturation);

    // Background map: the median of cells larger than stars, so that nebulae and gradients are not stars
    cv::Mat cells, background;
    const cv::Size grid(std::max(5, image.cols / BACKGROUND_CELL), std::max(5, image.rows / BACKGROUND_CELL));
    cv::resize(image, cells, grid, 0, 0, cv::INTER_AREA);
    cv::medianBlur(cells, cells, 5);
    cv::resize(cells, background, image.size(), 0, 0, cv::INTER_LINEAR);
    cv::Mat residual = image - background;
    background.release();

    // Detection: hot pixels removed, smoothed to about the size of a focused star
    cv::Mat detection;
    cv::medianBlur(residual, detection, 3);
    cv::GaussianBlur(detection, detection, cv::Size(0, 0), 1.5);
    const double threshold = DETECTION_SIGMA * std::max(robustSigma(detection), 1e-6);
    cv::Mat mask = detection > threshold;
    cv::Mat labels, components, centroids;
    const int count = cv::connectedComponentsWithStats(mask, labels, components, centroids, 8, CV_32S);

    // Light and brightest pixel of each source
    std::vector<double> flux(count, 0), peak(count, 0);
    for (int y = 0; y < image.rows; y++)
    {
        const int *label = labels.ptr<int>(y);
        const float *light = residual.ptr<float>(y);
        const float *raw = image.ptr<float>(y);
        for (int x = 0; x < image.cols; x++)
            if (label[x] > 0)
            {
                flux[label[x]] += light[x];
                peak[label[x]] = std::max(peak[label[x]], static_cast<double>(raw[x]));
            }
    }
    labels.release();
    std::vector<int> sources;
    for (int i = 1; i < count; i++)
        if (components.at<int>(i, cv::CC_STAT_AREA) >= MIN_STAR_AREA && flux[i] > 0)
            sources.push_back(i);
    std::sort(sources.begin(), sources.end(), [&flux](int a, int b)
    {
        return flux[a] > flux[b];
    });
    stats.measuredStars = sources.size();
    stats.stars = static_cast<int>(std::lround(stats.measuredStars / stats.measuredFraction));

    // Profiles of the brightest stars that are unsaturated, away from the edges, and without a bright neighbour
    // inside their background annulus
    std::vector<double> hfrs, concentrations, dips, elongations, eccentricities, lengths, weights;
    double sumCos = 0, sumSin = 0, sumWeight = 0;
    const size_t neighbours = std::min<size_t>(sources.size(), 3000);
    // Half the diagonal of the box around a source
    auto radius = [&components](int i)
    {
        return 0.5 * std::hypot(components.at<int>(i, cv::CC_STAT_WIDTH), components.at<int>(i, cv::CC_STAT_HEIGHT));
    };
    for (size_t k = 0; k < sources.size() && static_cast<int>(hfrs.size()) < MAX_SHAPE_STARS; k++)
    {
        const int i = sources[k];
        if (peak[i] >= 0.95 * saturation)
            continue;
        const double cx = centroids.at<double>(i, 0), cy = centroids.at<double>(i, 1);
        const double area = components.at<int>(i, cv::CC_STAT_AREA);
        const double aperture = std::clamp(std::max(3.0 * std::sqrt(area / M_PI), radius(i) + 4), 5.0, MAX_APERTURE);
        // Inside the frame with its background annulus
        const double margin = aperture + 11;
        if (cx < margin || cy < margin || cx > image.cols - margin || cy > image.rows - margin)
            continue;
        // No bright neighbour in the annulus, and not a piece of a brighter source, e.g. of the rim of a disc
        bool isolated = true;
        for (size_t j = 0; j < neighbours && isolated; j++)
        {
            const int other = sources[j];
            if (other == i || flux[other] <= 0.05 * flux[i])
                continue;
            const double distance = std::hypot(centroids.at<double>(other, 0) - cx, centroids.at<double>(other, 1) - cy);
            const cv::Rect box(components.at<int>(other, cv::CC_STAT_LEFT) - 2, components.at<int>(other, cv::CC_STAT_TOP) - 2,
                               components.at<int>(other, cv::CC_STAT_WIDTH) + 4, components.at<int>(other, cv::CC_STAT_HEIGHT) + 4);
            isolated = distance >= aperture + 9 &&
                       !(flux[other] > flux[i] && box.contains(cv::Point(static_cast<int>(cx), static_cast<int>(cy))));
        }
        Profile profile;
        if (!isolated || !measureProfile(residual, cx, cy, aperture, profile))
            continue;

        // A star counts by its signal: a few bright stars outweigh faint detections, e.g. hot pixels spread by
        // debayering, and a frame with few stars is judged by its real stars
        const double signal = std::sqrt(flux[i]);
        weights.push_back(signal);
        hfrs.push_back(profile.hfr);
        concentrations.push_back(profile.concentration);
        dips.push_back(profile.centreDip);
        elongations.push_back(profile.a / profile.b);
        eccentricities.push_back(std::sqrt(1.0 - (profile.b * profile.b) / (profile.a * profile.a)));
        // A line of length L blurred by a round star of sigma b has a^2 = L^2 / 12 + b^2
        lengths.push_back(std::sqrt(12.0 * (profile.a * profile.a - profile.b * profile.b)));
        // Directions are axes: theta and theta + 180 are the same, so average the doubled angle.
        // Round stars have no direction and weigh nothing.
        const double weight = signal * (profile.a - profile.b) / (profile.a + profile.b);
        sumCos += weight * std::cos(2 * profile.theta);
        sumSin += weight * std::sin(2 * profile.theta);
        sumWeight += weight;
    }

    stats.profiledStars = hfrs.size();
    stats.hfr = weightedMedian(hfrs, weights);
    stats.concentration = weightedMedian(concentrations, weights);
    stats.centreDip = dips.empty() ? 1 : weightedMedian(dips, weights);
    stats.elongation = elongations.empty() ? 1 : weightedMedian(elongations, weights);
    stats.eccentricity = weightedMedian(eccentricities, weights);
    stats.trailLength = lengths.empty() ? 0 : weightedMedian(lengths, weights);
    if (sumWeight > 0)
    {
        stats.coherence = std::hypot(sumCos, sumSin) / sumWeight;
        double angle = qRadiansToDegrees(std::atan2(sumSin, sumCos)) / 2.0;
        if (angle < 0)
            angle += 180;
        stats.angle = angle;
    }

    // The two images of a doubled star are two sources, each without an isolated profile, or one elongated source
    findDoubles(detection, mask, std::max(30.0, 15 * std::max(1.0, stats.hfr)), stats);

    const bool profiled = stats.profiledStars >= MIN_SHAPE_STARS;
    const bool trailed = profiled && stats.elongation >= MIN_TRAIL_ELONGATION && stats.coherence >= MIN_TRAIL_COHERENCE;
    // Only round stars large enough to resolve their profile tell their focus. A flat profile is not enough on its
    // own when the stars are elongated: a short trail or coma flattens it too.
    const bool defocused = profiled && stats.elongation < MIN_TRAIL_ELONGATION && stats.hfr >= MIN_PROFILE_HFR &&
                           (stats.concentration < MIN_FOCUSED_CONCENTRATION || stats.centreDip < MAX_FOCUSED_CENTRE_DIP);

    if (stats.stars < MIN_STARS)
        // Defocus spreads the light of faint stars below the detection limit: the few left tell why
        stats.verdict = defocused ? ImageVerdict::OutOfFocus : ImageVerdict::FewStars;
    else if (stats.doubleFraction >= MIN_DOUBLE_FRACTION)
        stats.verdict = ImageVerdict::DoubleImages;
    else if (!profiled)
        stats.verdict = ImageVerdict::Unknown;
    else if (trailed)
        stats.verdict = ImageVerdict::Trailed;
    else if (defocused)
        stats.verdict = ImageVerdict::OutOfFocus;
    else
        stats.verdict = ImageVerdict::Good;

    return stats;
}

double siderealDrift(double exposure, double declination, double pixelScale)
{
    if (pixelScale <= 0 || exposure <= 0)
        return 0;
    return SIDEREAL_RATE * std::cos(qDegreesToRadians(declination)) * exposure / pixelScale;
}

}
}
