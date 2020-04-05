/***************************************************************************
                          fitssepdetector.cpp  -  FITS Image
                             -------------------
    begin                : Sun March 29 2020
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

#include "sep/sep.h"
#include "fits_debug.h"
#include "fitssepdetector.h"

FITSStarDetector& FITSSEPDetector::configure(const QString &, const QVariant &)
{
    return *this;
}

int FITSSEPDetector::findSources(QList<Edge*> &starCenters, const QRect &boundary)
{
    FITSData const * const image_data = reinterpret_cast<FITSData const *>(parent());

    if (image_data == nullptr)
        return 0;

    FITSData::Statistic const &stats = image_data->getStatistics();

    int x = 0, y = 0, w = stats.width, h = stats.height, maxRadius = 50;

    if (!boundary.isNull())
    {
        x = boundary.x();
        y = boundary.y();
        w = boundary.width();
        h = boundary.height();
        maxRadius = w;
    }

    auto * data = new float[w * h];

    switch (stats.bitpix)
    {
        case BYTE_IMG:
            getFloatBuffer<uint8_t>(data, x, y, w, h, image_data);
            break;
        case SHORT_IMG:
            getFloatBuffer<int16_t>(data, x, y, w, h, image_data);
            break;
        case USHORT_IMG:
            getFloatBuffer<uint16_t>(data, x, y, w, h, image_data);
            break;
        case LONG_IMG:
            getFloatBuffer<int32_t>(data, x, y, w, h, image_data);
            break;
        case ULONG_IMG:
            getFloatBuffer<uint32_t>(data, x, y, w, h, image_data);
            break;
        case FLOAT_IMG:
            memcpy(data, image_data->getImageBuffer(), sizeof(float)*w*h);
            break;
        case LONGLONG_IMG:
            getFloatBuffer<int64_t>(data, x, y, w, h, image_data);
            break;
        case DOUBLE_IMG:
            getFloatBuffer<double>(data, x, y, w, h, image_data);
            break;
        default:
            delete [] data;
            return -1;
    }

    float * imback = nullptr;
    double * flux = nullptr, *fluxerr = nullptr, *area = nullptr;
    short * flag = nullptr;
    short flux_flag = 0;
    int status = 0;
    sep_bkg * bkg = nullptr;
    sep_catalog * catalog = nullptr;
    float conv[] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
    double flux_fractions[2] = {0};
    double requested_frac[2] = { 0.5, 0.99 };
    QList<Edge *> edges;

    // #0 Create SEP Image structure
    sep_image im = {data, nullptr, nullptr, SEP_TFLOAT, 0, 0, w, h, 0.0, SEP_NOISE_NONE, 1.0, 0.0};

    // #1 Background estimate
    status = sep_background(&im, 64, 64, 3, 3, 0.0, &bkg);
    if (status != 0) goto exit;

    // #2 Background evaluation
    imback = (float *)malloc((w * h) * sizeof(float));
    status = sep_bkg_array(bkg, imback, SEP_TFLOAT);
    if (status != 0) goto exit;

    // #3 Background subtraction
    status = sep_bkg_subarray(bkg, im.data, im.dtype);
    if (status != 0) goto exit;

    // #4 Source Extraction
    // Note that we set deblend_cont = 1.0 to turn off deblending.
    status = sep_extract(&im, 2 * bkg->globalrms, SEP_THRESH_ABS, 10, conv, 3, 3, SEP_FILTER_CONV, 32, 1.0, 1, 1.0, &catalog);
    if (status != 0) goto exit;

    // TODO
    // Must detect edge detection
    // Must limit to brightest 100 (by flux) centers
    // Should probably use ellipse to draw instead of simple circle?
    // Useful for galaxies and also elenogated stars.
    for (int i = 0; i < catalog->nobj; i++)
    {
        double flux = catalog->flux[i];
        // Get HFR
        sep_flux_radius(&im, catalog->x[i], catalog->y[i], maxRadius, 5, 0, &flux, requested_frac, 2, flux_fractions, &flux_flag);

        auto * center = new Edge();
        center->x = catalog->x[i] + x + 0.5;
        center->y = catalog->y[i] + y + 0.5;
        center->val = catalog->peak[i];
        center->sum = flux;
        center->HFR = center->width = flux_fractions[0];
        if (flux_fractions[1] < maxRadius)
            center->width = flux_fractions[1] * 2;
        edges.append(center);
    }

    // Let's sort edges, starting with widest
    std::sort(edges.begin(), edges.end(), [](const Edge * edge1, const Edge * edge2) -> bool { return edge1->width > edge2->width;});

    // Take only the first 100 stars
    {
        int starCount = qMin(100, edges.count());
        for (int i = 0; i < starCount; i++)
            starCenters.append(edges[i]);
    }

    edges.clear();

    qCDebug(KSTARS_FITS) << qSetFieldWidth(10) << "#" << "#X" << "#Y" << "#Flux" << "#Width" << "#HFR";
    for (int i = 0; i < starCenters.count(); i++)
        qCDebug(KSTARS_FITS) << qSetFieldWidth(10) << i << starCenters[i]->x << starCenters[i]->y
                             << starCenters[i]->sum << starCenters[i]->width << starCenters[i]->HFR;

exit:
    if (stats.bitpix != FLOAT_IMG)
        delete [] data;
    sep_bkg_free(bkg);
    sep_catalog_free(catalog);
    free(imback);
    free(flux);
    free(fluxerr);
    free(area);
    free(flag);

    if (status != 0)
    {
        char errorMessage[512];
        sep_get_errmsg(status, errorMessage);
        qCritical(KSTARS_FITS) << errorMessage;
        return -1;
    }

    return starCenters.count();
}

template <typename T>
void FITSSEPDetector::getFloatBuffer(float * buffer, int x, int y, int w, int h, FITSData const *data) const
{
    auto * rawBuffer = reinterpret_cast<T const *>(data->getImageBuffer());

    if (buffer == nullptr)
        return;

    float * floatPtr = buffer;

    int x2 = x + w;
    int y2 = y + h;

    FITSData::Statistic const &stats = data->getStatistics();

    for (int y1 = y; y1 < y2; y1++)
    {
        int offset = y1 * stats.width;
        for (int x1 = x; x1 < x2; x1++)
        {
            *floatPtr++ = rawBuffer[offset + x1];
        }
    }
}
