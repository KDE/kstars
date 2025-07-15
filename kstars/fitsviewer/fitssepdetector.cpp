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
#include "fitsdata.h"
#include "Options.h"
#include "kspaths.h"

#include <memory>
#include <math.h>
#include <QtConcurrent>

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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QtConcurrent::run(&FITSSEPDetector::findSourcesAndBackground, this, boundary);
#else
    return QtConcurrent::run(this, &FITSSEPDetector::findSourcesAndBackground, boundary);
#endif
}

bool FITSSEPDetector::findSourcesAndBackground(QRect const &boundary)
{
#ifndef HAVE_STELLARSOLVER
    Q_UNUSED(boundary)
    return false;
#else
    QList<Edge*> starCenters;
    SkyBackground skyBG;
    int maxStarsCount = getValue("maxStarsCount", 100000).toInt();

    int optionsProfileIndex = getValue("optionsProfileIndex", -1).toInt();
    Ekos::ProfileGroup group = static_cast<Ekos::ProfileGroup>(getValue("optionsProfileGroup", 1).toInt());
    m_Solver.reset(new StellarSolver(m_ImageData->getStatistics(), m_ImageData->getImageBuffer()));

    QString filename = "";
    QPointer<FITSData> image(m_ImageData);
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

    QString savedOptionsProfiles = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(filename);
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
        auto params = optionsList[optionsProfileIndex];
        params.partition = Options::stellarSolverPartition();
        m_Solver->setParameters(params);
        qCDebug(KSTARS_FITS) << "Sextract with: " << optionsList[optionsProfileIndex].listName;
    }
    else
    {
        auto params = SSolver::Parameters();  // This is default
        params.partition = Options::stellarSolverPartition();
        m_Solver->setParameters(params);
    }

    QList<FITSImage::Star> stars;
    const bool runHFR = group != Ekos::AlignProfiles;

    m_Solver->setLogLevel(SSolver::LOG_NONE);
    m_Solver->setSSLogLevel(SSolver::LOG_OFF);

    if (boundary.isValid())
        m_Solver->extract(runHFR, boundary);
    else
        m_Solver->extract(runHFR);

    stars = m_Solver->getStarList();

    // If m_ImageData goes out of scope, also return.
    if (stars.empty() || image.isNull())
        return false;

    auto bg = m_Solver->getBackground();

    skyBG.mean = bg.global;
    skyBG.sigma = bg.globalrms;
    skyBG.numPixelsInSkyEstimate = bg.bw * bg.bh;
    skyBG.setStarsDetected(bg.num_stars_detected);
    m_ImageData->setSkyBackground(skyBG);

    //There is more information that can be obtained by the Stellarsolver->
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
        oneEdge->numPixels = stars[i].numPixels;
        if (stars[i].a > 0)
            // See page 63 to find the ellipticity equation for SEP.
            // http://astroa.physics.metu.edu.tr/MANUALS/sextractor/Guide2source_extractor.pdf
            oneEdge->ellipticity = 1 - stars[i].b / stars[i].a;
        else
            oneEdge->ellipticity = 0;

        starCenters.append(oneEdge);
    }
    m_ImageData->setStarCenters(starCenters);
    return true;
#endif
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

void FITSSEPDetector::abort()
{
    if (m_Solver)
        m_Solver->abort();
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
