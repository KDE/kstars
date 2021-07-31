/***************************************************************************
                          skypoint.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001-2005 by Jason Harris
    email                : jharris@30doradus.org
    copyright            : (C) 2004-2005 by Pablo de Vicente
    email                : p.devicente@wanadoo.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skypoint.h"

#include "dms.h"
#include "ksnumbers.h"
#include "kstarsdatetime.h"
#include "kssun.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skyobject.h"
#include "skycomponents/skymapcomposite.h"

#include "config-kstars.h"

#include <KLocalizedString>

#include <QDebug>

#include <cmath>



// N.B. To use libnova depending on the availability of the package,
// uncomment the following:
/*
#ifdef HAVE_LIBNOVA
#define SKYPOINT_USE_LIBNOVA 1
#endif
*/

#ifdef SKYPOINT_USE_LIBNOVA
#include <libnova/libnova.h>
bool SkyPoint::implementationIsLibnova = true;
#else
bool SkyPoint::implementationIsLibnova = false;
#endif

#ifdef PROFILE_COORDINATE_CONVERSION
#include <ctime> // For profiling, remove if not profiling.
long unsigned SkyPoint::eqToHzCalls = 0;
double SkyPoint::cpuTime_EqToHz     = 0.;
#endif

KSSun *SkyPoint::m_Sun         = nullptr;
const double SkyPoint::altCrit = -1.0;

SkyPoint::SkyPoint()
{
    // Default constructor. Set nonsense values
    RA0.setD(-1);   // RA >= 0 always :-)
    Dec0.setD(180); // Dec is between -90 and 90 Degrees :-)
    RA            = RA0;
    Dec           = Dec0;
    lastPrecessJD = J2000; // By convention, we use J2000 coordinates
}

void SkyPoint::set(const dms &r, const dms &d)
{
    RA0 = RA = r;
    Dec0 = Dec    = d;
    lastPrecessJD = J2000; // By convention, we use J2000 coordinates
}

void SkyPoint::EquatorialToHorizontal(const dms *LST, const dms *lat)
{
    //    qDebug() << "NOTE: This EquatorialToHorizontal overload (using dms pointers instead of CachingDms pointers) is deprecated and should be replaced with CachingDms prototype wherever speed is desirable!";
    CachingDms _LST(*LST), _lat(*lat);
    EquatorialToHorizontal(&_LST, &_lat);
}

void SkyPoint::EquatorialToHorizontal(const CachingDms *LST, const CachingDms *lat)
{
#ifdef PROFILE_COORDINATE_CONVERSION
    std::clock_t start = std::clock();
#endif
    //Uncomment for spherical trig version
    double AltRad, AzRad;
    double sindec, cosdec, sinlat, coslat, sinHA, cosHA;
    double sinAlt, cosAlt;

    CachingDms HourAngle =
        (*LST) - ra(); // Using CachingDms subtraction operator to find cos/sin of HourAngle without calling sincos()

    lat->SinCos(sinlat, coslat);
    dec().SinCos(sindec, cosdec);
    HourAngle.SinCos(sinHA, cosHA);

    sinAlt = sindec * sinlat + cosdec * coslat * cosHA;
    AltRad = asin(sinAlt);

    cosAlt = sqrt(
                 1 -
                 sinAlt *
                 sinAlt); // Avoid trigonometric function. Return value of asin is always in [-pi/2, pi/2] and in this domain cosine is always non-negative, so we can use this.
    if (cosAlt == 0.)
        cosAlt = cos(AltRad);

    double arg = (sindec - sinlat * sinAlt) / (coslat * cosAlt);
    if (arg <= -1.0)
        AzRad = dms::PI;
    else if (arg >= 1.0)
        AzRad = 0.0;
    else
        AzRad = acos(arg);

    if (sinHA > 0.0 && AzRad != 0.0)
        AzRad = 2.0 * dms::PI - AzRad; // resolve acos() ambiguity

    Alt.setRadians(AltRad);
    Az.setRadians(AzRad);
#ifdef PROFILE_COORDINATE_CONVERSION
    std::clock_t stop = std::clock();
    cpuTime_EqToHz += double(stop - start) / double(CLOCKS_PER_SEC); // Accumulate time in seconds
    ++eqToHzCalls;
#endif

    // //Uncomment for XYZ version
    //  	double xr, yr, zr, xr1, zr1, sa, ca;
    // 	//Z-axis rotation by -LST
    // 	dms a = dms( -1.0*LST->Degrees() );
    // 	a.SinCos( sa, ca );
    // 	xr1 = m_X*ca + m_Y*sa;
    // 	yr  = -1.0*m_X*sa + m_Y*ca;
    // 	zr1 = m_Z;
    //
    // 	//Y-axis rotation by lat - 90.
    // 	a = dms( lat->Degrees() - 90.0 );
    // 	a.SinCos( sa, ca );
    // 	xr = xr1*ca - zr1*sa;
    // 	zr = xr1*sa + zr1*ca;
    //
    // 	//FIXME: eventually, we will work with XYZ directly
    // 	Alt.setRadians( asin( zr ) );
    // 	Az.setRadians( atan2( yr, xr ) );
}

void SkyPoint::HorizontalToEquatorial(const dms *LST, const dms *lat)
{
    double HARad, DecRad;
    double sinlat, coslat, sinAlt, cosAlt, sinAz, cosAz;
    double sinDec, cosDec;

    lat->SinCos(sinlat, coslat);
    alt().SinCos(sinAlt, cosAlt);
    Az.SinCos(sinAz, cosAz);

    sinDec = sinAlt * sinlat + cosAlt * coslat * cosAz;
    DecRad = asin(sinDec);
    cosDec = cos(DecRad);
    Dec.setRadians(DecRad);

    double x = (sinAlt - sinlat * sinDec) / (coslat * cosDec);

    //Under certain circumstances, x can be very slightly less than -1.0000, or slightly
    //greater than 1.0000, leading to a crash on acos(x).  However, the value isn't
    //*really* out of range; it's a kind of roundoff error.
    if (x < -1.0 && x > -1.000001)
        HARad = dms::PI;
    else if (x > 1.0 && x < 1.000001)
        HARad = 0.0;
    else if (x < -1.0)
    {
        //qWarning() << "Coordinate out of range.";
        HARad = dms::PI;
    }
    else if (x > 1.0)
    {
        //qWarning() << "Coordinate out of range.";
        HARad = 0.0;
    }
    else
        HARad = acos(x);

    if (sinAz > 0.0)
        HARad = 2.0 * dms::PI - HARad; // resolve acos() ambiguity

    RA.setRadians(LST->radians() - HARad);
    RA.reduceToRange(dms::ZERO_TO_2PI);
}

void SkyPoint::findEcliptic(const CachingDms *Obliquity, dms &EcLong, dms &EcLat)
{
    double sinRA, cosRA, sinOb, cosOb, sinDec, cosDec, tanDec;
    ra().SinCos(sinRA, cosRA);
    dec().SinCos(sinDec, cosDec);
    Obliquity->SinCos(sinOb, cosOb);

    double ycosDec        = sinRA * cosOb * cosDec + sinDec * sinOb;
    double ELongRad = atan2(ycosDec, cosDec * cosRA);
    EcLong.setRadians(ELongRad);
    EcLong.reduceToRange(dms::ZERO_TO_2PI);
    EcLat.setRadians(asin(sinDec * cosOb - cosDec * sinOb * sinRA)); // FIXME: Haversine
}

void SkyPoint::setFromEcliptic(const CachingDms *Obliquity, const dms &EcLong, const dms &EcLat)
{
    double sinLong, cosLong, sinLat, cosLat, sinObliq, cosObliq;
    EcLong.SinCos(sinLong, cosLong);
    EcLat.SinCos(sinLat, cosLat);
    Obliquity->SinCos(sinObliq, cosObliq);

    // double sinDec = sinLat * cosObliq + cosLat * sinObliq * sinLong;

    double ycosLat = sinLong * cosObliq * cosLat - sinLat * sinObliq;
    //    double RARad =  atan2( y, cosLong );
    RA.setUsing_atan2(ycosLat, cosLong * cosLat);
    RA.reduceToRange(dms::ZERO_TO_2PI);
    // Dec.setUsing_asin(sinDec);

    // Use Haversine to set declination
    Dec.setRadians(dms::PI/2.0 - 2.0 * asin(sqrt(0.5 * (
                                                     1.0 - sinLat * cosObliq
                                                     - cosLat * sinObliq * sinLong
                                                     ))));
}

void SkyPoint::precess(const KSNumbers *num)
{
    double cosRA0, sinRA0, cosDec0, sinDec0;
    const Eigen::Matrix3d &precessionMatrix = num->p2();
    Eigen::Vector3d v, s;

    RA0.SinCos(sinRA0, cosRA0);
    Dec0.SinCos(sinDec0, cosDec0);

    s[0] = cosRA0 * cosDec0;
    s[1] = sinRA0 * cosDec0;
    s[2] = sinDec0;

    // NOTE: Rotation matrices are the fastest way to do rotations on
    // a vector. Quaternions need more multiplications. The rotation
    // matrix compensates in some sense by having more 'precomputed'
    // multiplications. The matrix elements seem to cache nicely, so
    // there isn't much overhead in accessing them.

    //Multiply P2 and s to get v, the vector representing the new coords.
    // for ( unsigned int i=0; i<3; ++i ) {
    //     v[i] = 0.0;
    //     for (uint j=0; j< 3; ++j) {
    //         v[i] += num->p2( j, i )*s[j];
    //     }
    // }
    v.noalias() = precessionMatrix * s;

    //Extract RA, Dec from the vector:
    RA.setUsing_atan2(v[1], v[0]);
    RA.reduceToRange(dms::ZERO_TO_2PI);
    Dec.setUsing_asin(v[2]);
}

SkyPoint SkyPoint::deprecess(const KSNumbers *num, long double epoch)
{
    SkyPoint p1(RA, Dec);
    long double now = num->julianDay();
    p1.precessFromAnyEpoch(now, epoch);
    if ((std::isnan(RA0.Degrees()) || std::isnan(Dec0.Degrees())) ||
            (!std::isnan(Dec0.Degrees()) && fabs(Dec0.Degrees()) > 90.0))
    {
        // We have invalid RA0 and Dec0, so set them if epoch = J2000. Otherwise, do not touch.
        if (epoch == J2000L)
        {
            RA0  = p1.ra();
            Dec0 = p1.dec();
        }
    }
    return p1;
}

void SkyPoint::nutate(const KSNumbers *num, const bool reverse)
{
    //Step 2: Nutation
    if (fabs(Dec.Degrees()) < 80.0) //approximate method
    {
        double cosRA, sinRA, cosDec, sinDec, tanDec;
        double cosOb, sinOb;
        double dRA, dDec;

        RA.SinCos(sinRA, cosRA);
        Dec.SinCos(sinDec, cosDec);

        tanDec = sinDec / cosDec;

        // Equ 23.1 in Jean Meeus' book
        // nut_ecliptic / num->obliquity() is called epsilon in Meeus
        // nut.longitude / num->dEcLong() is called delta psi in Meeus
        // nut.obliquity / num->dObliq() is called delta epsilon in Meeus
        // Meeus notes that these expressions are invalid if the star is
        // close to one of the celestial poles

        // N.B. These expressions are valid for FK5 coordinates
        // (presumably also valid for ICRS by extension), but not for
        // FK4. See the "Important Remark" on Page 152 of Jean Meeus'
        // book.

#ifdef SKYPOINT_USE_LIBNOVA
        // code lifted from libnova ln_get_equ_nut, tailored to our needs
        // with the option to add or remove nutation
        struct ln_nutation nut;
        ln_get_nutation (num->julianDay(), &nut); // FIXME: Is this cached, or is it a slow call? If it is slow, we should move it to KSNumbers

        double nut_ecliptic = ln_deg_to_rad(nut.ecliptic + nut.obliquity);
        sinOb = sin(nut_ecliptic);
        cosOb = cos(nut_ecliptic);

        dRA = nut.longitude * (cosOb + sinOb * sinRA * tanDec) - nut.obliquity * cosRA * tanDec;
        dDec = nut.longitude * (sinOb * cosRA) + nut.obliquity * sinRA;
#else
        num->obliquity()->SinCos(sinOb, cosOb);

        // N.B. num->dEcLong() and num->dObliq() methods return in
        // degrees, whereby the corrections dRA and dDec are also in
        // degrees.
        dRA  = num->dEcLong() * (cosOb + sinOb * sinRA * tanDec) - num->dObliq() * cosRA * tanDec;
        dDec = num->dEcLong() * (sinOb * cosRA) + num->dObliq() * sinRA;
#endif
        // the sign changed to remove nutation
        if (reverse)
        {
            dRA = -dRA;
            dDec = -dDec;
        }
        RA.setD(RA.Degrees() + dRA);
        Dec.setD(Dec.Degrees() + dDec);
    }
    else //exact method
    {
        // NOTE: Meeus declares that you must add Δψ to the ecliptic
        // longitude of the body to get a more accurate precession
        // result, but fails to emphasize that the NCP of the two
        // coordinates systems is different. The (RA, Dec) without
        // nutation computed, i.e. the mean place of the sky point is
        // referenced to the un-nutated geocentric frame (without the
        // obliquity correction), whereas the (RA, Dec) after nutation
        // is applied is referenced to the nutated geocentric
        // frame. This is more clearly explained in the "Explanatory
        // Supplement to the Astronomical Almanac" by
        // K. P. Seidelmann, which can be borrowed on the internet
        // archive as of this writing, see page 114:
        // https://archive.org/details/explanatorysuppl00pken
        //
        // The rotation matrix formulation in (3.222-3) and the figure
        // 3.222.1 make this clear

        // TODO apply reverse test above 80 degrees
        dms EcLong, EcLat;
        CachingDms obliquityWithoutNutation(*num->obliquity() - dms(num->dObliq()));

        if (reverse)
        {
            //Subtract dEcLong from the Ecliptic Longitude
            findEcliptic(num->obliquity(), EcLong, EcLat);
            dms newLong(EcLong.Degrees() - num->dEcLong());
            setFromEcliptic(&obliquityWithoutNutation, newLong, EcLat); // FIXME: Check
        }
        else
        {
            //Add dEcLong to the Ecliptic Longitude
            findEcliptic(&obliquityWithoutNutation, EcLong, EcLat);
            dms newLong(EcLong.Degrees() + num->dEcLong());
            setFromEcliptic(num->obliquity(), newLong, EcLat);
        }
    }
}

SkyPoint SkyPoint::moveAway(const SkyPoint &from, double dist) const
{
    CachingDms lat1, dtheta;

    if (dist == 0.0)
    {
        qDebug() << "moveAway called with zero distance!";
        return *this;
    }

    double dst = fabs(dist * dms::DegToRad / 3600.0); // In radian

    // Compute the bearing angle w.r.t. the RA axis ("latitude")
    CachingDms dRA(ra() - from.ra());
    CachingDms dDec(dec() - from.dec());
    double bearing = atan2(dRA.sin() / dRA.cos(), dDec.sin()); // Do not use dRA = PI / 2!!
    //double bearing = atan2( dDec.radians() , dRA.radians() );

    //    double dir0 = (dist >= 0 ) ? bearing : bearing + dms::PI; // in radian
    double dir0   = bearing + std::signbit(dist) * dms::PI; // might be faster?
    double sinDst = sin(dst), cosDst = cos(dst);

    lat1.setUsing_asin(dec().sin() * cosDst + dec().cos() * sinDst * cos(dir0));
    dtheta.setUsing_atan2(sin(dir0) * sinDst * dec().cos(), cosDst - dec().sin() * lat1.sin());

    return SkyPoint(ra() + dtheta, lat1);
}

bool SkyPoint::checkBendLight()
{
    // First see if we are close enough to the sun to bother about the
    // gravitational lensing effect. We correct for the effect at
    // least till b = 10 solar radii, where the effect is only about
    // 0.06".  Assuming min. sun-earth distance is 200 solar radii.
    static const dms maxAngle(1.75 * (30.0 / 200.0) / dms::DegToRad);

    if (!m_Sun)
    {
        SkyComposite *skycomopsite = KStarsData::Instance()->skyComposite();

        if (skycomopsite == nullptr)
            return false;

        m_Sun = dynamic_cast<KSSun *>(skycomopsite->findByName(i18n("Sun")));

        if (m_Sun == nullptr)
            return false;
    }

    // TODO: This can be optimized further. We only need a ballpark estimate of the distance to the sun to start with.
    return (fabs(angularDistanceTo(static_cast<const SkyPoint *>(m_Sun)).Degrees()) <=
            maxAngle.Degrees()); // NOTE: dynamic_cast is slow and not important here.
}

bool SkyPoint::bendlight()
{
    // NOTE: This should be applied before aberration
    // NOTE: One must call checkBendLight() before unnecessarily calling this.
    // We correct for GR effects

    // NOTE: This code is buggy. The sun needs to be initialized to
    // the current epoch -- but we are not certain that this is the
    // case. We have, as of now, no way of telling if the sun is
    // initialized or not. If we initialize the sun here, we will be
    // slowing down the program rather substantially and potentially
    // introducing bugs. Therefore, we just ignore this problem, and
    // hope that whenever the user is interested in seeing the effects
    // of GR, we have the sun initialized correctly. This is usually
    // the case. When the sun is not correctly initialized, rearth()
    // is not computed, so we just assume it is nominally equal to 1
    // AU to get a reasonable estimate.
    Q_ASSERT(m_Sun);
    double corr_sec = 1.75 * m_Sun->physicalSize() /
                      ((std::isfinite(m_Sun->rearth()) ? m_Sun->rearth() : 1) * AU_KM *
                       angularDistanceTo(static_cast<const SkyPoint *>(m_Sun)).sin());
    Q_ASSERT(corr_sec > 0);

    SkyPoint sp = moveAway(*m_Sun, corr_sec);
    setRA(sp.ra());
    setDec(sp.dec());
    return true;
}

void SkyPoint::aberrate(const KSNumbers *num, bool reverse)
{
#ifdef SKYPOINT_USE_LIBNOVA
    ln_equ_posn pos { RA.Degrees(), Dec.Degrees() };
    ln_equ_posn abPos { 0, 0 };
    ln_get_equ_aber(&pos, num->julianDay(), &abPos);
    if (reverse)
    {
        RA.setD(RA.Degrees() * 2 - abPos.ra);
        Dec.setD(Dec.Degrees() * 2 - abPos.dec);
    }
    else
    {
        RA.setD(abPos.ra);
        Dec.setD(abPos.dec);
    }

#else
    // N.B. These expressions are valid for FK5 coordinates
    // (presumably also valid for ICRS by extension), but not for
    // FK4. See the "Important Remark" on Page 152 of Jean Meeus'
    // book.

    // N.B. Even though Meeus does not say this explicitly, these
    // equations must not apply near the pole. Therefore, we fall-back
    // to the expressions provided by Meeus in ecliptic coordinates
    // (Equ 23.2) when we are near the pole.

    double K = num->constAberr().Degrees(); //constant of aberration
    double e = num->earthEccentricity(); // eccentricity of Earth's orbit

    if (fabs(Dec.Degrees()) < 80.0)
    {

        double cosRA, sinRA, cosDec, sinDec;
        double cosL, sinL, cosP, sinP;
        double cosOb, sinOb;


        RA.SinCos(sinRA, cosRA);
        Dec.SinCos(sinDec, cosDec);

        num->obliquity()->SinCos(sinOb, cosOb);
        // double tanOb = sinOb/cosOb;

        num->sunTrueLongitude().SinCos(sinL, cosL);
        num->earthPerihelionLongitude().SinCos(sinP, cosP);

        //Step 3: Aberration
        // These are the expressions given in Jean Meeus, Equation (23.3)

        // double dRA = -1.0 * K * ( cosRA * cosL * cosOb + sinRA * sinL )/cosDec
        //               + e * K * ( cosRA * cosP * cosOb + sinRA * sinP )/cosDec;

        // double dDec = -1.0 * K * ( cosL * cosOb * ( tanOb * cosDec - sinRA * sinDec ) + cosRA * sinDec * sinL )
        //                + e * K * ( cosP * cosOb * ( tanOb * cosDec - sinRA * sinDec ) + cosRA * sinDec * sinP );

        // However, we have factorized the expressions below by pulling
        // out common factors to make it more optimized, in case the
        // compiler fails to spot these optimizations.

        // N.B. I had failed to factor out the expressions correctly,
        // making mistakes, in c5e709bd91, which should now be
        // fixed. --asimha

        // FIXME: Check if the unit tests have sufficient coverage to
        // check this expression

        double dRA = (K / cosDec) * (
            cosRA * cosOb * (e * cosP - cosL)
            + sinRA * (e * sinP - sinL)
            );
        double dDec = K * (
            (sinOb * cosDec - cosOb * sinDec * sinRA) * (e * cosP - cosL)
            + cosRA * sinDec * (e * sinP - sinL)
            );

        // N.B. Meeus points out that the result has the same units as
        // K, so the corrections are in degrees.

        if (reverse)
        {
            dRA = -dRA;
            dDec = -dDec;
        }
        RA.setD(RA.Degrees() + dRA);
        Dec.setD(Dec.Degrees() + dDec);
    }
    else
    {
        dms EcLong, EcLat;
        double sinEcLat, cosEcLat;
        const auto &L = num->sunTrueLongitude();
        const auto &P = num->earthPerihelionLongitude();

        findEcliptic(num->obliquity(), EcLong, EcLat);
        EcLat.SinCos(sinEcLat, cosEcLat);

        double sin_L_minus_EcLong, cos_L_minus_EcLong;
        double sin_P_minus_EcLong, cos_P_minus_EcLong;
        (L - EcLong).SinCos(sin_L_minus_EcLong, cos_L_minus_EcLong);
        (P - EcLong).SinCos(sin_P_minus_EcLong, cos_P_minus_EcLong);

        // Equation (23.2) in Meeus
        // N.B. dEcLong, dEcLat are in degrees, because K is expressed in degrees.
        double dEcLong = (K / cosEcLat) * (e * cos_P_minus_EcLong - cos_L_minus_EcLong);
        double dEcLat = K * sinEcLat * (e * sin_P_minus_EcLong - sin_L_minus_EcLong);

        // Note: These are approximate corrections, so it is
        // appropriate to change their sign to reverse them to first
        // order in the corrections. As a result, the forward and
        // reverse calculations will not be exact inverses, but only
        // approximate inverses.
        if (reverse)
        {
            dEcLong = -dEcLong;
            dEcLat = -dEcLat;
        }

        // Update the ecliptic coordinates to their new values
        EcLong.setD(EcLong.Degrees() + dEcLong);
        EcLat.setD(EcLat.Degrees() + dEcLat);
        setFromEcliptic(num->obliquity(), EcLong, EcLat);
    }
#endif
}

// Note: This method is one of the major rate determining factors in how fast the map pans / zooms in or out
void SkyPoint::updateCoords(const KSNumbers *num, bool /*includePlanets*/, const CachingDms *lat, const CachingDms *LST,
                            bool forceRecompute)
{
    //Correct the catalog coordinates for the time-dependent effects
    //of precession, nutation and aberration
    bool recompute, lens;

    // NOTE: The same short-circuiting checks are also implemented in
    // StarObject::JITUpdate(), even before calling
    // updateCoords(). While this is code-duplication, these bits of
    // code need to be really optimized, at least for stars. For
    // optimization purposes, the code is left duplicated in two
    // places. Please be wary of changing one without changing the
    // other.

    Q_ASSERT(std::isfinite(lastPrecessJD));

    if (Options::useRelativistic() && checkBendLight())
    {
        recompute = true;
        lens      = true;
    }
    else
    {
        recompute = (Options::alwaysRecomputeCoordinates() || forceRecompute ||
                     std::abs(lastPrecessJD - num->getJD()) >= 0.00069444); // Update once per solar minute
        lens      = false;
    }
    if (recompute)
    {
        precess(num);
        nutate(num);
        if (lens)
            bendlight(); // FIXME: Shouldn't we apply this on the horizontal coordinates?
        aberrate(num);
        lastPrecessJD = num->getJD();
        Q_ASSERT(std::isfinite(RA.Degrees()) && std::isfinite(Dec.Degrees()));
    }

    if (lat || LST)
        qWarning() << i18n("lat and LST parameters should only be used in KSPlanetBase objects.");
}

void SkyPoint::precessFromAnyEpoch(long double jd0, long double jdf)
{
    double cosRA, sinRA, cosDec, sinDec;
    double v[3], s[3];

    RA  = RA0;
    Dec = Dec0; // Is this necessary?

    if (jd0 == jdf)
        return;

    RA.SinCos(sinRA, cosRA);
    Dec.SinCos(sinDec, cosDec);

    if (jd0 == B1950L)
    {
        B1950ToJ2000();
        jd0 = J2000L;
        RA.SinCos(sinRA, cosRA);
        Dec.SinCos(sinDec, cosDec);
    }

    if (jd0 != jdf)
    {
        // The original coordinate is referred to the FK5 system and
        // is NOT J2000.
        if (jd0 != J2000L)
        {
            //v is a column vector representing input coordinates.
            v[0] = cosRA * cosDec;
            v[1] = sinRA * cosDec;
            v[2] = sinDec;

            //Need to first precess to J2000.0 coords
            //s is the product of P1 and v; s represents the
            //coordinates precessed to J2000
            KSNumbers num(jd0);
            for (unsigned int i = 0; i < 3; ++i)
            {
                s[i] = num.p1(0, i) * v[0] + num.p1(1, i) * v[1] + num.p1(2, i) * v[2];
            }

            //Input coords already in J2000, set s accordingly.
        }
        else
        {
            s[0] = cosRA * cosDec;
            s[1] = sinRA * cosDec;
            s[2] = sinDec;
        }

        if (jdf == B1950L)
        {
            RA.setRadians(atan2(s[1], s[0]));
            Dec.setRadians(asin(s[2]));
            J2000ToB1950();

            return;
        }

        KSNumbers num(jdf);
        for (unsigned int i = 0; i < 3; ++i)
        {
            v[i] = num.p2(0, i) * s[0] + num.p2(1, i) * s[1] + num.p2(2, i) * s[2];
        }

        RA.setUsing_atan2(v[1], v[0]);
        Dec.setUsing_asin(v[2]);

        RA.reduceToRange(dms::ZERO_TO_2PI);

        return;
    }
}

void SkyPoint::apparentCoord(long double jd0, long double jdf)
{
    precessFromAnyEpoch(jd0, jdf);
    KSNumbers num(jdf);
    nutate(&num);
    if (Options::useRelativistic() && checkBendLight())
        bendlight();
    aberrate(&num);
}

SkyPoint SkyPoint::catalogueCoord(long double jdf)
{
    KSNumbers num(jdf);

    // remove abberation
    aberrate(&num, true);

    // remove nutation
    nutate(&num, true);

    // remove precession
    // the start position needs to be in RA0,Dec0
    RA0 = RA;
    Dec0 = Dec;
    // from now to J2000
    precessFromAnyEpoch(jdf, static_cast<long double>(J2000));
    // the J2000 position is in RA,Dec, move to RA0, Dec0
    RA0 = RA;
    Dec0 = Dec;
    lastPrecessJD = J2000;

    SkyPoint sp(RA0, Dec0);
    return sp;
}

void SkyPoint::Equatorial1950ToGalactic(dms &galLong, dms &galLat)
{
    double a = 192.25;
    double sinb, cosb, sina_RA, cosa_RA, sinDEC, cosDEC, tanDEC;

    dms c(303.0);
    dms b(27.4);
    tanDEC = tan(Dec.radians());

    b.SinCos(sinb, cosb);
    dms(a - RA.Degrees()).SinCos(sina_RA, cosa_RA);
    Dec.SinCos(sinDEC, cosDEC);

    galLong.setRadians(c.radians() - atan2(sina_RA, cosa_RA * sinb - tanDEC * cosb));
    galLong.reduceToRange(dms::ZERO_TO_2PI);

    galLat.setRadians(asin(sinDEC * sinb + cosDEC * cosb * cosa_RA));
}

void SkyPoint::GalacticToEquatorial1950(const dms *galLong, const dms *galLat)
{
    double a = 123.0;
    double sinb, cosb, singLat, cosgLat, tangLat, singLong_a, cosgLong_a;

    dms c(12.25);
    dms b(27.4);
    tangLat = tan(galLat->radians());
    galLat->SinCos(singLat, cosgLat);

    dms(galLong->Degrees() - a).SinCos(singLong_a, cosgLong_a);
    b.SinCos(sinb, cosb);

    RA.setRadians(c.radians() + atan2(singLong_a, cosgLong_a * sinb - tangLat * cosb));
    RA.reduceToRange(dms::ZERO_TO_2PI);

    Dec.setRadians(asin(singLat * sinb + cosgLat * cosb * cosgLong_a));
}

void SkyPoint::B1950ToJ2000(void)
{
    double cosRA, sinRA, cosDec, sinDec;
    //	double cosRA0, sinRA0, cosDec0, sinDec0;
    double v[3], s[3];

    // 1984 January 1 0h
    KSNumbers num(2445700.5L);

    // Eterms due to aberration
    addEterms();
    RA.SinCos(sinRA, cosRA);
    Dec.SinCos(sinDec, cosDec);

    // Precession from B1950 to J1984
    s[0] = cosRA * cosDec;
    s[1] = sinRA * cosDec;
    s[2] = sinDec;
    for (unsigned int i = 0; i < 3; ++i)
    {
        v[i] = num.p2b(0, i) * s[0] + num.p2b(1, i) * s[1] + num.p2b(2, i) * s[2];
    }

    // RA zero-point correction at 1984 day 1, 0h.
    RA.setRadians(atan2(v[1], v[0]));
    Dec.setRadians(asin(v[2]));

    RA.setH(RA.Hours() + 0.06390 / 3600.);
    RA.SinCos(sinRA, cosRA);
    Dec.SinCos(sinDec, cosDec);

    s[0] = cosRA * cosDec;
    s[1] = sinRA * cosDec;
    s[2] = sinDec;

    // Precession from 1984 to J2000.

    for (unsigned int i = 0; i < 3; ++i)
    {
        v[i] = num.p1(0, i) * s[0] + num.p1(1, i) * s[1] + num.p1(2, i) * s[2];
    }

    RA.setRadians(atan2(v[1], v[0]));
    Dec.setRadians(asin(v[2]));
}

void SkyPoint::J2000ToB1950(void)
{
    double cosRA, sinRA, cosDec, sinDec;
    //	double cosRA0, sinRA0, cosDec0, sinDec0;
    double v[3], s[3];

    // 1984 January 1 0h
    KSNumbers num(2445700.5L);

    RA.SinCos(sinRA, cosRA);
    Dec.SinCos(sinDec, cosDec);

    s[0] = cosRA * cosDec;
    s[1] = sinRA * cosDec;
    s[2] = sinDec;

    // Precession from J2000 to 1984 day, 0h.

    for (unsigned int i = 0; i < 3; ++i)
    {
        v[i] = num.p2(0, i) * s[0] + num.p2(1, i) * s[1] + num.p2(2, i) * s[2];
    }

    RA.setRadians(atan2(v[1], v[0]));
    Dec.setRadians(asin(v[2]));

    // RA zero-point correction at 1984 day 1, 0h.

    RA.setH(RA.Hours() - 0.06390 / 3600.);
    RA.SinCos(sinRA, cosRA);
    Dec.SinCos(sinDec, cosDec);

    // Precession from B1950 to J1984

    s[0] = cosRA * cosDec;
    s[1] = sinRA * cosDec;
    s[2] = sinDec;
    for (unsigned int i = 0; i < 3; ++i)
    {
        v[i] = num.p1b(0, i) * s[0] + num.p1b(1, i) * s[1] + num.p1b(2, i) * s[2];
    }

    RA.setRadians(atan2(v[1], v[0]));
    Dec.setRadians(asin(v[2]));

    // Eterms due to aberration
    subtractEterms();
}

SkyPoint SkyPoint::Eterms(void)
{
    double sd, cd, sinEterm, cosEterm;
    dms raTemp, raDelta, decDelta;

    Dec.SinCos(sd, cd);
    raTemp.setH(RA.Hours() + 11.25);
    raTemp.SinCos(sinEterm, cosEterm);

    raDelta.setH(0.0227 * sinEterm / (3600. * cd));
    decDelta.setD(0.341 * cosEterm * sd / 3600. + 0.029 * cd / 3600.);

    return SkyPoint(raDelta, decDelta);
}

void SkyPoint::addEterms(void)
{
    SkyPoint spd = Eterms();

    RA  = RA + spd.ra();
    Dec = Dec + spd.dec();
}

void SkyPoint::subtractEterms(void)
{
    SkyPoint spd = Eterms();

    RA  = RA - spd.ra();
    Dec = Dec - spd.dec();
}

dms SkyPoint::angularDistanceTo(const SkyPoint *sp, double *const positionAngle) const
{
    // double dalpha = sp->ra().radians() - ra().radians() ;
    // double ddelta = sp->dec().radians() - dec().radians();
    CachingDms dalpha = sp->ra() - ra();
    CachingDms ddelta = sp->dec() - dec();

    // double sa = sin(dalpha/2.);
    // double sd = sin(ddelta/2.);

    // double hava = sa*sa;
    // double havd = sd*sd;

    // Compute the haversin directly:
    double hava = (1 - dalpha.cos()) / 2.;
    double havd = (1 - ddelta.cos()) / 2.;

    // Haversine law
    double aux = havd + (sp->dec().cos()) * dec().cos() * hava;

    dms angDist;
    angDist.setRadians(2. * fabs(asin(sqrt(aux))));

    if (positionAngle)
    {
        // Also compute the position angle of the line from this SkyPoint to sp
        //*positionAngle = acos( tan(-ddelta)/tan( angDist.radians() ) ); // FIXME: Might fail for large ddelta / zero angDist
        //if( -dalpha < 0 )
        //            *positionAngle = 2*M_PI - *positionAngle;
        *positionAngle =
            atan2f(dalpha.sin(), (dec().cos()) * tan(sp->dec().radians()) - (dec().sin()) * dalpha.cos()) * 180 / M_PI;
    }
    return angDist;
}

double SkyPoint::vRSun(long double jd0)
{
    double ca, sa, cd, sd, vsun;
    double cosRA, sinRA, cosDec, sinDec;

    /* Sun apex (where the sun goes) coordinates */

    dms asun(270.9592); // Right ascention: 18h 3m 50.2s [J2000]
    dms dsun(30.00467); // Declination: 30^o 0' 16.8'' [J2000]
    vsun = 20.;         // [km/s]

    asun.SinCos(sa, ca);
    dsun.SinCos(sd, cd);

    /* We need an auxiliary SkyPoint since we need the
    * source referred to the J2000 equinox and we do not want to overwrite
    * the current values
    */

    SkyPoint aux;
    aux.set(RA0, Dec0);

    aux.precessFromAnyEpoch(jd0, J2000L);

    aux.ra().SinCos(sinRA, cosRA);
    aux.dec().SinCos(sinDec, cosDec);

    /* Computation is done performing the scalar product of a unitary vector
    in the direction of the source with the vector velocity of Sun, both being in the
    LSR reference system:
    Vlsr	= Vhel + Vsun.u_radial =>
    Vlsr 	= Vhel + vsun(cos D cos A,cos D sen A,sen D).(cos d cos a,cos d sen a,sen d)
    Vhel 	= Vlsr - Vsun.u_radial
    */

    return vsun * (cd * cosDec * (cosRA * ca + sa * sinRA) + sd * sinDec);
}

double SkyPoint::vHeliocentric(double vlsr, long double jd0)
{
    return vlsr - vRSun(jd0);
}

double SkyPoint::vHelioToVlsr(double vhelio, long double jd0)
{
    return vhelio + vRSun(jd0);
}

double SkyPoint::vREarth(long double jd0)
{
    double sinRA, sinDec, cosRA, cosDec;

    /* u_radial = unitary vector in the direction of the source
        Vlsr 	= Vhel + Vsun.u_radial
            = Vgeo + VEarth.u_radial + Vsun.u_radial  =>

        Vgeo 	= (Vlsr -Vsun.u_radial) - VEarth.u_radial
            =  Vhel - VEarth.u_radial
            =  Vhel - (vx, vy, vz).(cos d cos a,cos d sen a,sen d)
    */

    /* We need an auxiliary SkyPoint since we need the
    * source referred to the J2000 equinox and we do not want to overwrite
    * the current values
    */

    SkyPoint aux(RA0, Dec0);

    aux.precessFromAnyEpoch(jd0, J2000L);

    aux.ra().SinCos(sinRA, cosRA);
    aux.dec().SinCos(sinDec, cosDec);

    /* vEarth is referred to the J2000 equinox, hence we need that
    the source coordinates are also in the same reference system.
    */

    KSNumbers num(jd0);
    return num.vEarth(0) * cosDec * cosRA + num.vEarth(1) * cosDec * sinRA + num.vEarth(2) * sinDec;
}

double SkyPoint::vGeocentric(double vhelio, long double jd0)
{
    return vhelio - vREarth(jd0);
}

double SkyPoint::vGeoToVHelio(double vgeo, long double jd0)
{
    return vgeo + vREarth(jd0);
}

double SkyPoint::vRSite(double vsite[3])
{
    double sinRA, sinDec, cosRA, cosDec;

    RA.SinCos(sinRA, cosRA);
    Dec.SinCos(sinDec, cosDec);

    return vsite[0] * cosDec * cosRA + vsite[1] * cosDec * sinRA + vsite[2] * sinDec;
}

double SkyPoint::vTopoToVGeo(double vtopo, double vsite[3])
{
    return vtopo + vRSite(vsite);
}

double SkyPoint::vTopocentric(double vgeo, double vsite[3])
{
    return vgeo - vRSite(vsite);
}

bool SkyPoint::checkCircumpolar(const dms *gLat) const
{
    return fabs(dec().Degrees()) > (90 - fabs(gLat->Degrees()));
}

dms SkyPoint::altRefracted() const
{
    return refract(Alt, Options::useRefraction());
}

void SkyPoint::setAltRefracted(dms alt_apparent)
{
    setAlt(unrefract(alt_apparent, Options::useRefraction()));
}

void SkyPoint::setAltRefracted(double alt_apparent)
{
    setAlt(unrefract(alt_apparent, Options::useRefraction()));
}

double SkyPoint::refractionCorr(double alt)
{
    return 1.02 / tan(dms::DegToRad * (alt + 10.3 / (alt + 5.11))) / 60;
}

double SkyPoint::refract(const double alt, bool conditional)
{
    if (!conditional)
    {
        return alt;
    }
    static double corrCrit = SkyPoint::refractionCorr(SkyPoint::altCrit);

    if (alt > SkyPoint::altCrit)
        return (alt + SkyPoint::refractionCorr(alt));
    else
        return (alt +
                corrCrit * (alt + 90) /
                (SkyPoint::altCrit + 90)); // Linear extrapolation from corrCrit at altCrit to 0 at -90 degrees
}

// Found uncorrected value by solving equation. This is OK since
// unrefract is never called in loops with the potential exception of
// slewing.
//
// Convergence is quite fast just a few iterations.
double SkyPoint::unrefract(const double alt, bool conditional)
{
    if (!conditional)
    {
        return alt;
    }
    double h0 = alt;
    double h1 =
        alt -
        (refract(h0) -
         h0); // It's probably okay to add h0 in refract() and subtract it here, since refract() is called way more frequently.

    while (fabs(h1 - h0) > 1e-4)
    {
        h0 = h1;
        h1 = alt - (refract(h0) - h0);
    }
    return h1;
}

dms SkyPoint::findAltitude(const SkyPoint *p, const KStarsDateTime &dt, const GeoLocation *geo, const double hour)
{
    Q_ASSERT(p);
    if (!p)
        return dms(NaN::d);

    // Jasem 2015-08-24 Using correct procedure to find altitude
    return SkyPoint::timeTransformed(p, dt, geo, hour).alt();
}

SkyPoint SkyPoint::timeTransformed(const SkyPoint *p, const KStarsDateTime &dt, const GeoLocation *geo,
                                   const double hour)
{
    Q_ASSERT(p);
    if (!p)
        return SkyPoint(NaN::d, NaN::d);

    // Jasem 2015-08-24 Using correct procedure to find altitude
    SkyPoint sp                   = *p; // make a copy
    KStarsDateTime targetDateTime = dt.addSecs(hour * 3600.0);
    dms LST                       = geo->GSTtoLST(targetDateTime.gst());
    sp.EquatorialToHorizontal(&LST, geo->lat());
    return sp;
}

double SkyPoint::maxAlt(const dms &lat) const
{
    double retval = (lat.Degrees() + 90. - dec().Degrees());
    if (retval > 90.)
        retval = 180. - retval;
    return retval;
}

double SkyPoint::minAlt(const dms &lat) const
{
    double retval = (lat.Degrees() - 90. + dec().Degrees());
    if (retval < -90.)
        retval = 180. + retval;
    return retval;
}

#ifndef KSTARS_LITE
QDBusArgument &operator<<(QDBusArgument &argument, const SkyPoint &source)
{
    argument.beginStructure();
    argument << source.ra().Hours() << source.dec().Degrees();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SkyPoint &dest)
{
    double ra, dec;
    argument.beginStructure();
    argument >> ra >> dec;
    argument.endStructure();
    dest = SkyPoint(ra, dec);
    return argument;
}
#endif

