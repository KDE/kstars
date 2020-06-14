/*  Correspondence class.
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QObject>
#include <QList>
#include <QVector3D>

#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitssepdetector.h"
#include "starcorrespondence.h"
#include "vect.h"
#include "matr.h"

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
 * guideStars.selectGuideStar(FITSdata*);
 * Selects a guide star, given the input image. This can be done at the start of
 * calibration, when guiding starts, or when the guide star is lost.
 *
 * Vector xy = guideStars.findGuideStar(FITSdata*, QRect trackingBox)
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
        QVector3D selectGuideStar(FITSData *imageData);

        // Finds the guide star previously selected with selectGuideStar()
        // in a new image. This sets up internal structures for getDrift().
        Vector findGuideStar(FITSData *imageData, const QRect &trackingBox);

        // Finds the drift of the star positions in arc-seconds for RA and DEC.
        // Must be called after findGuideStar().
        // Returns false if this can't be calculated.
        // setCalibration must have been called once before this (so that pixel
        // positions can be converted to RA and DEC).
        bool getDrift(double oneStarDrift, double reticle_x, double reticle_y,
                      double *RADrift, double *DECDrift);

        // Set calibration angle (degrees), binning (e.g. 2 for 2x2 binning),
        // pixel_size (mm) and focal_length (mm).
        void setCalibration(double angle_, int binning_,
                            double pixel_size_, double focal_length_);

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

    private:
        // Used to initialize the StarCorrespondence object, which ultimately finds
        // the guidestar using the geometry between it and the other stars detected.
        void setupStarCorrespondence(const QList<Edge> &neighbors, int guideIndex);

        // Evaluates which stars are desirable as guide stars and reference stars.
        void evaluateSEPStars(const QList<Edge *> &starCenters, QVector<double> *scores, const QRect *roi) const;

        // Prepares parameters for evaluateSEPStars().
        SSolver::Parameters getStarExtractionParameters(int num);

        // Returns the top num stars according to the evaluateSEPStars criteria.
        void findTopStars(FITSData *imageData, int num, QList<Edge> *stars, const QRect *roi = nullptr,
                          QList<double> *outputScores = nullptr);
        // The interface to the SEP star detection algoritms.
        int findAllSEPStars(FITSData *imageData, QList<Edge*> *sepStars, int num);

        // Convert from input image coordinates to output RA and DEC coordinates.
        Vector point2arcsec(const Vector &p) const;

        // Returns the RA and DEC distance between the star and the reference star.
        void computeStarDrift(const Edge &star, const Edge &reference,
                              double *driftRA, double *driftDEC) const;
        // Selects the guide star given the star detections and score generated
        // by evaluateSEPStars().
        QVector3D selectGuideStar(const QList<Edge> &detectedStars,
                                  const QList<double> &sepScores,
                                  int maxX, int maxY);
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

        // Sky background value generated by the SEP processing.
        // Used to calculate star SNR values.
        SkyBackground skyBackground;

        // Helpers to find the guide star.
        // The list of reference stars used by starCorrespondence.
        QList<Edge> guideStarNeighbors;
        // The index of the guide star in guideStarNeighbors.
        int guideStarNeighborIndex = 0;
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

        // calibration
        Ekos::Matrix ROT_Z;
        double calibration_angle = 0;
        int binning {1};
        double pixel_size {0};
        double focal_length {0};
        // Calibration must be initilized before drift can be calculated.
        bool calibrationInitialized {false};

        friend class TestGuideStars;
};
