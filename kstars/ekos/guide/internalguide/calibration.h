/*  Calibration class.
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "matr.h"
#include "vect.h"

class Calibration
{
    public:
        Calibration();
        ~Calibration() {}

        // Initialize all the calibration parameters.
        // These come in scatter-shot into gmath.cpp
        // Should organize into one call if possible.
        void setParameters(double ccd_pix_width, double ccd_pix_height,
                           double focalLengthMm);
        void setBinning(int binX, int binY);
        void setAngle(double rotationAngle);
        void setRaPulseMsPerPixel(double rate);
        void setDecPulseMsPerPixel(double rate);

        // Prints the calibration parameters in the debug log.
        void logCalibration() const;

        double getFocalLength() const
        {
            return focalMm;
        }
        double getAngle() const
        {
            return angle;
        }
        Ekos::Matrix getRotation() const
        {
            return ROT_Z;
        }

        // Converts the input x & y coordinates from pixels to arc-seconds.
        // Does not rotate the input into RA/DEC.
        Vector convertToArcseconds(const Vector &input) const;

        // Converts the input x & y coordinates from arc-seconds to pixels.
        // Does not rotate the input into RA/DEC.
        Vector convertToPixels(const Vector &input) const;
        void convertToPixels(double xArcseconds, double yArcseconds,
                             double *xPixel, double *yPixel) const;


        // Given offsets, convert to RA and DEC coordinates
        // by rotating according to the calibration.
        // Also inverts the y-axis. Does not convert to arc-seconds.
        Vector rotateToRaDec(const Vector &input) const;
        void rotateToRaDec(double dx, double dy, double *ra, double *dec) const;

        // Returns the number of milliseconds that should be pulsed to move
        // RA by one pixel.
        double raPulseMillisecondsPerPixel() const;

        // Returns the number of milliseconds that should be pulsed to move
        // RA by one arcsecond.
        double raPulseMillisecondsPerArcsecond() const;

        // Returns the number of milliseconds that should be pulsed to move
        // DEC by one pixel.
        double decPulseMillisecondsPerPixel() const;

        // Returns the number of milliseconds that should be pulsed to move
        // DEC by one arcsecond.
        double decPulseMillisecondsPerArcsecond() const;

        // Returns the number of pixels per arc-second in X and Y and vica versa.
        double xPixelsPerArcsecond() const;
        double yPixelsPerArcsecond() const;
        double xArcsecondsPerPixel() const;
        double yArcsecondsPerPixel() const;

    private:
        // Compute a rotation angle given start and end pixel coordinates
        double calculateRotation(double start_x, double start_y,
                                 double end_x, double end_y) const;

        // Sub-binning in X and Y.
        int subBinX { 1 };
        int subBinY { 1 };

        // Pixel width mm, for each pixel,
        // Binning does not affect this.
        double ccd_pixel_width { 0.003 };
        double ccd_pixel_height { 0.003 };

        // Focal length in millimeters.
        double focalMm { 500 };

        Ekos::Matrix ROT_Z;
        double angle { 0 };

        double raPulseMsPerPixel;
        double decPulseMsPerPixel;
};

