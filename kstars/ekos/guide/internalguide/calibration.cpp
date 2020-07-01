/*  Calibration class.
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "calibration.h"
#include "Options.h"
#include "ekos_guide_debug.h"

Calibration::Calibration()
{
    ROT_Z            = Ekos::Matrix(0);
}

void Calibration::setAngle(double rotationAngle)
{
    angle = rotationAngle;
    ROT_Z = Ekos::RotateZ(-M_PI * angle / 180.0);
}

void Calibration::logCalibration() const
{
    qCDebug(KSTARS_EKOS_GUIDE) <<
                               QString("pulse: ms/pix ra: %1 dec %2  a-s/px x: %3 y: %4 Angle %5 foc %6 pw %7 ph %8 bin %9x%10")
                               .arg(raPulseMsPerPixel).arg(decPulseMsPerPixel)
                               .arg(xArcsecondsPerPixel()).arg(yArcsecondsPerPixel())
                               .arg(angle)
                               .arg(focalMm).arg(ccd_pixel_width).arg(ccd_pixel_height)
                               .arg(subBinX).arg(subBinY);
}

void Calibration::setParameters(double ccd_pix_width, double ccd_pix_height,
                                double focalLengthMm)
{
    focalMm = focalLengthMm;
    ccd_pixel_width = ccd_pix_width;
    ccd_pixel_height = ccd_pix_height;
}

void Calibration::setBinning(int binX, int binY)
{
    subBinX = binX;
    subBinY = binY;
}

void Calibration::setRaPulseMsPerPixel(double rate)
{
    raPulseMsPerPixel = rate;
    qCDebug(KSTARS_EKOS_GUIDE) << "Dither RA Rate " <<  raPulseMsPerPixel << " ms/Pixel";

}

void Calibration::setDecPulseMsPerPixel(double rate)
{
    decPulseMsPerPixel = rate;
    qCDebug(KSTARS_EKOS_GUIDE) << "Dither DEC Rate " <<  decPulseMsPerPixel << " ms/Pixel";

}

Vector Calibration::convertToArcseconds(const Vector &input) const
{
    Vector arcseconds;
    arcseconds.x = input.x * xArcsecondsPerPixel();
    arcseconds.y = input.y * yArcsecondsPerPixel();
    return arcseconds;
}

Vector Calibration::convertToPixels(const Vector &input) const
{
    Vector arcseconds;
    arcseconds.x = input.x / xArcsecondsPerPixel();
    arcseconds.y = input.y / yArcsecondsPerPixel();
    return arcseconds;
}

void Calibration::convertToPixels(double xArcseconds, double yArcseconds,
                                  double *xPixel, double *yPixel) const
{
    *xPixel = xArcseconds / xArcsecondsPerPixel();
    *yPixel = yArcseconds / yArcsecondsPerPixel();
}

Vector Calibration::rotateToRaDec(const Vector &input) const
{
    Vector in;
    in.x = input.x;
    in.y = -input.y;
    return (in * ROT_Z);
}

void Calibration::rotateToRaDec(double dx, double dy,
                                double *ra, double *dec) const
{
    Vector input;
    input.x = dx;
    input.y = dy;
    Vector out = rotateToRaDec(input);
    *ra = out.x;
    *dec = out.y;
}

double Calibration::xArcsecondsPerPixel() const
{
    // arcs = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    return (206264.806 * ccd_pixel_width * subBinX) / focalMm;
}

double Calibration::yArcsecondsPerPixel() const
{
    return (206264.806 * ccd_pixel_height * subBinY) / focalMm;
}

double Calibration::xPixelsPerArcsecond() const
{
    return (focalMm / (206264.806 * ccd_pixel_width * subBinX));
}

double Calibration::yPixelsPerArcsecond() const
{
    return (focalMm / (206264.806 * ccd_pixel_height * subBinY));
}

double Calibration::raPulseMillisecondsPerPixel() const
{
    return raPulseMsPerPixel;
}

double Calibration::raPulseMillisecondsPerArcsecond() const
{
    // TODO: Not exactly right as X pixels / arcsecond would not be the
    // same along the RA or DEC axes if the pixel weren't square.
    // For now assume square as before.
    // Would need to combine the X and Y values according to the RA/DEC rotation angle.
    return raPulseMsPerPixel * xPixelsPerArcsecond();
}

double Calibration::decPulseMillisecondsPerPixel() const
{
    return decPulseMsPerPixel;
}

double Calibration::decPulseMillisecondsPerArcsecond() const
{
    // TODO: Not exactly right as X pixels / arcsecond would not be the
    // same along the RA or DEC axes if the pixel weren't square.
    // For now assume square as before.
    // Would need to combine the X and Y values according to the RA/DEC rotation angle.
    return decPulseMsPerPixel * xPixelsPerArcsecond();
}
