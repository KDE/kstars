/*
    SPDX-FileCopyrightText: 2023

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include "../fitsviewer/fitsstardetector.h"
#include "fitsviewer/fitsview.h"
#include "fitsviewer/fitsdata.h"
#include "curvefit.h"
#include "../ekos.h"
#include <ekos_focus_debug.h>


namespace Ekos
{

// This class performs FWHM processing on the passed in FITS image

class FocusFWHM
{
    public:

        FocusFWHM(Mathematics::RobustStatistics::ScaleCalculation scaleCalc);
        ~FocusFWHM();

        template <typename T>
        void processFWHM(const T &imageBuffer, const QList<Edge *> &focusStars, const QSharedPointer<FITSData> &imageData,
                         std::unique_ptr<CurveFitting> &starFitting,
                         double *FWHM, double *weight)
        {
            CurveFitting::StarParams starParams, starParams2;
            std::vector<double> FWHMs, R2s;

            auto skyBackground = imageData->getSkyBackground();
            auto stats = imageData->getStatistics();

            // Setup a vector for each of the stars to be processed
            QVector<StarBox> stars;
            StarBox star;

            for (int s = 0; s < focusStars.size(); s++)
            {
                int starSize = focusStars[s]->numPixels;

                // If the star size is invalid then ignore this star
                if (starSize <= 0)
                    continue;

                // factor scales a box around the star to use in fitting the Gaussian
                // too big and it wastes processing resource and since there is no deblending, it will also result
                // in more star exclusions. Too small and the gaussian won't fit properly. On the Simulator 1.4 is good.
                constexpr double factor = 1.4;
                int width = factor * 2 * sqrt(starSize / M_PI);
                int height = width;

                // Width x height box centred on star centroid
                star.star = s;
                star.isValid = true;
                star.start.first = focusStars[s]->x - width / 2.0;
                star.end.first = focusStars[s]->x + width / 2.0;
                star.start.second = focusStars[s]->y - height / 2.0;
                star.end.second = focusStars[s]->y + height / 2.0;

                // Check star box does not go over image edge, drop star if so
                if (star.start.first < 0 || star.end.first > stats.width || star.start.second < 0 || star.end.second > stats.height)
                    continue;

                stars.push_back(star);
            }

            // Ideally we would deblend where another star encroaches into this star's box
            // For now we'll just exclude stars in this situation by marking isValid as false
            for (int s1 = 0; s1 < stars.size(); s1++)
            {
                if (!stars[s1].isValid)
                    continue;

                for (int s2 = s1 + 1; s2 < stars.size(); s2++)
                {
                    if (!stars[s2].isValid)
                        continue;

                    if (boxOverlap(stars[s1].start, stars[s1].end, stars[s2].start, stars[s2].end))
                    {
                        stars[s1].isValid = false;
                        stars[s2].isValid = false;
                    }
                }
            }

            // We have the list of stars to process now so iterate through the list fitting a curve, etc
            for (int s = 0; s < stars.size(); s++)
            {
                if (!stars[s].isValid)
                    continue;

                starParams.background = skyBackground.mean;
                starParams.peak = focusStars[stars[s].star]->val;
                starParams.centroid_x = focusStars[stars[s].star]->x - stars[s].start.first;
                starParams.centroid_y = focusStars[stars[s].star]->y - stars[s].start.second;
                starParams.HFR = focusStars[stars[s].star]->HFR;
                starParams.theta = 0.0;
                starParams.FWHMx = -1;
                starParams.FWHMy = -1;
                starParams.FWHM = -1;

                starFitting->fitCurve3D(imageBuffer, stats.width, stars[s].start, stars[s].end, starParams, CurveFitting::FOCUS_GAUSSIAN,
                                        false);
                if (starFitting->getStarParams(CurveFitting::FOCUS_GAUSSIAN, &starParams2))
                {
                    starParams2.centroid_x += stars[s].start.first;
                    starParams2.centroid_y += stars[s].start.second;
                    double R2 = starFitting->calculateR2(CurveFitting::FOCUS_GAUSSIAN);
                    if (R2 >= 0.25)
                    {
                        // Filter stars - 0.25 works OK on Sim
                        FWHMs.push_back(starParams2.FWHM);
                        R2s.push_back(R2);

                        qCDebug(KSTARS_EKOS_FOCUS) << "Star" << s << " R2=" << R2
                                                   << " x=" << focusStars[s]->x << " vs " << starParams2.centroid_x
                                                   << " y=" << focusStars[s]->y << " vs " << starParams2.centroid_y
                                                   << " HFR=" << focusStars[s]->HFR << " FWHM=" << starParams2.FWHM
                                                   << " Background=" << skyBackground.mean << " vs " << starParams2.background
                                                   << " Peak=" << focusStars[s]->val << "vs" << starParams2.peak;
                    }
                }
            }

            if (FWHMs.size() == 0)
            {
                *FWHM = INVALID_STAR_MEASURE;
                *weight = 0.0;
            }
            else
            {
                // There are many ways to compute a robust location. Using data on the simulator there wasn't much to choose
                // between the Location measures available in RobustStatistics, so will use basic 2 Sigma Clipping
                double FWHM_sc = Mathematics::RobustStatistics::ComputeLocation(Mathematics::RobustStatistics::LOCATION_SIGMACLIPPING,
                                 FWHMs, 2.0);
                double R2_median = Mathematics::RobustStatistics::ComputeLocation(Mathematics::RobustStatistics::LOCATION_MEDIAN,
                                   R2s, 2.0);
                double FWHMweight = Mathematics::RobustStatistics::ComputeWeight(m_ScaleCalc, FWHMs);

                qCDebug(KSTARS_EKOS_FOCUS) << "Original Stars=" << focusStars.size()
                                           << " Processed=" << stars.size()
                                           << " Solved=" << FWHMs.size()
                                           << " R2 min/max/median=" << *std::min_element(R2s.begin(), R2s.end())
                                           << "/" << *std::max_element(R2s.begin(), R2s.end())
                                           << "/" << R2_median
                                           << " FWHM=" << FWHM_sc
                                           << " Weight=" << FWHMweight;

                *FWHM = FWHM_sc;
                *weight = FWHMweight;
            }
        }

        static double constexpr INVALID_STAR_MEASURE = -1.0;

    private:

        bool boxOverlap(const QPair<int, int> b1Start, const QPair<int, int> b1End, const QPair<int, int> b2Start,
                        const QPair<int, int> b2End);

        // Structure to hold parameters for box around a star for FWHM calcs
        struct StarBox
        {
            int star;
            bool isValid;
            QPair<int, int> start; // top left of box. x = first element, y = second element
            QPair<int, int> end; // bottom right of box. x = first element, y = second element
        };

        Mathematics::RobustStatistics::ScaleCalculation m_ScaleCalc;
};
}
