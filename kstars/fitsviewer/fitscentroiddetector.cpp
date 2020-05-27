/***************************************************************************
                          fitscentroiddetector.cpp  -  FITS Image
                             -------------------
    begin                : Sat March 28 2020
    copyright            : (C) 2004 by Jasem Mutlaq, (C) 2020 by Eric Dejouhanet
    email                : eric.dejouhanet@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/

#include <math.h>
#include <cmath>

#include "fitscentroiddetector.h"
#include "fits_debug.h"

FITSStarDetector &FITSCentroidDetector::configure(const QString &setting, const QVariant &value)
{
    if (!setting.compare("MINIMUM_STDVAR", Qt::CaseInsensitive))
        if (value.canConvert <int> ())
            MINIMUM_STDVAR = value.value <int> ();

    if (!setting.compare("MINIMUM_PIXEL_RANGE", Qt::CaseInsensitive))
        if (value.canConvert <int> ())
            MINIMUM_PIXEL_RANGE = value.value <int> ();

    if (!setting.compare("JMINDEX", Qt::CaseInsensitive))
        if (value.canConvert <double> ())
            JMINDEX = value.value <double> ();

    return *this;
}

bool FITSCentroidDetector::checkCollision(Edge * s1, Edge * s2) const
{
    int dis; //distance

    int diff_x = s1->x - s2->x;
    int diff_y = s1->y - s2->y;

    dis = std::abs(sqrt(diff_x * diff_x + diff_y * diff_y));
    dis -= s1->width / 2;
    dis -= s2->width / 2;

    if (dis <= 0) //collision
        return true;

    //no collision
    return false;
}

/*** Find center of stars and calculate Half Flux Radius */
int FITSCentroidDetector::findSources(QList<Edge*> &starCenters, const QRect &boundary)
{
    switch (parent()->property("dataType").toInt())
    {
        case TBYTE:
            return findSources<uint8_t const>(starCenters, boundary);

        case TSHORT:
            return findSources<int16_t const>(starCenters, boundary);

        case TUSHORT:
            return findSources<uint16_t const>(starCenters, boundary);

        case TLONG:
            return findSources<int32_t const>(starCenters, boundary);

        case TULONG:
            return findSources<uint32_t const>(starCenters, boundary);

        case TFLOAT:
            return findSources<float const>(starCenters, boundary);

        case TLONGLONG:
            return findSources<int64_t const>(starCenters, boundary);

        case TDOUBLE:
            return findSources<double const>(starCenters, boundary);

        default:
            return -1;
    }
}

template <typename T>
int FITSCentroidDetector::findSources(QList<Edge*> &starCenters, const QRect &boundary)
{
    FITSData const * const image_data = reinterpret_cast<FITSData const *>(parent());

    if (image_data == nullptr)
        return 0;

    Statistic const &stats = image_data->getStatistics();
    FITSMode const m_Mode = static_cast <FITSMode> (parent()->property("mode").toInt());

    int initStdDev = MINIMUM_STDVAR;
    int minEdgeWidth = MINIMUM_PIXEL_RANGE;

    double threshold = 0, sum = 0, avg = 0, min = 0;
    int starDiameter     = 0;
    int pixVal           = 0;
    int minimumEdgeCount = MINIMUM_EDGE_LIMIT;

    auto * buffer = reinterpret_cast<T const *>(image_data->getImageBuffer());

    double JMIndex = JMINDEX;

    float dispersion_ratio = 1.5;

    QList<Edge *> edges;

    if (JMIndex < DIFFUSE_THRESHOLD)
    {
        minEdgeWidth     = JMIndex * 35 + 1;
        minimumEdgeCount = minEdgeWidth - 1;
    }
    else
    {
        minEdgeWidth     = 6;
        minimumEdgeCount = 4;
    }

    while (initStdDev >= 1)
    {
        minEdgeWidth--;
        minimumEdgeCount--;

        minEdgeWidth     = qMax(3, minEdgeWidth);
        minimumEdgeCount = qMax(3, minimumEdgeCount);

        if (JMIndex < DIFFUSE_THRESHOLD)
        {
            // Taking the average out seems to have better result for noisy images
            threshold = stats.max[0] - stats.mean[0] * ((MINIMUM_STDVAR - initStdDev) * 0.5 + 1);

            min = stats.min[0];
            if (threshold - min < 0)
            {
                threshold = stats.mean[0] * ((MINIMUM_STDVAR - initStdDev) * 0.5 + 1);
                min       = 0;
            }

            dispersion_ratio = 1.4 - (MINIMUM_STDVAR - initStdDev) * 0.08;
        }
        else
        {
            threshold = stats.mean[0] + stats.stddev[0] * initStdDev * (0.3 - (MINIMUM_STDVAR - initStdDev) * 0.05);
            min       = stats.min[0];
            // Ratio between centeroid center and edge
            dispersion_ratio = 1.8 - (MINIMUM_STDVAR - initStdDev) * 0.2;
        }

        qCDebug(KSTARS_FITS) << "SNR: " << stats.SNR;
        qCDebug(KSTARS_FITS) << "The threshold level is " << threshold << "(actual " << threshold - min
                             << ")  minimum edge width" << minEdgeWidth << " minimum edge limit " << minimumEdgeCount;

        threshold -= min;

        int subX, subY, subW, subH;

        if (boundary.isNull())
        {
            if (m_Mode == FITS_GUIDE || m_Mode == FITS_FOCUS)
            {
                // Only consider the central 70%
                subX = round(stats.width * 0.15);
                subY = round(stats.height * 0.15);
                subW = stats.width - subX;
                subH = stats.height - subY;
            }
            else
            {
                // Consider the complete area 100%
                subX = 0;
                subY = 0;
                subW = stats.width;
                subH = stats.height;
            }
        }
        else
        {
            subX = boundary.x();
            subY = boundary.y();
            subW = subX + boundary.width();
            subH = subY + boundary.height();
        }

        // Detect "edges" that are above threshold
        for (int i = subY; i < subH; i++)
        {
            starDiameter = 0;

            for (int j = subX; j < subW; j++)
            {
                pixVal = buffer[j + (i * stats.width)] - min;

                // If pixel value > threshold, let's get its weighted average
                if (pixVal >= threshold)
                {
                    avg += j * pixVal;
                    sum += pixVal;
                    starDiameter++;
                }
                // Value < threshold but avg exists
                else if (sum > 0)
                {
                    // We found a potential centroid edge
                    if (starDiameter >= minEdgeWidth)
                    {
                        float center = avg / sum + 0.5;
                        if (center > 0)
                        {
                            int i_center = std::floor(center);

                            // Check if center is 10% or more brighter than edge, if not skip
                            if (((buffer[i_center + (i * stats.width)] - min) /
                                    (buffer[i_center + (i * stats.width) - starDiameter / 2] - min) >=
                                    dispersion_ratio) &&
                                    ((buffer[i_center + (i * stats.width)] - min) /
                                     (buffer[i_center + (i * stats.width) + starDiameter / 2] - min) >=
                                     dispersion_ratio))
                            {
                                qCDebug(KSTARS_FITS)
                                        << "Edge center is " << buffer[i_center + (i * stats.width)] - min
                                        << " Edge is " << buffer[i_center + (i * stats.width) - starDiameter / 2] - min
                                        << " and ratio is "
                                        << ((buffer[i_center + (i * stats.width)] - min) /
                                            (buffer[i_center + (i * stats.width) - starDiameter / 2] - min))
                                        << " located at X: " << center << " Y: " << i + 0.5;

                                auto * newEdge = new Edge();

                                newEdge->x       = center;
                                newEdge->y       = i + 0.5;
                                newEdge->scanned = 0;
                                newEdge->val     = buffer[i_center + (i * stats.width)] - min;
                                newEdge->width   = starDiameter;
                                newEdge->HFR     = 0;
                                newEdge->sum     = sum;

                                edges.append(newEdge);
                            }
                        }
                    }

                    // Reset
                    avg = sum = starDiameter = 0;
                }
            }
        }

        qCDebug(KSTARS_FITS) << "Total number of edges found is: " << edges.count();

        // In case of hot pixels
        if (edges.count() == 1 && initStdDev > 1)
        {
            initStdDev--;
            continue;
        }

        if (edges.count() >= MAX_EDGE_LIMIT)
        {
            qCWarning(KSTARS_FITS) << "Too many edges, aborting... " << edges.count();
            qDeleteAll(edges);
            return -1;
        }

        if (edges.count() >= minimumEdgeCount)
            break;

        qDeleteAll(edges);
        edges.clear();
        initStdDev--;
    }

    int cen_count = 0;
    int cen_x     = 0;
    int cen_y     = 0;
    int cen_v     = 0;
    int cen_w     = 0;
    int width_sum = 0;

    // Let's sort edges, starting with widest
    auto const greaterThan = [](Edge const * a, Edge const * b)
    {
        return a->sum > b->sum;
    };
    std::sort(edges.begin(), edges.end(), greaterThan);

    // Now, let's scan the edges and find the maximum centroid vertically
    for (int i = 0; i < edges.count(); i++)
    {
        qCDebug(KSTARS_FITS) << "# " << i << " Edge at (" << edges[i]->x << "," << edges[i]->y << ") With a value of "
                             << edges[i]->val << " and width of " << edges[i]->width << " pixels. with sum " << edges[i]->sum;

        // If edge scanned already, skip
        if (edges[i]->scanned == 1)
        {
            qCDebug(KSTARS_FITS) << "Skipping check for center " << i << " because it was already counted";
            continue;
        }

        qCDebug(KSTARS_FITS) << "Investigating edge # " << i << " now ...";

        // Get X, Y, and Val of edge
        cen_x = edges[i]->x;
        cen_y = edges[i]->y;
        cen_v = edges[i]->sum;
        cen_w = edges[i]->width;

        float avg_x = 0;
        float avg_y = 0;

        sum       = 0;
        cen_count = 0;

        // Now let's compare to other edges until we hit a maxima
        for (int j = 0; j < edges.count(); j++)
        {
            if (edges[j]->scanned)
                continue;

            if (checkCollision(edges[j], edges[i]))
            {
                if (edges[j]->sum >= cen_v)
                {
                    cen_v = edges[j]->sum;
                    cen_w = edges[j]->width;
                }

                edges[j]->scanned = 1;
                cen_count++;

                avg_x += edges[j]->x * edges[j]->val;
                avg_y += edges[j]->y * edges[j]->val;
                sum += edges[j]->val;

                continue;
            }
        }

        int cen_limit = (MINIMUM_ROWS_PER_CENTER - (MINIMUM_STDVAR - initStdDev));

        if (edges.count() < LOW_EDGE_CUTOFF_1)
        {
            if (edges.count() < LOW_EDGE_CUTOFF_2)
                cen_limit = 1;
            else
                cen_limit = 2;
        }

        qCDebug(KSTARS_FITS) << "center_count: " << cen_count << " and initstdDev= " << initStdDev << " and limit is "
                             << cen_limit;

        if (cen_limit < 1)
            continue;

        // If centroid count is within acceptable range
        //if (cen_limit >= 2 && cen_count >= cen_limit)
        if (cen_count >= cen_limit)
        {
            // We detected a centroid, let's init it
            auto * rCenter = new Edge();

            rCenter->x = avg_x / sum;
            rCenter->y = avg_y / sum;
            width_sum += rCenter->width;
            rCenter->width = cen_w;

            qCDebug(KSTARS_FITS) << "Found a real center with number with (" << rCenter->x << "," << rCenter->y << ")";

            // Calculate Total Flux From Center, Half Flux, Full Summation
            double TF   = 0;
            double HF   = 0;
            double FSum = 0;

            cen_x = (int)std::floor(rCenter->x);
            cen_y = (int)std::floor(rCenter->y);

            if (cen_x < 0 || cen_x > stats.width || cen_y < 0 || cen_y > stats.height)
            {
                delete rCenter;
                continue;
            }

            // Complete sum along the radius
            //for (int k=0; k < rCenter->width; k++)
            for (int k = rCenter->width / 2; k >= -(rCenter->width / 2); k--)
            {
                FSum += buffer[cen_x - k + (cen_y * stats.width)] - min;
                //qDebug() << image_buffer[cen_x-k+(cen_y*stats.width)] - min;
            }

            // Half flux
            HF = FSum / 2.0;

            // Total flux starting from center
            TF = buffer[cen_y * stats.width + cen_x] - min;

            int pixelCounter = 1;

            // Integrate flux along radius axis until we reach half flux
            for (int k = 1; k < rCenter->width / 2; k++)
            {
                if (TF >= HF)
                {
                    qCDebug(KSTARS_FITS) << "Stopping at TF " << TF << " after #" << k << " pixels.";
                    break;
                }

                TF += buffer[cen_y * stats.width + cen_x + k] - min;
                TF += buffer[cen_y * stats.width + cen_x - k] - min;

                pixelCounter++;
            }

            // Calculate weighted Half Flux Radius
            rCenter->HFR = pixelCounter * (HF / TF);
            // Store full flux
            rCenter->val = FSum;

            qCDebug(KSTARS_FITS) << "HFR for this center is " << rCenter->HFR << " pixels and the total flux is " << FSum;

            starCenters.append(rCenter);
        }
    }

    if (starCenters.count() > 1 && m_Mode != FITS_FOCUS)
    {
        float width_avg = (float)width_sum / starCenters.count();
        float lsum = 0, sdev = 0;

        for (auto &center : starCenters)
            lsum += (center->width - width_avg) * (center->width - width_avg);

        sdev = (std::sqrt(lsum / (starCenters.count() - 1))) * 4;

        // Reject stars > 4 * stddev
        foreach (Edge * center, starCenters)
            if (center->width > sdev)
                starCenters.removeOne(center);

        //foreach(Edge *center, starCenters)
        //qDebug() << center->x << "," << center->y << "," << center->width << "," << center->val << endl;
    }

    // Release memory
    qDeleteAll(edges);

    return starCenters.count();
}


