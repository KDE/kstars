/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "calibration.h"
#include "Options.h"
#include "ekos_guide_debug.h"
#include <QDateTime>
#include "indi/indimount.h"

#define CAL_VERSION     "Cal v1.1"
#define CAL_VERSION_1_0 "Cal v1.0"

Calibration::Calibration()
{
    ROT_Z = GuiderUtils::Matrix(0);
}

// Set angle
void Calibration::setAngle(double rotationAngle)
{
    angle = rotationAngle;
    // Matrix is set a priori to NEGATIVE angle, because we want a CCW rotation in the
    // left hand system of the sensor coordinate system.
    // Rotation is used in guidestars::computeStarDrift() and guidestars::getDrift()
    ROT_Z = GuiderUtils::RotateZ(-M_PI * angle / 180.0);
}

void Calibration::setParameters(double ccd_pix_width, double ccd_pix_height,
                                double focalLengthMm,
                                int binX, int binY,
                                ISD::Mount::PierSide currentPierSide,
                                const dms &mountRa, const dms &mountDec)
{
    focalMm             = focalLengthMm;
    ccd_pixel_width     = ccd_pix_width;
    ccd_pixel_height    = ccd_pix_height;
    calibrationRA       = mountRa;
    calibrationDEC      = mountDec;
    subBinX             = binX;
    subBinY             = binY;
    subBinXused         = subBinX;
    subBinYused         = subBinY;
    calibrationPierSide = currentPierSide;
}

void Calibration::setBinningUsed(int x, int y)
{
    subBinXused         = x;
    subBinYused         = y;
}

void Calibration::setRaPulseMsPerArcsecond(double rate)
{
    raPulseMsPerArcsecond = rate;
}

void Calibration::setDecPulseMsPerArcsecond(double rate)
{
    decPulseMsPerArcsecond = rate;
}

GuiderUtils::Vector Calibration::convertToArcseconds(const GuiderUtils::Vector &input) const
{
    GuiderUtils::Vector arcseconds;
    arcseconds.x = input.x * xArcsecondsPerPixel();
    arcseconds.y = input.y * yArcsecondsPerPixel();
    return arcseconds;
}

GuiderUtils::Vector Calibration::convertToPixels(const GuiderUtils::Vector &input) const
{
    GuiderUtils::Vector arcseconds;
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

GuiderUtils::Vector Calibration::rotateToRaDec(const GuiderUtils::Vector &input) const
{
    GuiderUtils::Vector in;
    in.x = input.x;
    in.y = input.y;
    return (in * ROT_Z);
}

void Calibration::rotateToRaDec(double dx, double dy,
                                double *ra, double *dec) const
{
    GuiderUtils::Vector input;
    input.x = dx;
    input.y = dy;
    GuiderUtils::Vector out = rotateToRaDec(input);
    *ra = out.x;
    *dec = out.y;
}

double Calibration::binFactor() const
{
    return static_cast<double>(subBinXused) / static_cast<double>(subBinX);
}

double Calibration::inverseBinFactor() const
{
    return 1.0 / binFactor();
}

double Calibration::xArcsecondsPerPixel() const
{
    // arcs = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    return binFactor() * (206264.806 * ccd_pixel_width * subBinX) / focalMm;
}

double Calibration::yArcsecondsPerPixel() const
{
    return binFactor() * (206264.806 * ccd_pixel_height * subBinY) / focalMm;
}

double Calibration::xPixelsPerArcsecond() const
{
    return inverseBinFactor() * (focalMm / (206264.806 * ccd_pixel_width * subBinX));
}

double Calibration::yPixelsPerArcsecond() const
{
    return inverseBinFactor() * (focalMm / (206264.806 * ccd_pixel_height * subBinY));
}

double Calibration::raPulseMillisecondsPerArcsecond() const
{
    return raPulseMsPerArcsecond;
}

double Calibration::decPulseMillisecondsPerArcsecond() const
{
    return decPulseMsPerArcsecond;
}

double Calibration::calculateRotation(double x, double y)
{
    double phi;

    //if( (!GuiderUtils::Vector(delta_x, delta_y, 0)) < 2.5 )
    // JM 2015-12-10: Lower threshold to 1 pixel
    if ((!GuiderUtils::Vector(x, y, 0)) < 1)
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

bool Calibration::calculate1D(double start_x, double start_y,
                              double end_x, double end_y, int RATotalPulse)
{
    return calculate1D(end_x - start_x, end_y - start_y, RATotalPulse);
}

bool Calibration::calculate1D(double dx, double dy, int RATotalPulse)
{
    const double arcSecondsX = dx * xArcsecondsPerPixel();
    const double arcSecondsY = dy * yArcsecondsPerPixel();
    const double arcSeconds = std::hypot(arcSecondsX, arcSecondsY);
    if (arcSeconds < .1 || RATotalPulse <= 0)
    {
        qCDebug(KSTARS_EKOS_GUIDE)
                << QString("Bad input to calculate1D: ra %1 %2 total pulse %3")
                .arg(dx).arg(dy).arg(RATotalPulse);
        return false;
    }

    const double phi = calculateRotation(arcSecondsX, arcSecondsY);
    if (phi < 0)
        return false;

    setAngle(phi);
    calibrationAngle = phi;
    calibrationAngleRA = phi;
    calibrationAngleDEC = -1;
    decSwap = calibrationDecSwap = false;

    if (RATotalPulse > 0)
        setRaPulseMsPerArcsecond(RATotalPulse / arcSeconds);

    if (raPulseMillisecondsPerArcsecond() > 10000)
    {
        qCDebug(KSTARS_EKOS_GUIDE)
                << "Calibration computed unreasonable pulse-milliseconds-per-arcsecond: "
                << raPulseMillisecondsPerArcsecond() << " & " << decPulseMillisecondsPerArcsecond();
    }

    initialized = true;
    return true;
}

bool Calibration::calculate2D(
    double start_ra_x, double start_ra_y, double end_ra_x, double end_ra_y,
    double start_dec_x, double start_dec_y, double end_dec_x, double end_dec_y,
    bool *reverse_dec_dir, int RATotalPulse, int DETotalPulse)
{
    return calculate2D((end_ra_x - start_ra_x),
                       (end_ra_y - start_ra_y),
                       (end_dec_x - start_dec_x),
                       (end_dec_y - start_dec_y),
                       reverse_dec_dir, RATotalPulse, DETotalPulse);
}
bool Calibration::calculate2D(
    double ra_dx, double ra_dy, double dec_dx, double dec_dy,
    bool *reverse_dec_dir, int RATotalPulse, int DETotalPulse)
{
    const double raArcsecondsdX = ra_dx * xArcsecondsPerPixel();
    const double raArcsecondsdY = ra_dy * yArcsecondsPerPixel();
    const double decArcsecondsdX = dec_dx * xArcsecondsPerPixel();
    const double decArcsecondsdY = dec_dy * yArcsecondsPerPixel();
    const double raArcseconds = std::hypot(raArcsecondsdX, raArcsecondsdY);
    const double decArcseconds = std::hypot(decArcsecondsdX, decArcsecondsdY);
    if (raArcseconds < .1 || decArcseconds < .1 || RATotalPulse <= 0 || DETotalPulse <= 0)
    {
        qCDebug(KSTARS_EKOS_GUIDE)
                << QString("Bad input to calculate2D: ra %1 %2 dec %3 %4 total pulses %5 %6")
                .arg(ra_dx).arg(ra_dy).arg(dec_dx).arg(dec_dy).arg(RATotalPulse).arg(DETotalPulse);
        return false;
    }

    double phi_ra  = 0; // angle calculated by GUIDE_RA drift
    double phi_dec = 0; // angle calculated by GUIDE_DEC drift
    double phi     = 0;

    GuiderUtils::Vector ra_vect  = GuiderUtils::Normalize(GuiderUtils::Vector(raArcsecondsdX, raArcsecondsdY, 0));
    GuiderUtils::Vector dec_vect = GuiderUtils::Normalize(GuiderUtils::Vector(decArcsecondsdX, decArcsecondsdY, 0));

    GuiderUtils::Vector dec_vect_rotated_CCW = dec_vect * GuiderUtils::RotateZ(M_PI / 2);
    GuiderUtils::Vector dec_vect_rotated_CW = dec_vect * GuiderUtils::RotateZ(-M_PI / 2);

    double scalar_product_CCW = dec_vect_rotated_CCW & ra_vect;
    double scalar_product_CW = dec_vect_rotated_CW & ra_vect;

    bool ra_dec_is_CW_system = scalar_product_CCW > scalar_product_CW ? true : false;

    phi_ra = calculateRotation(raArcsecondsdX, raArcsecondsdY);
    if (phi_ra < 0)
        return false;

    phi_dec = calculateRotation(decArcsecondsdX, decArcsecondsdY);
    if (phi_dec < 0)
        return false;

    // Store the calibration angles.
    calibrationAngleRA = phi_ra;
    calibrationAngleDEC = phi_dec;

    if (ra_dec_is_CW_system)
        phi_dec += 90;
    else // ra-dec is standard CCW system
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

    // check DEC: Make standard CCW coordinate system
    if (reverse_dec_dir)
        *reverse_dec_dir = decSwap = ra_dec_is_CW_system;
    calibrationDecSwap = decSwap;

    if (RATotalPulse > 0)
        setRaPulseMsPerArcsecond(RATotalPulse / raArcseconds);

    if (DETotalPulse > 0)
        setDecPulseMsPerArcsecond(DETotalPulse / decArcseconds);

    // Check for unreasonable values.
    if (raPulseMillisecondsPerArcsecond() > 10000 || decPulseMillisecondsPerArcsecond() > 10000)
    {
        qCDebug(KSTARS_EKOS_GUIDE)
                << "Calibration computed unreasonable pulse-milliseconds-per-arcsecond: "
                << raPulseMillisecondsPerArcsecond() << " & " << decPulseMillisecondsPerArcsecond();
        return false;
    }

    qCDebug(KSTARS_EKOS_GUIDE) << QString("Set RA ms/as = %1ms / %2as = %3. DEC: %4ms / %5px = %6.")
                               .arg(RATotalPulse).arg(raArcseconds).arg(raPulseMillisecondsPerArcsecond())
                               .arg(DETotalPulse).arg(decArcseconds).arg(decPulseMillisecondsPerArcsecond());
    initialized = true;
    return true;
}

void Calibration::computeDrift(const GuiderUtils::Vector &detection, const GuiderUtils::Vector &reference,
                               double *raDrift, double *decDrift) const
{
    GuiderUtils::Vector drift = detection - reference;
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
        QString("%16,bx=%1,by=%2,pw=%3,ph=%4,fl=%5,ang=%6,angR=%7,angD=%8,"
                "ramspas=%9,decmspas=%10,swap=%11,ra=%12,dec=%13,side=%14,when=%15,calEnd")
        .arg(subBinX).arg(subBinY).arg(ccd_pixel_width).arg(ccd_pixel_height)
        .arg(focalMm).arg(calibrationAngle).arg(calibrationAngleRA)
        .arg(calibrationAngleDEC).arg(raPulseMsPerArcsecond)
        .arg(decPulseMsPerArcsecond).arg(calibrationDecSwap ? 1 : 0)
        .arg(raStr).arg(decStr).arg(static_cast<int>(calibrationPierSide)).arg(dateStr).arg(CAL_VERSION);
    return s;
}

namespace
{
bool parseString(const QString &ref, const QString &id, QString *result)
{
    if (!ref.startsWith(id)) return false;
    *result = ref.mid(id.size());
    return true;
}

bool parseDouble(const QString &ref, const QString &id, double *result)
{
    bool ok;
    if (!ref.startsWith(id)) return false;
    *result = ref.mid(id.size()).toDouble(&ok);
    return ok;
}

bool parseInt(const QString &ref, const QString &id, int *result)
{
    bool ok;
    if (!ref.startsWith(id)) return false;
    *result = ref.mid(id.size()).toInt(&ok);
    return ok;
}
}  // namespace

bool Calibration::restore(const QString &encoding)
{
    QStringList items = encoding.split(',');
    if (items.size() != 17) return false;
    int i = 0;

    bool fixOldCalibration = false;
    if (items[i] == CAL_VERSION_1_0)
    {
        fixOldCalibration = true;
    }
    else if (items[i] == CAL_VERSION) fixOldCalibration = false;
    else return false;

    if (!parseInt(items[++i], "bx=", &subBinX)) return false;
    if (!parseInt(items[++i], "by=", &subBinY)) return false;
    if (!parseDouble(items[++i], "pw=", &ccd_pixel_width)) return false;
    if (!parseDouble(items[++i], "ph=", &ccd_pixel_height)) return false;
    if (!parseDouble(items[++i], "fl=", &focalMm)) return false;
    if (!parseDouble(items[++i], "ang=", &calibrationAngle)) return false;
    setAngle(calibrationAngle);
    if (!parseDouble(items[++i], "angR=", &calibrationAngleRA)) return false;
    if (!parseDouble(items[++i], "angD=", &calibrationAngleDEC)) return false;

    // Switched from storing raPulseMsPerPixel to ...PerArcsecond and similar for DEC.
    // The below allows back-compatibility for older stored calibrations.
    if (!parseDouble(items[++i], "ramspas=", &raPulseMsPerArcsecond))
    {
        // Try the old version
        double raPulseMsPerPixel;
        if (parseDouble(items[i], "ramspp=", &raPulseMsPerPixel) && (xArcsecondsPerPixel() > 0))
        {
            // The previous calibration only worked on square pixels.
            raPulseMsPerArcsecond = raPulseMsPerPixel / xArcsecondsPerPixel();
        }
        else return false;
    }
    if (!parseDouble(items[++i], "decmspas=", &decPulseMsPerArcsecond))
    {
        // Try the old version
        double decPulseMsPerPixel;
        if (parseDouble(items[i], "decmspp=", &decPulseMsPerPixel) && (yArcsecondsPerPixel() > 0))
        {
            // The previous calibration only worked on square pixels.
            decPulseMsPerArcsecond = decPulseMsPerPixel / yArcsecondsPerPixel();
        }
        else return false;
    }

    int tempInt;
    if (!parseInt(items[++i], "swap=", &tempInt)) return false;
    decSwap = static_cast<bool>(tempInt);
    if (fixOldCalibration)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << QString("Modifying v1.0 calibration");
        decSwap = !decSwap;
    }
    calibrationDecSwap = decSwap;
    QString tempStr;
    if (!parseString(items[++i], "ra=", &tempStr)) return false;
    dms nullDms;
    calibrationRA = tempStr.size() == 0 ? nullDms : dms::fromString(tempStr, true);
    if (!parseString(items[++i], "dec=", &tempStr)) return false;
    calibrationDEC = tempStr.size() == 0 ? nullDms : dms::fromString(tempStr, false);
    if (!parseInt(items[++i], "side=", &tempInt)) return false;
    calibrationPierSide = static_cast<ISD::Mount::PierSide>(tempInt);
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

bool Calibration::restore(ISD::Mount::PierSide currentPierSide,
                          bool reverseDecOnPierChange, int currentBinX, int currentBinY,
                          const dms *declination)
{
    return restore(Options::serializedCalibration(), currentPierSide,
                   reverseDecOnPierChange, currentBinX, currentBinY, declination);
}

double Calibration::correctRA(double raMsPerArcsec, const dms &calibrationDec, const dms &currentDec)
{
    constexpr double MAX_DEC = 60.0;
    // Don't use uninitialized dms values.
    if (std::isnan(calibrationDec.Degrees()) || std::isnan(currentDec.Degrees()))
    {
        qCDebug(KSTARS_EKOS_GUIDE) << QString("Did not convert calibration RA rate. Input DEC invalid");
        return raMsPerArcsec;
    }
    if ((calibrationDec.Degrees() > MAX_DEC) || (calibrationDec.Degrees() < -MAX_DEC))
    {
        qCDebug(KSTARS_EKOS_GUIDE) << QString("Didn't correct calibration RA rate. Calibration DEC %1 too close to pole")
                                   .arg(QString::number(calibrationDec.Degrees(), 'f', 1));
        return raMsPerArcsec;
    }
    const double toRadians = M_PI / 180.0;
    const double numer = std::cos(currentDec.Degrees() * toRadians);
    const double denom = std::cos(calibrationDec.Degrees() * toRadians);
    if (raMsPerArcsec == 0) return raMsPerArcsec;
    const double rate = 1.0 / raMsPerArcsec;
    if (denom == 0) return raMsPerArcsec;
    const double adjustedRate = numer * rate / denom;
    if (adjustedRate == 0) return raMsPerArcsec;
    const double adjustedMsPerArcsecond = 1.0 / adjustedRate;
    qCDebug(KSTARS_EKOS_GUIDE) << QString("Corrected calibration RA rate. %1 --> %2. Calibration DEC %3 current DEC %4.")
                               .arg(QString::number(raMsPerArcsec, 'f', 1))
                               .arg(QString::number(adjustedMsPerArcsecond, 'f', 1))
                               .arg(QString::number(calibrationDec.Degrees(), 'f', 1))
                               .arg(QString::number(currentDec.Degrees(), 'f', 1));
    return adjustedMsPerArcsecond;
}

bool Calibration::restore(const QString &encoding, ISD::Mount::PierSide currentPierSide,
                          bool reverseDecOnPierChange, int currentBinX, int currentBinY,
                          const dms *currentDeclination)
{
    // Fail if we couldn't read the calibration.
    if (!restore(encoding))
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Could not restore calibration--couldn't read.";
        return false;
    }
    // We don't restore calibrations where either the calibration or the current pier side
    // is unknown.
    if (calibrationPierSide == ISD::Mount::PIER_UNKNOWN ||
            currentPierSide == ISD::Mount::PIER_UNKNOWN)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Could not restore calibration--pier side unknown.";
        return false;
    }

    if (currentDeclination != nullptr)
        raPulseMsPerArcsecond = correctRA(raPulseMsPerArcsecond, calibrationDEC, *currentDeclination);

    subBinXused = currentBinX;
    subBinYused = currentBinY;

    // Succeed if the calibration was on the same side of the pier as we're currently using.
    if (currentPierSide == calibrationPierSide)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Restored calibration--same pier side. Encoding:" << encoding;
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
            << QString("Restored calibration--flipped angles. Angle %1, swap %2 ms/as: %3 %4. Encoding: %5")
            .arg(angle).arg(decSwap ? "T" : "F").arg(raPulseMsPerArcsecond).arg(decPulseMsPerArcsecond).arg(encoding);
    return true;
}
