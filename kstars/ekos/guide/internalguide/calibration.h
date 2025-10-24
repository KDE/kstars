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

        void updateRotation(double CamRotation);

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
            return m_Current.FocalLength;
        }

        double getRestoredAngle() const
        {
            return m_Original.Angle;
        }
        double getRestoredRAAngle() const
        {
            return m_Original.AngleRA;
        }
        double getRestoredDECAngle() const
        {
            return m_Original.AngleDEC;
        }
        double getAngle() const
        {
            return m_Current.Angle;
        }
        double getRAAngle() const
        {
            return m_Current.AngleRA;
        }
        double getDECAngle() const
        {
            return m_Current.AngleDEC;
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
        // Does not convert to arc-seconds.
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
        void save();
        // Restore the saved calibration. If the pier side is different than
        // when was calibrated, adjust the angle accordingly.
        bool restore(ISD::Mount::PierSide currentPierSide, bool reverseDecOnPierChange,
                     int currentBinX, int currentBinY,
                     const dms declination);
        // As above, but for testing.
        bool restore(const QString &encoding, ISD::Mount::PierSide currentPierSide,
                     bool reverseDecOnPierChange, int currentBinX, int currentBinY,
                     const dms declination);

        bool declinationSwapEnabled() const
        {
            return m_Current.DecSwap;
        }
        void setDeclinationSwapEnabled(bool value);

        void reset()
        {
            m_initialized = false;
        }
        // Returns true if calculate1D, calculate2D or restore have been called.
        bool isInitialized()
        {
            return m_initialized;
        }
        // Prints the calibration parameters in the debug log.
        void logCalibration() const;

        double getDiffDEC(const dms currentDEC);
        void updateRAPulse(const dms currentDEC);
        void updatePierside(ISD::Mount::PierSide *Pierside, double *Rotation,
                            const bool FlipRotReady, const bool ManualRotatorOnly);

    private:
        // Internal calibration methods.
        bool calculate1D(double dx, double dy, int RATotalPulse);
        bool calculate2D(double ra_dx, double ra_dy, double dec_dx, double dec_dy,
                         bool *swap_dec, int RATotalPulse, int DETotalPulse);

        // Serialize and restore the calibration state.
        QString serialize() const;
        bool restore(const QString &encoding);

        // Adjusts the RA rate, according to the calibration and current declination values.
        double correctRA(double raMsPerPixel, const dms calibrationDec, const dms currentDec);

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



        // The rotation matrix that converts between pixel coordinates and RA/DEC.
        // This is derived from angle in setAngle().
        GuiderUtils::Matrix ROT_Z;

        struct Calibration_Parameters
        {
            int SubBinX { 1 };
            int SubBinY { 1 };
            double CCDPixelWidth { 0.003 };
            double CCDPixelHeight { 0.003 };
            double FocalLength { 500 };
            double Angle { 0 };
            double AngleRA { 0 };
            double AngleDEC { 0 };
            dms MountRA;
            dms MountDEC;
            double PulseRA { 0 };
            double PulseDEC { 0 };
            ISD::Mount::PierSide PierSide { ISD::Mount::PIER_UNKNOWN };
            bool DecSwap { false };
        };

        // The parameters associated with the original/stored calibration
        Calibration_Parameters m_Original;
        // The parameters associated with the compute calibration
        Calibration_Parameters m_Current;

        bool m_initialized { false };
        friend class TestGuideStars;
};

