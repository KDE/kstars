/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#include "config-kstars.h"

#include "fits_debug.h"
#include "fitssepdetector.h"
#include "Options.h"
#include "kspaths.h"

#include <math.h>
#include <QPointer>
#include <QtConcurrent>

#ifdef HAVE_STELLARSOLVER
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#include <stellarsolver.h>
#else
#include <cstring>
#include "sep/sep.h"
#endif

//void FITSSEPDetector::configure(const QString &param, const QVariant &value)
//{
//    if (param == "numStars")
//        numStars = value.toInt();
//    else if (param == "fractionRemoved")
//        fractionRemoved = value.toDouble();
//    else if (param == "deblendNThresh")
//        deblendNThresh = value.toInt();
//    else if (param == "deblendMincont")
//        deblendMincont = value.toDouble();
//    else if (param == "radiusIsBoundary")
//        radiusIsBoundary = value.toBool();
//    else
//        qCDebug(KSTARS_FITS) << "Bad SEP Parameter!!!!! " << param;
//}

// TODO: (hy 4/11/2020)
// The api into these star detection methods should be generalized so that various parameters
// could be controlled by the caller. For instance, saturation removal, removal of N% of the largest stars
// number of stars desired, some parameters to the SEP algorithms, as all of these may have different
// optimal values for different uses. Also, there may be other desired outputs
// (e.g. unfiltered number of stars detected,background sky level). Waiting on rlancaste's
// investigations into SEP before doing this.

QFuture<bool> FITSSEPDetector::findSources(QRect const &boundary)
{
    return QtConcurrent::run(this, &FITSSEPDetector::findSourcesAndBackground, boundary);
}

bool FITSSEPDetector::findSourcesAndBackground(QRect const &boundary)
{
    QList<Edge*> starCenters;
    SkyBackground skyBG;
    int maxStarsCount = getValue("maxStarsCount", 100000).toInt();
#ifdef HAVE_STELLARSOLVER
    int optionsProfileIndex = getValue("optionsProfileIndex", -1).toInt();
    Ekos::ProfileGroup group = static_cast<Ekos::ProfileGroup>(getValue("optionsProfileGroup", 1).toInt());

    QPointer<StellarSolver> solver = new StellarSolver(m_ImageData->getStatistics(), m_ImageData->getImageBuffer());
    QString filename = "";
    switch(group)
    {
        case Ekos::AlignProfiles:
            //So it should not be here if it is Align.
            break;
        case Ekos::GuideProfiles:
            filename = "SavedGuideProfiles.ini";
            break;
        case Ekos::FocusProfiles:
            filename = "SavedFocusProfiles.ini";
            break;
        case Ekos::HFRProfiles:
            filename = "SavedHFRProfiles.ini";
            break;
    }

    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(filename);
    QList<SSolver::Parameters> optionsList;
    if(QFile(savedOptionsProfiles).exists())
        optionsList = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
    {
        switch(group)
        {
            case Ekos::AlignProfiles:
                optionsList = Ekos::getDefaultAlignOptionsProfiles();
                break;
            case Ekos::GuideProfiles:
                optionsList = Ekos::getDefaultGuideOptionsProfiles();
                break;
            case Ekos::FocusProfiles:
                optionsList = Ekos::getDefaultFocusOptionsProfiles();
                break;
            case Ekos::HFRProfiles:
                optionsList = Ekos::getDefaultHFROptionsProfiles();
                break;
        }
    }
    if (optionsProfileIndex >= 0 && optionsList.count() > optionsProfileIndex)
    {
        solver->setParameters(optionsList[optionsProfileIndex]);
        qCDebug(KSTARS_FITS) << "Sextract with: " << optionsList[optionsProfileIndex].listName;
    }
    else
        solver->setParameters(SSolver::Parameters()); // This is default

    // Wait synchronously

    QEventLoop loop;
    connect(solver, &StellarSolver::finished, &loop, &QEventLoop::quit);
    QList<FITSImage::Star> stars;
    const bool runHFR = group != Ekos::AlignProfiles;

    if (boundary.isValid())
        solver->extract(runHFR, boundary);
    else
        solver->extract(runHFR);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    stars = solver->getStarList();

    if (stars.empty())
        return false;

    auto bg = solver->getBackground();

    skyBG.mean = bg.global;
    skyBG.sigma = bg.globalrms;
    skyBG.numPixelsInSkyEstimate = bg.bw * bg.bh;
    skyBG.setStarsDetected(bg.num_stars_detected);
    m_ImageData->setSkyBackground(skyBG);

    //There is more information that can be obtained by the Stellarsolver.
    //Background info, Star positions(if a plate solve was done before), etc
    //The information is available as long as the StellarSolver exists.

    // Let's sort edges, starting with widest
    if (runHFR)
        std::sort(stars.begin(), stars.end(), [](const FITSImage::Star & star1, const FITSImage::Star & star2) -> bool { return star1.HFR > star2.HFR;});
    else
        std::sort(stars.begin(), stars.end(), [](const FITSImage::Star & star1, const FITSImage::Star & star2) -> bool { return star1.flux > star2.flux;});


    // Take only the first maxNumCenters stars
    int starCount = qMin(maxStarsCount, stars.count());
    starCenters.reserve(starCount);
    for (int i = 0; i < starCount; i++)
    {
        Edge *oneEdge = new Edge();
        oneEdge->x = stars[i].x;
        oneEdge->y = stars[i].y;
        oneEdge->val = stars[i].peak;
        oneEdge->sum = stars[i].flux;
        oneEdge->HFR = stars[i].HFR;
        oneEdge->width = stars[i].a;
        if (stars[i].a > 0)
            // See page 63 to find the ellipticity equation for SEP.
            // http://astroa.physics.metu.edu.tr/MANUALS/sextractor/Guide2source_extractor.pdf
            oneEdge->ellipticity = 1 - stars[i].b / stars[i].a;
        else
            oneEdge->ellipticity = 0;

        starCenters.append(oneEdge);
    }
    m_ImageData->setStarCenters(starCenters);

#else

    FITSImage::Statistic const &stats = m_ImageData->getStatistics();

    int x = 0, y = 0, w = stats.width, h = stats.height, maxRadius = 50;
    std::vector<std::pair<int, double>> ovals;
    int maxNumCenters = getValue("maxStars", 50).toInt();

    // We may skip 20% of the stars (those with the largest 20% HFRs) as those are suspect
    // to be non-stars) if we have plenty of detections.
    int startIndex = 0;

    if (!boundary.isNull())
    {
        x = boundary.x();
        y = boundary.y();
        w = boundary.width();
        h = boundary.height();
        if (getValue("radiusIsBoundary", true).toBool())
            maxRadius = w;
    }

    auto * data = new float[w * h];

    switch (stats.dataType)
    {
        case TBYTE:
            getFloatBuffer<uint8_t>(data, x, y, w, h, m_ImageData);
            break;
        case TSHORT:
            getFloatBuffer<int16_t>(data, x, y, w, h, m_ImageData);
            break;
        case TUSHORT:
            getFloatBuffer<uint16_t>(data, x, y, w, h, m_ImageData);
            break;
        case TLONG:
            getFloatBuffer<int32_t>(data, x, y, w, h, m_ImageData);
            break;
        case TULONG:
            getFloatBuffer<uint32_t>(data, x, y, w, h, m_ImageData);
            break;
        case TFLOAT:
            if (boundary.isNull())
                memcpy(data, m_ImageData->getImageBuffer(), sizeof(float)*w * h);
            else
                getFloatBuffer<float>(data, x, y, w, h, m_ImageData);
            break;
        case TLONGLONG:
            getFloatBuffer<int64_t>(data, x, y, w, h, m_ImageData);
            break;
        case TDOUBLE:
            getFloatBuffer<double>(data, x, y, w, h, m_ImageData);
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

    auto cleanup = [ & ]()
    {
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
        }
    };

    // #0 Create SEP Image structure
    sep_image im = {data, nullptr, nullptr, SEP_TFLOAT, 0, 0, w, h, 0.0, SEP_NOISE_NONE, 1.0, 0.0};

    // #1 Background estimate
    status = sep_background(&im, 64, 64, 3, 3, 0.0, &bkg);
    if (status != 0)
    {
        cleanup();
        return false;
    }

    // #2 Background evaluation
    imback = (float *)malloc((w * h) * sizeof(float));
    status = sep_bkg_array(bkg, imback, SEP_TFLOAT);
    if (status != 0)
    {
        cleanup();
        return false;
    }

    skyBG.initialize(bkg->global, bkg->globalrms, bkg->bh * bkg->bw);

    // #3 Background subtraction
    status = sep_bkg_subarray(bkg, im.data, im.dtype);
    if (status != 0)
    {
        cleanup();
        return false;
    }

    // #4 Source Extraction
    status = sep_extract(&im, 2 * bkg->globalrms, SEP_THRESH_ABS, 10, conv, 3, 3, SEP_FILTER_CONV,
                         getValue("deblendNThresh", 32).toInt(),
                         getValue("deblendMincont", 0.005).toDouble(),
                         1, 1.0, &catalog);
    if (status != 0)
    {
        cleanup();
        return false;
    }
    qCDebug(KSTARS_FITS) << "SEP detected " << catalog->nobj << " stars.";
    skyBG.setStarsDetected(catalog->nobj);

    double fractionRemoved = getValue("fractionRemoved", 0.2).toDouble();
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

    // Let's sort edges, starting with widest
    std::sort(edges.begin(), edges.end(), [](const Edge * edge1, const Edge * edge2) -> bool { return edge1->HFR > edge2->HFR;});

    // Take only the first maxNumCenters stars
    {
        int starCount = qMin(maxStarsCount, edges.count());
        for (int i = 0; i < starCount; i++)
            starCenters.append(edges[i]);

        m_ImageData->setStarCenters(starCenters);
    }

    edges.clear();

    qCDebug(KSTARS_FITS) << QString("Sky background: global %1 rms %2 cell ht %3 wd %4")
                         .arg(QString::number(bkg->global, 'f', 2))
                         .arg(QString::number(bkg->globalrms, 'f', 2))
                         .arg(bkg->bh).arg(bkg->bw);
    qCDebug(KSTARS_FITS) << qSetFieldWidth(10) << "#" << "#X" << "#Y" << "#Flux" << "#Width" << "sum" << "numpix" << "#HFR";
    for (int i = 0; i < starCenters.count(); i++)
        qCDebug(KSTARS_FITS) << qSetFieldWidth(10) << i << starCenters[i]->x << starCenters[i]->y
                             << starCenters[i]->sum << starCenters[i]->width << starCenters[i]->sum
                             << starCenters[i]->numPixels << starCenters[i]->HFR;
    m_ImageData->setStarCenters(starCenters);
    m_ImageData->setSkyBackground(skyBG);

    cleanup();
#endif
    return true;
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
double SkyBackground::SNR(double flux, double numPixels, double gain) const
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
