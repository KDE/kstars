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
#include "indi/indimount.h"

class Calibration
{
    public:
        Calibration();
        ~Calibration() {}

        // Initialize the parameters needed to convert from pixels to arc-seconds,
        // the current pier side, and the current pointing position.
        void setParameters(double ccd_pix_width, double ccd_pix_height, double focalLengthMm,
                           int binX, int binY, ISD::Mount::PierSide pierSide,
                           const dms &mountRA, const dms &mountDec);

        // Set the current binning, which may be different from what was used during calibration.
        void setBinningUsed(int x, int y);

        // The guide camera and optics a calibration belongs to. Binning is not part of it: a calibration
        // can be used with another binning (see setBinningUsed()). Zero or empty values are unknown.
        struct Optics
        {
            QString camera;
            int chipWidth { 0 };
            int chipHeight { 0 };
            // Pixel size in millimeters, as in setParameters()
            double pixelWidthMm { 0 };
            double pixelHeightMm { 0 };
            double focalLengthMm { 0 };
        };

        // Record the guide camera of a new calibration, next to setParameters().
        void setCamera(const QString &camera, int chipWidth, int chipHeight);

        // The camera and optics of this calibration.
        Optics optics() const;

        // Returns an empty string if this calibration can be used with the current camera and optics,
        // otherwise a description of what differs. Values unknown on either side are not compared, so
        // calibrations saved before the camera was recorded are only checked by pixel size and focal length.
        QString opticsMismatch(const Optics &current) const;

        // Generate new calibrations according to the input parameters.
        bool calculate1D(double start_x, double start_y,
                         double end_x, double end_y, int RATotalPulse);

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
        // RA by one arcsecond.
        double raPulseMillisecondsPerArcsecond() const;

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
        // If current is given, the calibration is not restored when it was made with another camera or
        // other optics (see opticsMismatch()); mismatch then receives the reason.
        bool restore(ISD::Mount::PierSide currentPierSide, bool reverseDecOnPierChange,
                     int currentBinX, int currentBinY,
                     const dms *declination = nullptr,
                     const Optics *current = nullptr, QString *mismatch = nullptr);
        // As above, but for testing.
        bool restore(const QString &encoding, ISD::Mount::PierSide currentPierSide,
                     bool reverseDecOnPierChange, int currentBinX, int currentBinY,
                     const dms *declination = nullptr,
                     const Optics *current = nullptr, QString *mismatch = nullptr);

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
        // Internal calibration methods.
        bool calculate1D(double dx, double dy, int RATotalPulse);
        bool calculate2D(double ra_dx, double ra_dy, double dec_dx, double dec_dy,
                         bool *swap_dec, int RATotalPulse, int DETotalPulse);

        // Serialize and restore the calibration state.
        QString serialize() const;
        bool restore(const QString &encoding);

        // Adjusts the RA rate, according to the calibration and current declination values.
        double correctRA(double raMsPerPixel, const dms &calibrationDec, const dms &currentDec);

        // Compute a rotation angle given pixel change coordinates
        static double calculateRotation(double x, double y);

        // Set the rotation angle and the rotation matrix.
        // The angles are in the arc-second coordinate system (not the pixels--though that would
        // be the same thing if the pixels were square).
        void setAngle(double rotationAngle);

        // Set the calibrated ra and dec rates.
        void setRaPulseMsPerArcsecond(double rate);
        void setDecPulseMsPerArcsecond(double rate);

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

        // The guide camera and its chip size (unbinned) during calibration. Empty / 0 if unknown.
        QString calibrationCamera;
        int calibrationChipWidth { 0 };
        int calibrationChipHeight { 0 };

        // This angle is the one in use for calibrating. It may differ from the
        // calibrationAngle below if the pier side changes.
        double angle { 0 };

        // The rotation matrix that converts between pixel coordinates and RA/DEC.
        // This is derived from angle in setAngle().
        GuiderUtils::Matrix ROT_Z;

        // The angles associated with the calibration that was computed or
        // restored. They won't change as we change pier sides.
        double calibrationAngle { 0 };
        double calibrationAngleRA = 0;
        double calibrationAngleDEC = 0;

        // The calibrated values of how many pulse milliseconds are required to
        // move one arcsecond in RA and DEC.
        double raPulseMsPerArcsecond { 0 };
        double decPulseMsPerArcsecond { 0 };

        // The decSwap that was computed in calibration.
        bool calibrationDecSwap { false };

        // The decSwap in use. May be the opposite of calibrationDecSwap if the calibration
        // is being used on the opposite pier side as the calibration pier side.
        bool decSwap { false };

        // The RA and DEC when calibration was performed. For reference. Not currently used.
        dms calibrationRA;
        dms calibrationDEC;

        // The side of the pier where the current calibration was calculated.
        ISD::Mount::PierSide calibrationPierSide { ISD::Mount::PIER_UNKNOWN };

        bool initialized { false };
        friend class TestGuideStars;
};

