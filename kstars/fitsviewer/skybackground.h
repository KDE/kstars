/***************************************************************************
                          fitssepdetector.h  -  FITS Image
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

#pragma once

class SkyBackground
{
    public:
        // Must call initialize() if using the default constructor;
        SkyBackground() {}
        SkyBackground(double m, double sig, double np);
        virtual ~SkyBackground() = default;

        // Mean of the background level (ADUs).
        double mean {0};
        // Standard deviation of the background level.
        double sigma {0};
        // Number of pixels used to estimate the background level.
        int numPixelsInSkyEstimate {0};

        // Number of stars detected in the sky. A relative measure of sky quality
        // (compared with the same part of the sky at a different time).
        int starsDetected {0};

        // Given a source with flux spread over numPixels, and a CCD with gain = ADU/#electron)
        // returns an SNR estimate.
        double SNR(double flux, double numPixels, double gain = 0.5) const;
        void initialize(double mean_, double sigma_, double numPixelsInSkyEstimate_, int numStars_ = 0);
        void setStarsDetected(int numStars)
        {
            starsDetected = numStars;
        }

    private:
        double varSky;
};

