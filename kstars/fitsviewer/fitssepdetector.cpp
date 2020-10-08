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

#include "config-kstars.h"
#include "fits_debug.h"
#include "fitssepdetector.h"
#include "Options.h"
#include "kspaths.h"

#include <math.h>
#include <QPointer>

#ifdef HAVE_STELLARSOLVER
#include <stellarsolver.h>
#else
#include "sep/sep.h"
#endif

FITSSEPDetector &FITSSEPDetector::configure(const QString &param, const QVariant &value)
{
    if (param == "numStars")
        numStars = value.toInt();
    else if (param == "fractionRemoved")
        fractionRemoved = value.toDouble();
    else if (param == "deblendNThresh")
        deblendNThresh = value.toInt();
    else if (param == "deblendMincont")
        deblendMincont = value.toDouble();
    else if (param == "radiusIsBoundary")
        radiusIsBoundary = value.toBool();
    else
        qCDebug(KSTARS_FITS) << "Bad SEP Parameter!!!!! " << param;
    return *this;
}

// TODO: (hy 4/11/2020)
// The api into these star detection methods should be generalized so that various parameters
// could be controlled by the caller. For instance, saturation removal, removal of N% of the largest stars
// number of stars desired, some parameters to the SEP algorithms, as all of these may have different
// optimal values for different uses. Also, there may be other desired outputs
// (e.g. unfiltered number of stars detected,background sky level). Waiting on rlancaste's
// investigations into SEP before doing this.

int FITSSEPDetector::findSources(QList<Edge*> &starCenters, QRect const &boundary)
{
    return findSourcesAndBackground(starCenters, boundary, nullptr);
}

int FITSSEPDetector::findSourcesAndBackground(QList<Edge*> &starCenters, QRect const &boundary,
        SkyBackground *bg)
{
    FITSData const * const image_data = reinterpret_cast<FITSData const *>(parent());
    starCenters.clear();

    if (image_data == nullptr)
        return 0;

#ifdef HAVE_STELLARSOLVER
    Q_UNUSED(bg);
    //Note this is the part I added.  It is just an initial attempt to get it working
    constexpr int maxNumCenters =
        50;  //This parameter can be set in the profile, but I did it this way since it is used in the code further down.
    QPointer<StellarSolver> solver = new StellarSolver(image_data->getStatistics(), image_data->getImageBuffer());
    QString savedOptionsProfiles = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                                   QString("SavedOptionsProfiles.ini");
    QList<SSolver::Parameters> optionsList = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    if(optionsList.count() > static_cast<int>(Options::focusOptionsProfile()))
        solver->setParameters(optionsList.at(Options::focusOptionsProfile()));
    else
        solver->setParameterProfile(SSolver::Parameters::ALL_STARS);

    qCDebug(KSTARS_FITS) << "Sextract with: " << optionsList.at(Options::focusOptionsProfile()).listName;

    QList<FITSImage::Star> stars;
    if (!boundary.isNull())
    {
        solver->sextractWithHFR(boundary);
        if(!solver->sextractionDone() || solver->failed())
            return 0;
        stars = solver->getStarList();
    }

    if (starCenters.empty())
    {
        solver->sextractWithHFR();
        if(!solver->sextractionDone() || solver->failed())
            return 0;
        stars = solver->getStarList();
    }

    QList<Edge *> edges;

    for (int index = 0; index < stars.count(); index++)
    {
        FITSImage::Star star = stars.at(index);
        auto * center = new Edge();
        center->x = star.x;
        center->y = star.y;
        center->val = star.peak;
        center->sum = star.flux;
        center->HFR = star.HFR;
        center->width = star.a;
        edges.append(center);
    }
    //There is more information that can be obtained by the Stellarsolver.
    //Background info, Star positions(if a plate solve was done before), etc
    //The information is available as long as the StellarSolver exists.

#else

    FITSImage::Statistic const &stats = image_data->getStatistics();

    int x = 0, y = 0, w = stats.width, h = stats.height, maxRadius = 50;
    std::vector<std::pair<int, double>> ovals;
    const int maxNumCenters = numStars;

    // We may skip 20% of the stars (those with the largest 20% HFRs) as those are suspect
    // to be non-stars) if we have plenty of detections.
    int startIndex = 0;

    if (!boundary.isNull())
    {
        x = boundary.x();
        y = boundary.y();
        w = boundary.width();
        h = boundary.height();
        if (radiusIsBoundary)
            maxRadius = w;
    }

    auto * data = new float[w * h];

    switch (parent()->property("dataType").toInt())
    {
        case TBYTE:
            getFloatBuffer<uint8_t>(data, x, y, w, h, image_data);
            break;
        case TSHORT:
            getFloatBuffer<int16_t>(data, x, y, w, h, image_data);
            break;
        case TUSHORT:
            getFloatBuffer<uint16_t>(data, x, y, w, h, image_data);
            break;
        case TLONG:
            getFloatBuffer<int32_t>(data, x, y, w, h, image_data);
            break;
        case TULONG:
            getFloatBuffer<uint32_t>(data, x, y, w, h, image_data);
            break;
        case TFLOAT:
            if (boundary.isNull())
                memcpy(data, image_data->getImageBuffer(), sizeof(float)*w * h);
            else
                getFloatBuffer<float>(data, x, y, w, h, image_data);
            break;
        case TLONGLONG:
            getFloatBuffer<int64_t>(data, x, y, w, h, image_data);
            break;
        case TDOUBLE:
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

    if (bg != nullptr)
        bg->initialize(bkg->global, bkg->globalrms, bkg->bh * bkg->bw);

    // #3 Background subtraction
    status = sep_bkg_subarray(bkg, im.data, im.dtype);
    if (status != 0) goto exit;

    // #4 Source Extraction
    status = sep_extract(&im, 2 * bkg->globalrms, SEP_THRESH_ABS, 10, conv, 3, 3, SEP_FILTER_CONV,
                         deblendNThresh, deblendMincont, 1, 1.0, &catalog);
    if (status != 0) goto exit;
    qCDebug(KSTARS_FITS) << "SEP detected " << catalog->nobj << " stars.";
    if (bg != nullptr)
        bg->setStarsDetected(catalog->nobj);

    // Skip the 20% largest stars if we have plenty.
    if (catalog->nobj * (1 - fractionRemoved) > maxNumCenters)
        startIndex = catalog->nobj * fractionRemoved;

    // Find the oval sizes for each detection in the detected star catalog, and sort by that. Oval size
    // correlates very well with HFR, so we don't need to call sep_flux_radius on all detections later
    // to find the maxNumCenters largest stars. This can save a lot of time.
    for (int i = 0; i < catalog->nobj; i++)
    {
        const double ovalSizeSq = catalog->a[i] * catalog->a[i] + catalog->b[i] * catalog->b[i];
        ovals.push_back(std::pair<int, double>(i, ovalSizeSq));
    }
    std::sort(ovals.begin(), ovals.end(), [](const std::pair<int, double> &o1, const std::pair<int, double> &o2) -> bool { return o1.second > o2.second;});

    // Go through the largest (by oval size) detections and compute the HFR for the first maxNumCenters.
    for (int index = startIndex; index < catalog->nobj; index++)
    {
        if (edges.size() >= maxNumCenters) break;
        int i = ovals[index].first;
        double flux = catalog->flux[i];
        // Get HFR
        sep_flux_radius(&im, catalog->x[i], catalog->y[i], maxRadius, 5, 0, &flux, requested_frac, 2, flux_fractions, &flux_flag);

        auto * center = new Edge();
        center->x = catalog->x[i] + x + 0.5;
        center->y = catalog->y[i] + y + 0.5;
        center->val = catalog->peak[i];
        center->sum = flux;
        center->numPixels = catalog->npix[i];
        center->HFR = center->width = flux_fractions[0];
        if (flux_fractions[1] < maxRadius)
            center->width = flux_fractions[1] * 2;
        edges.append(center);
    }
#endif

    // Let's sort edges, starting with widest
    std::sort(edges.begin(), edges.end(), [](const Edge * edge1, const Edge * edge2) -> bool { return edge1->HFR > edge2->HFR;});

    // Take only the first maxNumCenters stars
    {
        int starCount = qMin(maxNumCenters, edges.count());
        for (int i = 0; i < starCount; i++)
            starCenters.append(edges[i]);
    }

    edges.clear();

#ifndef HAVE_STELLARSOLVER
    qCDebug(KSTARS_FITS) << QString("Sky background: global %1 rms %2 cell ht %3 wd %4")
                         .arg(QString::number(bkg->global, 'f', 2))
                         .arg(QString::number(bkg->globalrms, 'f', 2))
                         .arg(bkg->bh).arg(bkg->bw);
    qCDebug(KSTARS_FITS) << qSetFieldWidth(10) << "#" << "#X" << "#Y" << "#Flux" << "#Width" << "sum" << "numpix" << "#HFR";
    for (int i = 0; i < starCenters.count(); i++)
        qCDebug(KSTARS_FITS) << qSetFieldWidth(10) << i << starCenters[i]->x << starCenters[i]->y
                             << starCenters[i]->sum << starCenters[i]->width << starCenters[i]->sum
                             << starCenters[i]->numPixels << starCenters[i]->HFR;



exit:
    delete[] data;
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
#endif
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

    FITSImage::Statistic const &stats = data->getStatistics();

    for (int y1 = y; y1 < y2; y1++)
    {
        int offset = y1 * stats.width;
        for (int x1 = x; x1 < x2; x1++)
        {
            *floatPtr++ = rawBuffer[offset + x1];
        }
    }
}

SkyBackground::SkyBackground(double mean_, double sigma_, double numPixels_)
{
    initialize(mean_, sigma_, numPixels_);
}

void SkyBackground::initialize(double mean_, double sigma_,
                               double numPixelsInSkyEstimate_, int numStars_)
{
    mean = mean_;
    sigma = sigma_;
    numPixelsInSkyEstimate = numPixelsInSkyEstimate_;
    varSky = sigma_ * sigma_;
    starsDetected = numStars_;
}

// Taken from: http://www1.phys.vt.edu/~jhs/phys3154/snr20040108.pdf
double SkyBackground::SNR(double flux, double numPixels, double gain)
{
    if (numPixelsInSkyEstimate <= 0 || gain <= 0)
        return 0;
    // const double numer = flux - numPixels * mean;
    const double numer = flux;  // It seems SEP flux subtracts out the background.
    const double denom = sqrt( numer / gain + numPixels * varSky  * (1 + 1 / numPixelsInSkyEstimate));
    if (denom <= 0)
        return 0;
    return numer / denom;

}
