/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#include <math.h>
#include <cmath>
#include <QtConcurrent>

#include "fits_debug.h"
#include "fitsthresholddetector.h"

//void FITSThresholdDetector::configure(const QString &setting, const QVariant &value)
//{
//    if (!setting.compare("THRESHOLD_PERCENTAGE", Qt::CaseInsensitive))
//        if (value.canConvert<int>())
//            THRESHOLD_PERCENTAGE = value.value<int>();
//}

QFuture<bool> FITSThresholdDetector::findSources(QRect const &boundary)
{
    FITSImage::Statistic const &stats = m_ImageData->getStatistics();
    switch (stats.dataType)
    {
        case TSHORT:
            return QtConcurrent::run(this, &FITSThresholdDetector::findOneStar<int16_t>, boundary);

        case TUSHORT:
            return QtConcurrent::run(this, &FITSThresholdDetector::findOneStar<uint16_t>, boundary);

        case TLONG:
            return QtConcurrent::run(this, &FITSThresholdDetector::findOneStar<int32_t>, boundary);

        case TULONG:
            return QtConcurrent::run(this, &FITSThresholdDetector::findOneStar<uint32_t>, boundary);

        case TFLOAT:
            return QtConcurrent::run(this, &FITSThresholdDetector::findOneStar<float>, boundary);

        case TLONGLONG:
            return QtConcurrent::run(this, &FITSThresholdDetector::findOneStar<int64_t>, boundary);

        case TDOUBLE:
            return QtConcurrent::run(this, &FITSThresholdDetector::findOneStar<double>, boundary);

        case TBYTE:
        default:
            return QtConcurrent::run(this, &FITSThresholdDetector::findOneStar<uint8_t>, boundary);
    }

}

template <typename T>
bool FITSThresholdDetector::findOneStar(const QRect &boundary) const
{
    FITSImage::Statistic const &stats = m_ImageData->getStatistics();

    int subX = boundary.x();
    int subY = boundary.y();
    int subW = subX + boundary.width();
    int subH = subY + boundary.height();

    float massX = 0, massY = 0, totalMass = 0;

    auto * buffer = reinterpret_cast<T const *>(m_ImageData->getImageBuffer());

    double THRESHOLD_PERCENTAGE = getValue("THRESHOLD_PERCENTAGE", 120.0).toDouble();
    // TODO replace magic number with something more useful to understand
    double threshold = stats.mean[0] * THRESHOLD_PERCENTAGE / 100.0;

    for (int y = subY; y < subH; y++)
    {
        for (int x = subX; x < subW; x++)
        {
            T pixel = buffer[x + y * stats.width];
            if (pixel > threshold)
            {
                totalMass += pixel;
                massX += x * pixel;
                massY += y * pixel;
            }
        }
    }

    qCDebug(KSTARS_FITS) << "FITS: Weighted Center is X: " << massX / totalMass << " Y: " << massY / totalMass;

    auto * center  = new Edge;
    center->width = -1;
    center->x     = massX / totalMass + 0.5;
    center->y     = massY / totalMass + 0.5;
    center->HFR   = 1;

    // Maximum Radius
    int maxR = qMin(subW - 1, subH - 1) / 2;

    // Critical threshold
    double critical_threshold = threshold * 0.7;
    double running_threshold  = threshold;

    while (running_threshold >= critical_threshold)
    {
        for (int r = maxR; r > 1; r--)
        {
            int pass = 0;

            for (float theta = 0; theta < 2 * M_PI; theta += (2 * M_PI) / 10.0)
            {
                int testX = center->x + std::cos(theta) * r;
                int testY = center->y + std::sin(theta) * r;

                // if out of bound, break;
                if (testX < subX || testX > subW || testY < subY || testY > subH)
                    break;

                if (buffer[testX + testY * stats.width] > running_threshold)
                    pass++;
            }

            //qDebug() << "Testing for radius " << r << " passes # " << pass << " @ threshold " << running_threshold;
            //if (pass >= 6)
            if (pass >= 5)
            {
                center->width = r * 2;
                break;
            }
        }

        if (center->width > 0)
            break;

        // Increase threshold fuzziness by 10%
        running_threshold -= running_threshold * 0.1;
    }

    // If no stars were detected
    if (center->width == -1)
    {
        delete center;
        return false;
    }

    // 30% fuzzy
    //center->width += center->width*0.3 * (running_threshold / threshold);

    double FSum = 0, HF = 0, TF = 0, min = stats.min[0];
    const double resolution = 1.0 / 20.0;

    int cen_y = qRound(center->y);

    double rightEdge = center->x + center->width / 2.0;
    double leftEdge  = center->x - center->width / 2.0;

    QVector<double> subPixels;
    subPixels.reserve(center->width / resolution);

    for (double x = leftEdge; x <= rightEdge; x += resolution)
    {
        //subPixels[x] = resolution * (image_buffer[static_cast<int>(floor(x)) + cen_y * stats.width] - min);
        double slice = resolution * (buffer[static_cast<int>(floor(x)) + cen_y * stats.width] - min);
        FSum += slice;
        subPixels.append(slice);
    }

    // Half flux
    HF = FSum / 2.0;

    //double subPixelCenter = center->x - fmod(center->x,resolution);
    int subPixelCenter = (center->width / resolution) / 2;

    // Start from center
    TF            = subPixels[subPixelCenter];
    double lastTF = TF;
    // Integrate flux along radius axis until we reach half flux
    //for (double k=resolution; k < (center->width/(2*resolution)); k += resolution)
    for (int k = 1; k < subPixelCenter; k++)
    {
        TF += subPixels[subPixelCenter + k];
        TF += subPixels[subPixelCenter - k];

        if (TF >= HF)
        {
            // We have two ways to calculate HFR. The first is the correct method but it can get quite variable within 10% due to random fluctuations of the measured star.
            // The second method is not truly HFR but is much more resistant to noise.

            // #1 Approximate HFR, accurate and reliable but quite variable to small changes in star flux
            center->HFR = (k - 1 + ((HF - lastTF) / (TF - lastTF)) * 2) * resolution;

            // #2 Not exactly HFR, but much more stable
            //center->HFR = (k*resolution) * (HF/TF);
            break;
        }

        lastTF = TF;
    }

    QList<Edge*> starCenters;
    starCenters.append(center);
    m_ImageData->setStarCenters(starCenters);

    return true;
}
