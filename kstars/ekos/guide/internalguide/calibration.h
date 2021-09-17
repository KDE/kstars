/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "matr.h"
#include "vect.h"
#include <QString>
#include "ekos/ekos.h"
#include "indi/indicommon.h"
#include "indi/inditelescope.h"

class Calibration
{
    public:
        Calibration();
        ~Calibration() {}

        // Initialize the parameters needed to convert from pixels to arc-seconds,
        // the current pier side, and the current pointing position.
        void setParameters(double ccd_pix_width, double ccd_pix_height, double focalLengthMm,
                           int binX, int binY, ISD::Telescope::PierSide pierSide,
                           const dms &mountRA, const dms &mountDec);

        // Set the current binning, which may be different from what was used during calibration.
        void setBinningUsed(int x, int y);

        // Generate new calibrations according to the input parameters.
        bool calculate1D(double x, double y, int RATotalPulse);
        bool calculate1D(double start_x, double start_y,
                         double end_x, double end_y, int RATotalPulse);

        bool calculate2D(double ra_x, double ra_y, double dec_x, double dec_y,
                         bool *swap_dec, int RATotalPulse, int DETotalPulse);
        bool calculate2D(
            double start_ra_x, double start_ra_y, double end_ra_x, double end_ra_y,
            double start_dec_x, double start_dec_y, double end_dec_x, double end_dec_y,
            bool *swap_dec, int RATotalPulse, int DETotalPulse);

        // Computes the drift from the detection relative to the reference position.
        // If inputs are in pixels, then drift outputs are in pixels.
        // If inputs are in arcsecond coordinates then drifts are in arcseconds.
        void computeDrift(const GuiderUtils::Vector &detection, const GuiderUtils::Vector &reference,
                          double *raDrift, double *decDrift) const;

        double getFocalLength() const
        {
            return focalMm;
        }
        double getAngle() const
        {
            return angle;
        }
        double getRAAngle() const
        {
            return calibrationAngleRA;
        }
        double getDECAngle() const
        {
            return calibrationAngleDEC;
        }

        // Converts the input x & y coordinates from pixels to arc-seconds.
        // Does not rotate the input into RA/DEC.
        GuiderUtils::Vector convertToArcseconds(const GuiderUtils::Vector &input) const;

        // Converts the input x & y coordinates from arc-seconds to pixels.
        // Does not rotate the input into RA/DEC.
        GuiderUtils::Vector convertToPixels(const GuiderUtils::Vector &input) const;
        void convertToPixels(double xArcseconds, double yArcseconds,
                             double *xPixel, double *yPixel) const;

        // Given offsets, convert to RA and DEC coordinates
        // by rotating according to the calibration.
        // Also inverts the y-axis. Does not convert to arc-seconds.
        GuiderUtils::Vector rotateToRaDec(const GuiderUtils::Vector &input) const;
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

        // Save the calibration to Options.
        void save() const;
        // Restore the saved calibration. If the pier side is different than
        // when was calibrated, adjust the angle accordingly.
        bool restore(ISD::Telescope::PierSide currentPierSide, bool reverseDecOnPierChange,
                     int currentBinX, int currentBinY,
                     const dms *declination = nullptr);
        // As above, but for testing.
        bool restore(const QString &encoding, ISD::Telescope::PierSide currentPierSide,
                     bool reverseDecOnPierChange, int currentBinX, int currentBinY,
                     const dms *declination = nullptr);

        bool declinationSwapEnabled() const
        {
            return decSwap;
        }
        void setDeclinationSwapEnabled(bool value);

        void reset()
        {
            initialized = false;
        }
        // Returns true if calculate1D, calculate2D or restore have been called.
        bool isInitialized()
        {
            return initialized;
        }
        // Prints the calibration parameters in the debug log.
        void logCalibration() const;

    private:
        // Serialize and restore the calibration state.
        QString serialize() const;
        bool restore(const QString &encoding);

        // Adjusts the RA rate, according to the calibration and current declination values.
        double correctRA(double raMsPerPixel, const dms &calibrationDec, const dms &currentDec);

        // Compute a rotation angle given pixel change coordinates
        static double calculateRotation(double x, double y);

        // Set the rotation angle and the rotation matrix.
        void setAngle(double rotationAngle);

        void setRaPulseMsPerPixel(double rate);
        void setDecPulseMsPerPixel(double rate);

        // computes the ratio of the binning currently used to the binning in use while calibrating.
        double binFactor() const;
        // Inverse of above.
        double inverseBinFactor() const;

        // Sub-binning in X and Y.
        int subBinX { 1 };
        int subBinY { 1 };

        // It is possible that this calibration was done with one binning, but is now
        // being used with another binning. This is the current binning (as opposed to the above
        // which is the binning that was in-place during calibration.
        int subBinXused { 1 };
        int subBinYused { 1 };

        // Pixel width mm, for each pixel,
        // Binning does not affect this.
        double ccd_pixel_width { 0.003 };
        double ccd_pixel_height { 0.003 };

        // Focal length in millimeters.
        double focalMm { 500 };

        // This angle is the one in use for calibrating. It may differ from the
        // calibrationAngle below if the pier side changes.
        double angle { 0 };

        // The rotation matrix that converts between pixel coordinates and RA/DEC.
        // This is derived from angle in setAngle().
        GuiderUtils::Matrix ROT_Z;

        // The angles associated with the calibration that was computerd or
        // restored. They won't change as we change pier sides.
        double calibrationAngle { 0 };
        double calibrationAngleRA = 0;
        double calibrationAngleDEC = 0;

        // The calibrated values of how many pulse milliseconds are required to
        // move one pixel in RA and DEC.
        double raPulseMsPerPixel { 0 };
        double decPulseMsPerPixel { 0 };

        // The decSwap that was computed in calibration.
        bool calibrationDecSwap { false };

        // The decSwap in use. May be the opposite of calibrationDecSwap if the calibration
        // is being used on the opposite pier side as the calibration pier side.
        bool decSwap { false };

        // The RA and DEC when calibration was performed. For reference. Not currently used.
        dms calibrationRA;
        dms calibrationDEC;

        // The side of the pier where the current calibration was calculated.
        ISD::Telescope::PierSide calibrationPierSide { ISD::Telescope::PIER_UNKNOWN };

        bool initialized { false };
        friend class TestGuideStars;
};

