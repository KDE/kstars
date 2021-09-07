/*  Gaussian Process Guider support class.
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "vect.h"
#include "indi/indicommon.h"
#include "MPI_IS_gaussian_process/src/gaussian_process_guider.h"

class GuideStars;
class GaussianProcessGuider;
class Calibration;

// This is a wrapper class around the GaussianProcessGuider contributed class
// to make integration with EKos easier.
class GPG
{
    public:
        GPG();
        ~GPG() {}

        // Reads parameters from Options, and updates the GPG.
        void updateParameters();

        // Restarts the gpg.
        void reset();

        // Should be called when dithering starts.
        // Inputs are pixel offsets in camera coordinates.
        void startDithering(double dx, double dy, const Calibration &cal);

        // Should be called after dithering is done.
        // Indicated whether dithering settled or not.
        void ditheringSettled(bool success);

        // Should be called while suspended, at the point when
        // guiding would normally occur. GPG gets updated but does not
        // emit a pulse.
        void suspended(const GuiderUtils::Vector &guideStarPosition,
                       const GuiderUtils::Vector &reticlePosition,
                       GuideStars *guideStars,
                       const Calibration &cal);

        // Compute the RA pulse for guiding.
        // Returns false if it chooses not to compute a pulse.
        bool computePulse(double raArcsecError, GuideStars *guideStars,
                          int *pulseLength, GuideDirection *pulseDir,
                          const Calibration &cal);

    private:
        std::unique_ptr<GaussianProcessGuider> gpg;
        int gpgSamples = 0;
        int gpgSkippedSamples = 0;
};
