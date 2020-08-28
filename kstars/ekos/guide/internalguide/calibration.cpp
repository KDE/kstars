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
#include <QStringRef>
#include <QDateTime>
#include "indi/inditelescope.h"

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
                               QString("Calibration. pulse: ms/pix ra: %1 dec %2  a-s/px x: %3 y: %4 Angle %5 foc %6 pw %7 ph %8 bin %9x%10")
                               .arg(raPulseMsPerPixel).arg(decPulseMsPerPixel)
                               .arg(xArcsecondsPerPixel()).arg(yArcsecondsPerPixel())
                               .arg(angle)
                               .arg(focalMm).arg(ccd_pixel_width).arg(ccd_pixel_height)
                               .arg(subBinX).arg(subBinY);
}

void Calibration::setParameters(double ccd_pix_width, double ccd_pix_height,
                                double focalLengthMm,
                                int binX, int binY,
                                ISD::Telescope::PierSide currentPierSide,
                                const dms &mountRa, const dms &mountDec)
{
    focalMm             = focalLengthMm;
    ccd_pixel_width     = ccd_pix_width;
    ccd_pixel_height    = ccd_pix_height;
    calibrationRA       = mountRa;
    calibrationDEC      = mountDec;
    subBinX             = binX;
    subBinY             = binY;
    calibrationPierSide = currentPierSide;
}

void Calibration::setRaPulseMsPerPixel(double rate)
{
    raPulseMsPerPixel = rate;
}

void Calibration::setDecPulseMsPerPixel(double rate)
{
    decPulseMsPerPixel = rate;
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

double Calibration::calculateRotation(double x, double y)
{
    double phi;

    y = -y;

    //if( (!Vector(delta_x, delta_y, 0)) < 2.5 )
    // JM 2015-12-10: Lower threshold to 1 pixel
    if ((!Vector(x, y, 0)) < 1)
        return -1;

    // 90 or 270 degrees
    if (fabs(x) < fabs(y) / 1000000.0)
    {
        phi = y > 0 ? 90.0 : 270;
    }
    else
    {
        phi = 180.0 / M_PI * atan2(y, x);
        if (phi < 0)
            phi += 360.0;
    }

    return phi;
}


bool Calibration::calculate1D(double x, double y, int RATotalPulse)
{
    double phi = calculateRotation(x, y);
    if (phi < 0)
        return false;

    setAngle(phi);
    calibrationAngle = phi;
    calibrationAngleRA = phi;
    calibrationAngleDEC = -1;
    decSwap = calibrationDecSwap = false;

    if (RATotalPulse > 0)
        setRaPulseMsPerPixel(RATotalPulse / std::hypot(x, y));

    initialized = true;
    return true;
}

bool Calibration::calculate2D(
    double ra_x, double ra_y, double dec_x, double dec_y,
    bool *swap_dec, int RATotalPulse, int DETotalPulse)
{
    double phi_ra  = 0; // angle calculated by GUIDE_RA drift
    double phi_dec = 0; // angle calculated by GUIDE_DEC drift
    double phi     = 0;

    Vector ra_vect  = Normalize(Vector(ra_x, -ra_y, 0));
    Vector dec_vect = Normalize(Vector(dec_x, -dec_y, 0));

    Vector try_increase = dec_vect * Ekos::RotateZ(M_PI / 2);
    Vector try_decrease = dec_vect * Ekos::RotateZ(-M_PI / 2);

    double cos_increase = try_increase & ra_vect;
    double cos_decrease = try_decrease & ra_vect;

    bool do_increase = cos_increase > cos_decrease ? true : false;

    phi_ra = calculateRotation(ra_x, ra_y);
    if (phi_ra < 0)
        return false;

    phi_dec = calculateRotation(dec_x, dec_y);
    if (phi_dec < 0)
        return false;

    // Store the calibration angles.
    calibrationAngleRA = phi_ra;
    calibrationAngleDEC = phi_dec;

    if (do_increase)
        phi_dec += 90;
    else
        phi_dec -= 90;

    if (phi_dec > 360)
        phi_dec -= 360.0;
    if (phi_dec < 0)
        phi_dec += 360.0;

    if (fabs(phi_dec - phi_ra) > 180)
    {
        if (phi_ra > phi_dec)
            phi_ra -= 360;
        else
            phi_dec -= 360;
    }

    // average angles
    phi = (phi_ra + phi_dec) / 2;
    if (phi < 0)
        phi += 360.0;

    // setAngle sets the angle we'll use when guiding.
    // calibrationAngle is the saved angle to be stored.
    // They're the same now, but if we flip pier sides, angle may change.
    setAngle(phi);
    calibrationAngle = phi;

    // check DEC
    if (swap_dec)
        *swap_dec = decSwap = do_increase ? false : true;
    calibrationDecSwap = decSwap;

    if (RATotalPulse > 0)
        setRaPulseMsPerPixel(RATotalPulse / std::hypot(ra_x, ra_y));

    if (DETotalPulse > 0)
        setDecPulseMsPerPixel(DETotalPulse / std::hypot(dec_x, dec_y));

    qCDebug(KSTARS_EKOS_GUIDE) << QString("Set RA ms/px = %1ms / %2px = %3. DEC: %4ms / %5px = %6.")
                               .arg(RATotalPulse).arg(std::hypot(ra_x, ra_y)).arg(raPulseMsPerPixel)
                               .arg(DETotalPulse).arg(std::hypot(dec_x, dec_y)).arg(decPulseMsPerPixel);
    initialized = true;
    return true;
}

void Calibration::computeDrift(const Vector &detection, const Vector &reference,
                               double *raDrift, double *decDrift) const
{
    Vector drift = detection - reference;
    drift = rotateToRaDec(drift);
    *raDrift   = drift.x;
    *decDrift = drift.y;
}

// Not sure why we allow this.
void Calibration::setDeclinationSwapEnabled(bool value)
{
    qCDebug(KSTARS_EKOS_GUIDE) << QString("decSwap set to %1, was %2, cal %3")
                               .arg(value ? "T" : "F")
                               .arg(decSwap ? "T" : "F")
                               .arg(calibrationDecSwap ? "T" : "F");
    decSwap = value;
}
QString Calibration::serialize() const
{
    QDateTime now = QDateTime::currentDateTime();
    QString dateStr = now.toString("yyyy-MM-dd hh:mm:ss");
    QString raStr = std::isnan(calibrationRA.Degrees()) ? "" : calibrationRA.toDMSString(false, true, true);
    QString decStr = std::isnan(calibrationDEC.Degrees()) ? "" : calibrationDEC.toHMSString(true, true);
    QString s =
        QString("Cal v1.0,bx=%1,by=%2,pw=%3,ph=%4,fl=%5,ang=%6,angR=%7,angD=%8,"
                "ramspp=%9,decmspp=%10,swap=%11,ra=%12,dec=%13,side=%14,when=%15,calEnd")
        .arg(subBinX).arg(subBinY).arg(ccd_pixel_width).arg(ccd_pixel_height)
        .arg(focalMm).arg(calibrationAngle).arg(calibrationAngleRA)
        .arg(calibrationAngleDEC).arg(raPulseMsPerPixel)
        .arg(decPulseMsPerPixel).arg(calibrationDecSwap ? 1 : 0)
        .arg(raStr).arg(decStr).arg(static_cast<int>(calibrationPierSide)).arg(dateStr);
    return s;
}

namespace
{
bool parseString(const QStringRef &ref, const QString &id, QString *result)
{
    if (!ref.startsWith(id)) return false;
    *result = ref.mid(id.size()).toString();
    return true;
}

bool parseDouble(const QStringRef &ref, const QString &id, double *result)
{
    bool ok;
    if (!ref.startsWith(id)) return false;
    *result = ref.mid(id.size()).toDouble(&ok);
    return ok;
}

bool parseInt(const QStringRef &ref, const QString &id, int *result)
{
    bool ok;
    if (!ref.startsWith(id)) return false;
    *result = ref.mid(id.size()).toInt(&ok);
    return ok;
}
}  // namespace

bool Calibration::restore(const QString &encoding)
{
    QVector<QStringRef> items = encoding.splitRef(',');
    if (items.size() != 17) return false;
    int i = 0;
    if (items[i] != "Cal v1.0") return false;
    if (!parseInt(items[++i], "bx=", &subBinX)) return false;
    if (!parseInt(items[++i], "by=", &subBinY)) return false;
    if (!parseDouble(items[++i], "pw=", &ccd_pixel_width)) return false;
    if (!parseDouble(items[++i], "ph=", &ccd_pixel_height)) return false;
    if (!parseDouble(items[++i], "fl=", &focalMm)) return false;
    if (!parseDouble(items[++i], "ang=", &calibrationAngle)) return false;
    setAngle(calibrationAngle);
    if (!parseDouble(items[++i], "angR=", &calibrationAngleRA)) return false;
    if (!parseDouble(items[++i], "angD=", &calibrationAngleDEC)) return false;
    if (!parseDouble(items[++i], "ramspp=", &raPulseMsPerPixel)) return false;
    if (!parseDouble(items[++i], "decmspp=", &decPulseMsPerPixel)) return false;
    int tempInt;
    if (!parseInt(items[++i], "swap=", &tempInt)) return false;
    decSwap = static_cast<bool>(tempInt);
    calibrationDecSwap = decSwap;
    QString tempStr;
    if (!parseString(items[++i], "ra=", &tempStr)) return false;
    dms nullDms;
    calibrationRA = tempStr.size() == 0 ? nullDms : dms::fromString(tempStr, true);
    if (!parseString(items[++i], "dec=", &tempStr)) return false;
    calibrationDEC = tempStr.size() == 0 ? nullDms : dms::fromString(tempStr, false);
    if (!parseInt(items[++i], "side=", &tempInt)) return false;
    calibrationPierSide = static_cast<ISD::Telescope::PierSide>(tempInt);
    if (!parseString(items[++i], "when=", &tempStr)) return false;
    // Don't keep the time. It's just for reference.
    if (items[++i] != "calEnd") return false;
    initialized = true;
    return true;
}

void Calibration::save() const
{
    QString encoding = serialize();
    Options::setSerializedCalibration(encoding);
    qCDebug(KSTARS_EKOS_GUIDE) << QString("Saved calibration: %1").arg(encoding);
}

bool Calibration::restore(ISD::Telescope::PierSide currentPierSide,
                          bool reverseDecOnPierChange, const dms *declination)
{
    return restore(Options::serializedCalibration(), currentPierSide,
                   reverseDecOnPierChange, declination);
}

double Calibration::correctRA(double raMsPerPixel, const dms &calibrationDec, const dms &currentDec)
{
    constexpr double MAX_DEC = 60.0;
    // Don't use uninitialized dms values.
    if (std::isnan(calibrationDec.Degrees()) || std::isnan(currentDec.Degrees()))
    {
        qCDebug(KSTARS_EKOS_GUIDE) << QString("Did not convert calibration RA rate. Input DEC invalid");
        return raMsPerPixel;
    }
    if ((calibrationDec.Degrees() > MAX_DEC) || (calibrationDec.Degrees() < -MAX_DEC))
    {
        qCDebug(KSTARS_EKOS_GUIDE) << QString("Didn't correct calibration RA rate. Calibration DEC %1 too close to pole")
                                   .arg(QString::number(calibrationDec.Degrees(), 'f', 1));
        return raMsPerPixel;
    }
    const double toRadians = M_PI / 180.0;
    const double numer = std::cos(currentDec.Degrees() * toRadians);
    const double denom = std::cos(calibrationDec.Degrees() * toRadians);
    const double rate = 1.0 / raMsPerPixel;
    const double adjustedRate = numer * rate / denom;
    const double adjustedMsPerPixel = 1.0 / adjustedRate;
    qCDebug(KSTARS_EKOS_GUIDE) << QString("Corrected calibration RA rate. %1 --> %2. Calibration DEC %3 current DEC %4.")
                               .arg(QString::number(raMsPerPixel, 'f', 1))
                               .arg(QString::number(adjustedMsPerPixel, 'f', 1))
                               .arg(QString::number(calibrationDec.Degrees(), 'f', 1))
                               .arg(QString::number(currentDec.Degrees(), 'f', 1));
    return adjustedMsPerPixel;
}

bool Calibration::restore(const QString &encoding, ISD::Telescope::PierSide currentPierSide,
                          bool reverseDecOnPierChange, const dms *currentDeclination)
{
    // Fail if we couldn't read the calibration.
    if (!restore(encoding))
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Could not restore calibration--couldn't read.";
        return false;
    }
    // We don't restore calibrations where either the calibration or the current pier side
    // is unknown.
    if (calibrationPierSide == ISD::Telescope::PIER_UNKNOWN ||
            currentPierSide == ISD::Telescope::PIER_UNKNOWN)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Could not restore calibration--pier side unknown.";
        return false;
    }

    if (currentDeclination != nullptr)
        raPulseMsPerPixel = correctRA(raPulseMsPerPixel, calibrationDEC, *currentDeclination);

    // Succeed if the calibration was on the same side of the pier as we're currently using.
    if (currentPierSide == calibrationPierSide)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Restored calibration--same pier side.";
        return true;
    }

    // Otherwise, swap the angles and succeed.
    angle = angle + 180.0;
    while (angle >= 360.0)
        angle = angle - 360.0;
    while (angle < 0.0)
        angle = angle + 360.0;
    // Although angle is already set, we do this to set the rotation matrix.
    setAngle(angle);

    // Note the negation in testing the option.
    // Since above the angle rotated by 180 degrees, both RA and DEC have already reversed.
    // If the user says not to reverse DEC, then we re-reverse by setting decSwap.
    // Doing it this way makes the option consistent with other programs the user may be familiar with (PHD2).
    if (!reverseDecOnPierChange)
        decSwap = !calibrationDecSwap;

    qCDebug(KSTARS_EKOS_GUIDE)
            << QString("Restored calibration--flipped angles. Angle %1, swap %2 mspp: %3 %4")
            .arg(angle).arg(decSwap ? "T" : "F").arg(raPulseMsPerPixel).arg(decPulseMsPerPixel);
    return true;
}
