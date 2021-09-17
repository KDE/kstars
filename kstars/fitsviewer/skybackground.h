/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

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
        double varSky = 0;
};

