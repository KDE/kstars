/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "calibration.h"
#include "Options.h"
#include "ekos_guide_debug.h"
#include "ekos/auxiliary/rotatorutils.h"
#include "ksutils.h"
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
    m_Current.Angle = rotationAngle;
    // Matrix is set a priori to NEGATIVE angle, because we want a CCW rotation in the
    // left hand system of the sensor coordinate system.
    // Rotation is used in guidestars::computeStarDrift() and guidestars::getDrift()
    ROT_Z = GuiderUtils::RotateZ(-M_PI * m_Current.Angle / 180.0);
}

void Calibration::setParameters(double ccd_pix_width, double ccd_pix_height,
                                double focalLengthMm,
                                int binX, int binY,
                                ISD::Mount::PierSide currentPierSide,
                                const dms &mountRa, const dms &mountDec)
{
    m_Current.FocalLength = focalLengthMm;
    m_Current.CCDPixelWidth = ccd_pix_width;
    m_Current.CCDPixelHeight = ccd_pix_height;
    m_Current.MountRA = mountRa;
    m_Current.MountDEC = mountDec;
    m_Current.SubBinX = binX;
    m_Current.SubBinY = binY;
    m_Current.PierSide = currentPierSide;
}

void Calibration::setBinningUsed(int x, int y)
{
    m_Current.SubBinX = x;
    m_Current.SubBinY = y;
}

void Calibration::setRaPulseMsPerArcsecond(double rate)
{
    m_Current.PulseRA = rate;
}

void Calibration::setDecPulseMsPerArcsecond(double rate)
{
    m_Current.PulseDEC = rate;
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
    return static_cast<double>(m_Current.SubBinX) / static_cast<double>(m_Original.SubBinX);
}

double Calibration::inverseBinFactor() const
{
    return 1.0 / binFactor();
}

double Calibration::xArcsecondsPerPixel() const
{
    // arcs = 3600*180/pi * (pix*ccd_pix_sz) / focal_len
    return binFactor() * (206264.806 * m_Current.CCDPixelWidth * m_Original.SubBinX) / m_Current.FocalLength;
}

double Calibration::yArcsecondsPerPixel() const
{
    return binFactor() * (206264.806 * m_Current.CCDPixelHeight * m_Original.SubBinY) / m_Current.FocalLength;
}

double Calibration::xPixelsPerArcsecond() const
{
    return inverseBinFactor() * (m_Current.FocalLength / (206264.806 * m_Current.CCDPixelWidth * m_Original.SubBinX));
}

double Calibration::yPixelsPerArcsecond() const
{
    return inverseBinFactor() * (m_Current.FocalLength / (206264.806 * m_Current.CCDPixelHeight * m_Original.SubBinY));
}

double Calibration::raPulseMillisecondsPerArcsecond() const
{
    return m_Current.PulseRA;
}

double Calibration::decPulseMillisecondsPerArcsecond() const
{
    return m_Current.PulseDEC;
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
    m_Current.Angle = phi;
    m_Current.AngleRA = phi;
    m_Current.AngleDEC = -1;
    m_Current.DecSwap = false;

    if (RATotalPulse > 0)
        setRaPulseMsPerArcsecond(RATotalPulse / arcSeconds);

    if (raPulseMillisecondsPerArcsecond() > 10000)
    {
        qCDebug(KSTARS_EKOS_GUIDE)
                << "Calibration computed unreasonable pulse-milliseconds-per-arcsecond: "
                << raPulseMillisecondsPerArcsecond() << " & " << decPulseMillisecondsPerArcsecond();
    }
    m_Original = m_Current;
    m_initialized = true;
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
    m_Current.AngleRA = phi_ra;
    m_Current.AngleDEC = phi_dec;

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
    // [m_Current.Angle] is the saved angle to be stored.
    // They're the same now, but if we flip pier sides, angle may change.
    setAngle(phi);
    m_Current.Angle = phi;

    // check DEC: Make standard CCW coordinate system
    if (reverse_dec_dir)
        *reverse_dec_dir = m_Current.DecSwap = ra_dec_is_CW_system;

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
    m_Original = m_Current;
    m_initialized = true;
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

// For test suite
void Calibration::setDeclinationSwapEnabled(bool value)
{
    qCDebug(KSTARS_EKOS_GUIDE) << QString("m_DecSwap set to %1, was %2, cal %3")
                               .arg(value ? "T" : "F")
                               .arg(m_Current.DecSwap ? "T" : "F")
                               .arg(m_Original.DecSwap ? "T" : "F");
    m_Original.DecSwap = value;
}

QString Calibration::serialize() const
{
    QDateTime now = QDateTime::currentDateTime();
    QString dateStr = now.toString("yyyy-MM-dd hh:mm:ss");
    QString raStr = std::isnan(m_Current.MountRA.Degrees()) ? "" : m_Current.MountRA.toDMSString(false, true, true);
    QString decStr = std::isnan(m_Current.MountDEC.Degrees()) ? "" : m_Current.MountDEC.toHMSString(true, true);
    QString s =
        QString("%16,bx=%1,by=%2,pw=%3,ph=%4,fl=%5,ang=%6,angR=%7,angD=%8,"
                "ramspas=%9,decmspas=%10,swap=%11,ra=%12,dec=%13,side=%14,when=%15,calEnd")
        .arg(m_Current.SubBinX).arg(m_Current.SubBinY).arg(m_Current.CCDPixelWidth).arg(m_Current.CCDPixelHeight)
        .arg(m_Current.FocalLength).arg(m_Current.Angle).arg(m_Current.AngleRA)
        .arg(m_Current.AngleDEC).arg(m_Current.PulseRA)
        .arg(m_Current.PulseDEC).arg(m_Current.DecSwap ? 1 : 0)
        .arg(raStr).arg(decStr).arg(static_cast<int>(m_Current.PierSide)).arg(dateStr).arg(CAL_VERSION);
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
    if (!parseInt(items[++i], "bx=", &m_Original.SubBinX)) return false;
    if (!parseInt(items[++i], "by=", &m_Original.SubBinY)) return false;
    if (!parseDouble(items[++i], "pw=", &m_Original.CCDPixelWidth)) return false;
    if (!parseDouble(items[++i], "ph=", &m_Original.CCDPixelHeight)) return false;
    if (!parseDouble(items[++i], "fl=", &m_Original.FocalLength)) return false;
    // Mutable parameters
    if (!parseDouble(items[++i], "ang=", &m_Original.Angle)) return false;
    if (!parseDouble(items[++i], "angR=", &m_Original.AngleRA)) return false;
    if (!parseDouble(items[++i], "angD=", &m_Original.AngleDEC)) return false;
    // Switched from storing raPulseMsPerPixel to ...PerArcsecond and similar for DEC.
    // The below allows back-compatibility for older stored calibrations.
    if (!parseDouble(items[++i], "ramspas=", &m_Original.PulseRA))
    {
        // Try the old version
        double raPulseMsPerPixel;
        if (parseDouble(items[i], "ramspp=", &raPulseMsPerPixel) && (xArcsecondsPerPixel() > 0))
        {
            // The previous calibration only worked on square pixels.
            m_Original.PulseRA = raPulseMsPerPixel / xArcsecondsPerPixel();
        }
        else return false;
    }
    if (!parseDouble(items[++i], "decmspas=", &m_Original.PulseDEC))
    {
        // Try the old version
        double decPulseMsPerPixel;
        if (parseDouble(items[i], "decmspp=", &decPulseMsPerPixel) && (yArcsecondsPerPixel() > 0))
        {
            // The previous calibration only worked on square pixels.
            m_Original.PulseDEC = decPulseMsPerPixel / yArcsecondsPerPixel();
        }
        else return false;
    }
    int tempInt;
    if (!parseInt(items[++i], "swap=", &tempInt)) return false;
    m_Original.DecSwap = static_cast<bool>(tempInt);
    if (fixOldCalibration)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << QString("Modifying v1.0 calibration");
        m_Original.DecSwap = !m_Original.DecSwap;
    }
    QString tempStr;
    if (!parseString(items[++i], "ra=", &tempStr)) return false;
    dms nullDms;
    m_Original.MountRA = tempStr.size() == 0 ? nullDms : dms::fromString(tempStr, true);
    if (!parseString(items[++i], "dec=", &tempStr)) return false;
    m_Original.MountDEC = tempStr.size() == 0 ? nullDms : dms::fromString(tempStr, false);
    if (!parseInt(items[++i], "side=", &tempInt)) return false;
    m_Original.PierSide = static_cast<ISD::Mount::PierSide>(tempInt);
    if (!parseString(items[++i], "when=", &tempStr)) return false;
    // Don't keep the time. It's just for reference.
    if (items[++i] != "calEnd") return false;
    m_Current = m_Original;
    m_initialized = true;
    return true;
}

void Calibration::save()
{
    QString encoding = serialize();
    Options::setSerializedCalibration(encoding);
    qCDebug(KSTARS_EKOS_GUIDE) << QString("Saved calibration: %1").arg(encoding);

}

bool Calibration::restore(ISD::Mount::PierSide currentPierSide,
                          bool reverseDecOnPierChange, int currentBinX, int currentBinY,
                          const dms declination)
{
    return restore(Options::serializedCalibration(), currentPierSide,
                   reverseDecOnPierChange, currentBinX, currentBinY, declination);
}

double Calibration::correctRA(double raMsPerArcsec, const dms calibrationDec, const dms currentDec)
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
                          const dms currentDeclination)
{
    // Fail if we couldn't read the calibration.
    if (!restore(encoding))
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Could not restore calibration--couldn't read.";
        return false;
    }
    // We don't restore calibrations where either the calibration or the current pier side
    // is unknown.
    if (m_Original.PierSide == ISD::Mount::PIER_UNKNOWN ||
            currentPierSide == ISD::Mount::PIER_UNKNOWN)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Could not restore calibration--pier side unknown.";
        return false;
    }

    if (std::isnan(currentDeclination.Degrees()))
    {
        return false;
    }

    m_Current.DecSwap = !reverseDecOnPierChange; // see updatePierSide() below!
    m_Current.SubBinX = currentBinX;
    m_Current.SubBinY = currentBinY;

    return true;
}

double Calibration::getDiffDEC(const dms currentDEC)
{
    return dms(m_Original.AngleDEC).deltaAngle(dms(currentDEC)).Degrees();
}

void Calibration::updateRotation(double CamRotation)
{
    setAngle(CamRotation);
    double DiffRotation = dms(CamRotation).deltaAngle(dms(m_Original.Angle)).Degrees();
    m_Current.AngleDEC = KSUtils::range360(m_Original.AngleDEC + DiffRotation);
    m_Current.AngleRA = KSUtils::range360(m_Original.AngleRA + DiffRotation);
}

void Calibration::updateRAPulse(const dms currentDEC)
{
    m_Current.PulseRA = correctRA(m_Original.PulseRA, m_Original.MountDEC, currentDEC);
}

void Calibration::updatePierside(ISD::Mount::PierSide *Pierside, double *Rotation,
                                 const bool FlipRotReady, const bool ManualRotatorOnly)
{
    if (*Pierside == ISD::Mount::PIER_UNKNOWN)
        *Pierside = RotatorUtils::Instance()->getMountPierside();
    if ((m_Current.PierSide != *Pierside) && (*Pierside != ISD::Mount::PIER_UNKNOWN))
    {
        m_Current.PierSide = *Pierside;
        // With an active rotator device:
        // After a flip [m_Current.Angle] is already set according to the rotated camera PA
        // via newPA() in RotatorSettings even if there was no subsequent alignment.
        // Hence there is nothing to do.

        // With "manual rotator" (is always activated in Camera::setRotator()):
        // (TODO: Better check if there is a rotator name via RotatorUtils!?)
        if (ManualRotatorOnly)
        {
            // After a flip with a subsequent alignment [m_Current.Angle] is set according to the
            // rotated camera PA via newPA() in Align.
            // After a flip with NO subsequent alignment [m_Current.Angle] has to be rotated.
            if (!FlipRotReady)
                *Rotation = KSUtils::range360(*Rotation + 180);
        }
    }
     // Note the negation of the option [reverseDecOnPierChange] in the following test:
    // [reverseDecOnPierChange] describes a feature of the mount:  If after a flip
    // the mount returns a reversed DEC direction (for coordinate and pulse) the option
    // has to be set to true and there is no DecSwap needed (i.e. DecSwap remains false).
    // Otherwise guide module must reverse DEC direction. The option has to be set
    // to false which in turn sets DecSwap to true in case the current pierside differs
    // from the one used for calibration.
    // Doing it this way makes the option consistent with other programs the user may be familiar with (PHD2).
    if (!Options::reverseDecOnPierSideChange())
            m_Current.DecSwap = (m_Current.PierSide == m_Original.PierSide) ? m_Original.DecSwap : !m_Original.DecSwap;
}
