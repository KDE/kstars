/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QList>
#include <QVector3D>

#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitssepdetector.h"
#include "starcorrespondence.h"
#include "vect.h"
#include "../guideview.h"
#include "calibration.h"

namespace SSolver
{
class Parameters;
}

/*
 * This class manages selecting a guide star, finding that star in a new image
 * and calculating the drift of the new star image.
 * The major methods are:
 *
 * GuideStars guideStars();
 * guideStars.selectGuideStar(QSharedPointer<FITSdata>);
 * Selects a guide star, given the input image. This can be done at the start of
 * calibration, when guiding starts, or when the guide star is lost.
 *
 * Vector xy = guideStars.findGuideStar(QSharedPointer<FITSdata>, QRect trackingBox)
 * Finds the guide star that was previously selected. xy will contain the pixel coordinates
 * {x,y,0}. The tracking box is not enforced if the multi-star star-finding algorithm
 * is used, however, if that fails, it backs off to the star with the best score
 * (basically the brightest star) in the tracking box.
 *
 * bool success = guideStars.getDrift(guideStarDrift,  reticle_x, reticle_y, RADrift, DECDrift)
 * Returns the star movement in RA and DEC. The reticle can be input indicating
 * that the desired position for the original guide star and reference stars has
 * shifted (e.g. dithering).
 */

class GuideStars
{
    public:
        GuideStars();
        ~GuideStars() {}

        // Select a guide star, given the image.
        // Performs a SEP processing to detect stars, then finds the
        // most desirable guide star.
        QVector3D selectGuideStar(const QSharedPointer<FITSData> &imageData);

        // Finds the guide star previously selected with selectGuideStar()
        // in a new image. This sets up internal structures for getDrift().
        GuiderUtils::Vector findGuideStar(const QSharedPointer<FITSData> &imageData, const QRect &trackingBox,
                                          GuideView *guideView = nullptr);

        // Finds the drift of the star positions in arc-seconds for RA and DEC.
        // Must be called after findGuideStar().
        // Returns false if this can't be calculated.
        // setCalibration must have been called once before this (so that pixel
        // positions can be converted to RA and DEC).
        bool getDrift(double oneStarDrift, double reticle_x, double reticle_y,
                      double *RADrift, double *DECDrift);

        // Use this calibration object for conversions to arcseconds and RA/DEC.
        void setCalibration(const Calibration &calibration);

        // Returns the sky background object that was obtained from SEP analysis.
        const SkyBackground &skybackground() const
        {
            return skyBackground;
        }
        double getGuideStarMass() const
        {
            return guideStarMass;
        }
        double getGuideStarSNR() const
        {
            return guideStarSNR;
        }

        void reset()
        {
            starCorrespondence.reset();
        }

    private:
        // Used to initialize the StarCorrespondence object, which ultimately finds
        // the guidestar using the geometry between it and the other stars detected.
        void setupStarCorrespondence(const QList<Edge> &neighbors, int guideIndex);

        // Evaluates which stars are desirable as guide stars and reference stars.
        void evaluateSEPStars(const QList<Edge *> &starCenters, QVector<double> *scores,
                              const QRect *roi, const double maxHFR) const;

        // Prepares parameters for evaluateSEPStars().
        SSolver::Parameters getStarExtractionParameters(int num);

        // Returns the top num stars according to the evaluateSEPStars criteria.
        void findTopStars(const QSharedPointer<FITSData> &imageData, int num, QList<Edge> *stars,
                          const double maxHFR,
                          const QRect *roi = nullptr,
                          QList<double> *outputScores = nullptr,
                          QList<double> *minDistances = nullptr);
        // The interface to the SEP star detection algoritms.
        int findAllSEPStars(const QSharedPointer<FITSData> &imageData, QList<Edge*> *sepStars, int num);

        // Convert from input image coordinates to output RA and DEC coordinates.
        GuiderUtils::Vector point2arcsec(const GuiderUtils::Vector &p) const;

        // Returns the RA and DEC distance between the star and the reference star.
        void computeStarDrift(const Edge &star, const Edge &reference,
                              double *driftRA, double *driftDEC) const;
        // Selects the guide star given the star detections and score generated
        // by evaluateSEPStars().
        QVector3D selectGuideStar(const QList<Edge> &detectedStars,
                                  const QList<double> &sepScores,
                                  int maxX, int maxY,
                                  const QList<double> &minDistances);

        // Computes the distance from stars[i] to its closest neighbor.
        double findMinDistance(int index, const QList<Edge*> &stars);

        // Plot the positions of the neighbor stars on the guideView display.
        void plotStars(GuideView *guideView, const QRect &trackingBox);

        // These three methods are useful for testing.
        void setDetectedStars(const QList<Edge> &stars)
        {
            detectedStars = stars;
        };
        void setSkyBackground(const SkyBackground &background)
        {
            skyBackground = background;
        }
        void setStarMap(const QVector<int> &map)
        {
            starMap = map;
        }
        int getStarMap(int index);

        // Sky background value generated by the SEP processing.
        // Used to calculate star SNR values.
        SkyBackground skyBackground;

        // Used to find the guide star in a new set of image detections.
        StarCorrespondence starCorrespondence;

        // These are set when the guide star is detected, and can be queried, e.g.
        // for logging.
        double guideStarMass = 0;
        double guideStarSNR = 0;

        // The newly detected stars.
        QVector<int> starMap;
        // This maps between the newly detected stars and the reference stars.
        QList<Edge> detectedStars;

        Calibration calibration;
        bool calibrationInitialized {false};

        // Find guide star will allow robust star correspondence.
        bool allowMissingGuideStar { true };

        // counts consecutive missed guide stars.
        int missedGuideStars { 0 };

        friend class TestGuideStars;
};
