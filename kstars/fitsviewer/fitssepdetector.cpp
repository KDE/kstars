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
#include "sexysolver/sexysolver.h"
#include "sexysolver/structuredefinitions.h"

FITSStarDetector& FITSSEPDetector::configure(const QString &, const QVariant &)
{
    return *this;
}

// TODO: (hy 4/11/2020)
// The api into these star detection methods should be generalized so that various parameters
// could be controlled by the caller. For instance, saturation removal, removal of N% of the largest stars
// number of stars desired, some parameters to the SEP algorithms, as all of these may have different
// optimal values for different uses. Also, there may be other desired outputs
// (e.g. unfiltered number of stars detected,background sky level). Waiting on rlancaste's
// investigations into SEP before doing this.
int FITSSEPDetector::findSources(QList<Edge*> &starCenters, const QRect &boundary)
{
    FITSData const * const image_data = reinterpret_cast<FITSData const *>(parent());
    starCenters.clear();

    if (image_data == nullptr)
        return 0;

    constexpr int maxNumCenters = 50;

    SexySolver *solver = new SexySolver(image_data->getStatistics(),image_data->getImageBuffer(), parent());
    solver->setParameterProfile(ALL_STARS);
    if (!boundary.isNull())
        solver->setUseSubframe(boundary);
    solver->sextractWithHFR();
    if(!solver->sextractionDone() || solver->failed())
        return 0;
    QList<Star> stars = solver->getStarList();
    QList<Edge *> edges;

    for (int index = 0; index < stars.count(); index++)
    {
        Star star = stars.at(index);
        auto * center = new Edge();
        center->x = star.x;
        center->y = star.y;
        center->val = star.peak;
        center->sum = star.flux;
        center->HFR = star.HFR;
        edges.append(center);
    }

    // Let's sort edges, starting with widest
    std::sort(edges.begin(), edges.end(), [](const Edge * edge1, const Edge * edge2) -> bool { return edge1->HFR > edge2->HFR;});

    // Take only the first maxNumCenters stars
    {
        int starCount = qMin(maxNumCenters, edges.count());
        for (int i = 0; i < starCount; i++)
            starCenters.append(edges[i]);
    }

    edges.clear();

    qCDebug(KSTARS_FITS) << qSetFieldWidth(10) << "#" << "#X" << "#Y" << "#Flux" << "#Width" << "#HFR";
    for (int i = 0; i < starCenters.count(); i++)
        qCDebug(KSTARS_FITS) << qSetFieldWidth(10) << i << starCenters[i]->x << starCenters[i]->y
                             << starCenters[i]->sum << starCenters[i]->width << starCenters[i]->HFR;

    return starCenters.count();
}
