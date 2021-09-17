/*
    SPDX-FileCopyrightText: 2001-2005 Jason Harris <jharris@30doradus.org>
    SPDX-FileCopyrightText: 2004-2005 Pablo de Vicente <p.devicente@wanadoo.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "cachingdms.h"
#include "kstarsdatetime.h"

#include <QList>
#ifndef KSTARS_LITE
#include <QtDBus/QtDBus>
#endif

//#define PROFILE_COORDINATE_CONVERSION

class KSNumbers;
class KSSun;
class GeoLocation;

/**
 * @class SkyPoint
 *
 * The sky coordinates of a point in the sky.  The
 * coordinates are stored in both Equatorial (Right Ascension,
 * Declination) and Horizontal (Azimuth, Altitude) coordinate systems.
 * Provides set/get functions for each coordinate angle, and functions
 * to convert between the Equatorial and Horizon coordinate systems.
 *
 * Because the coordinate values change slowly over time (due to
 * precession, nutation), the "catalog coordinates" are stored
 * (RA0, Dec0), which were the true coordinates on Jan 1, 2000.
 * The true coordinates (RA, Dec) at any other epoch can be found
 * from the catalog coordinates using updateCoords().
 * @short Stores dms coordinates for a point in the sky.
 * for converting between coordinate systems.
 *
 * @author Jason Harris
 * @version 1.0
 */
class SkyPoint
{
    public:
        /**
         * Default constructor: Sets RA, Dec and RA0, Dec0 according
         * to arguments.  Does not set Altitude or Azimuth.
         *
         * @param r Right Ascension
         * @param d Declination
         */
        SkyPoint(const dms &r, const dms &d) : RA0(r), Dec0(d), RA(r), Dec(d), lastPrecessJD(J2000) {}

        SkyPoint(const CachingDms &r, const CachingDms &d) : RA0(r), Dec0(d), RA(r), Dec(d), lastPrecessJD(J2000) {}

        /**
         * Alternate constructor using double arguments, for convenience.
         * It behaves essentially like the default constructor.
         *
         * @param r Right Ascension, expressed as a double
         * @param d Declination, expressed as a double
         * @note This also sets RA0 and Dec0
         */
        //FIXME: this (*15.0) thing is somewhat hacky.
        explicit SkyPoint(double r, double d) : RA0(r * 15.0), Dec0(d), RA(r * 15.0), Dec(d), lastPrecessJD(J2000) {}

        /** @short Default constructor. Sets nonsense values for RA, Dec etc */
        SkyPoint();

        virtual ~SkyPoint() = default;

        ////
        //// 1.  Setting Coordinates
        //// =======================

        /**
         * @short Sets RA, Dec and RA0, Dec0 according to arguments.
         * Does not set Altitude or Azimuth.
         *
         * @param r Right Ascension
         * @param d Declination
         * @note This function also sets RA0 and Dec0 to the same values, so call at your own peril!
         * @note FIXME: This method must be removed, or an epoch argument must be added.
         */
        void set(const dms &r, const dms &d);

        /**
         * Sets RA0, the catalog Right Ascension.
         *
         * @param r catalog Right Ascension.
         */
        inline void setRA0(dms r)
        {
            RA0 = r;
        }
        inline void setRA0(CachingDms r)
        {
            RA0 = r;
        }

        /**
         * Overloaded member function, provided for convenience.
         * It behaves essentially like the above function.
         *
         * @param r Right Ascension, expressed as a double.
         */
        inline void setRA0(double r)
        {
            RA0.setH(r);
        }

        /**
         * Sets Dec0, the catalog Declination.
         *
         * @param d catalog Declination.
         */
        inline void setDec0(dms d)
        {
            Dec0 = d;
        }
        inline void setDec0(const CachingDms &d)
        {
            Dec0 = d;
        }

        /**
         * Overloaded member function, provided for convenience.
         * It behaves essentially like the above function.
         *
         * @param d Declination, expressed as a double.
         */
        inline void setDec0(double d)
        {
            Dec0.setD(d);
        }

        /**
         * Sets RA, the current Right Ascension.
         *
         * @param r Right Ascension.
         */
        inline void setRA(dms &r)
        {
            RA = r;
        }
        inline void setRA(const CachingDms &r)
        {
            RA = r;
        }

        /**
         * Overloaded member function, provided for convenience.
         * It behaves essentially like the above function.
         *
         * @param r Right Ascension, expressed as a double.
         */
        inline void setRA(double r)
        {
            RA.setH(r);
        }

        /**
         * Sets Dec, the current Declination
         *
         * @param d Declination.
         */
        inline void setDec(dms d)
        {
            Dec = d;
        }
        inline void setDec(const CachingDms &d)
        {
            Dec = d;
        }

        /**
         * Overloaded member function, provided for convenience.
         * It behaves essentially like the above function.
         *
         * @param d Declination, expressed as a double.
         */
        inline void setDec(double d)
        {
            Dec.setD(d);
        }

        /**
         * Sets Alt, the Altitude.
         *
         * @param alt Altitude.
         */
        inline void setAlt(dms alt)
        {
            Alt = alt;
        }

        /**
         * Sets the apparent altitude, checking whether refraction corrections are enabled
         *
         * @param alt_apparent Apparent altitude (subject to Options::useRefraction())
         */
        void setAltRefracted(dms alt_apparent);

        /**
         * Overloaded member function, provided for convenience.
         * It behaves essentially like the above function.
         *
         * @param alt_apparent Apparent altitude (subject to Options::useRefraction())
         */
        void setAltRefracted(double alt_apparent);

        /**
         * Overloaded member function, provided for convenience.
         * It behaves essentially like the above function.
         *
         * @param alt Altitude, expressed as a double.
         */
        inline void setAlt(double alt)
        {
            Alt.setD(alt);
        }

        /**
         * Sets Az, the Azimuth.
         *
         * @param az Azimuth.
         */
        inline void setAz(dms az)
        {
            Az = az;
        }

        /**
         * Overloaded member function, provided for convenience.
         * It behaves essentially like the above function.
         *
         * @param az Azimuth, expressed as a double.
         */
        inline void setAz(double az)
        {
            Az.setD(az);
        }

        ////
        //// 2. Returning coordinates.
        //// =========================

        /** @return a pointer to the catalog Right Ascension. */
        inline const CachingDms &ra0() const
        {
            return RA0;
        }

        /** @return a pointer to the catalog Declination. */
        inline const CachingDms &dec0() const
        {
            return Dec0;
        }

        /** @returns a pointer to the current Right Ascension. */
        inline const CachingDms &ra() const
        {
            return RA;
        }

        /** @return a pointer to the current Declination. */
        inline const CachingDms &dec() const
        {
            return Dec;
        }

        /** @return a pointer to the current Azimuth. */
        inline const dms &az() const
        {
            return Az;
        }

        /** @return a pointer to the current Altitude. */
        inline const dms &alt() const
        {
            return Alt;
        }

        /**
         * @return refracted altitude. This function uses
         * Options::useRefraction to determine whether refraction
         * correction should be applied
         */
        dms altRefracted() const;

        /** @return the JD for the precessed coordinates */
        inline double getLastPrecessJD() const
        {
            return lastPrecessJD;
        }

        /**
         * @return the airmass of the point. Convenience method.
         * @note Question: is it better to use alt or refracted alt? Minor difference, probably doesn't matter.
         */
        inline double airmass() const
        {
            return 1. / sin(alt().radians());
        }

        ////
        //// 3. Coordinate conversions.
        //// ==========================

        /**
         * Determine the (Altitude, Azimuth) coordinates of the
         * SkyPoint from its (RA, Dec) coordinates, given the local
         * sidereal time and the observer's latitude.
         * @param LST pointer to the local sidereal time
         * @param lat pointer to the geographic latitude
         */
        void EquatorialToHorizontal(const CachingDms *LST, const CachingDms *lat);

        // Deprecated method provided for compatibility
        void EquatorialToHorizontal(const dms *LST, const dms *lat);

        /**
         * Determine the (RA, Dec) coordinates of the
         * SkyPoint from its (Altitude, Azimuth) coordinates, given the local
         * sidereal time and the observer's latitude.
         *
         * @param LST pointer to the local sidereal time
         * @param lat pointer to the geographic latitude
         */
        void HorizontalToEquatorial(const dms *LST, const dms *lat);

        /**
         * Determine the Ecliptic coordinates of the SkyPoint, given the Julian Date.
         * The ecliptic coordinates are returned as reference arguments (since
         * they are not stored internally)
         */
        void findEcliptic(const CachingDms *Obliquity, dms &EcLong, dms &EcLat);

        /**
         * Set the current (RA, Dec) coordinates of the
         * SkyPoint, given pointers to its Ecliptic (Long, Lat) coordinates, and
         * to the current obliquity angle (the angle between the equator and ecliptic).
         */
        void setFromEcliptic(const CachingDms *Obliquity, const dms &EcLong, const dms &EcLat);

        /**
         * Computes galactic coordinates from equatorial coordinates referred to
         * epoch 1950. RA and Dec are, therefore assumed to be B1950 coordinates.
         */
        void Equatorial1950ToGalactic(dms &galLong, dms &galLat);

        /**
         * Computes equatorial coordinates referred to 1950 from galactic ones referred to
         * epoch B1950. RA and Dec are, therefore assumed to be B1950 coordinates.
         */
        void GalacticToEquatorial1950(const dms *galLong, const dms *galLat);

        ////
        //// 4. Coordinate update/corrections.
        //// =================================

        /**
         * Determine the current coordinates (RA, Dec) from the catalog
         * coordinates (RA0, Dec0), accounting for both precession and nutation.
         * @param num pointer to KSNumbers object containing current values of time-dependent variables.
         * @param includePlanets does nothing in this implementation (see KSPlanetBase::updateCoords()).
         * @param lat does nothing in this implementation (see KSPlanetBase::updateCoords()).
         * @param LST does nothing in this implementation (see KSPlanetBase::updateCoords()).
         * @param forceRecompute reapplies precession, nutation and aberration even if the time passed
         * since the last computation is not significant.
         */
        virtual void updateCoords(const KSNumbers *num, bool includePlanets = true, const CachingDms *lat = nullptr,
                                  const CachingDms *LST = nullptr, bool forceRecompute = false);

        /**
         * @brief updateCoordsNow Shortcut for updateCoords( const KSNumbers *num, false, nullptr, nullptr, true)
         *
         * @param num pointer to KSNumbers object containing current values of time-dependent variables.
         */
        virtual void updateCoordsNow(const KSNumbers *num)
        {
            updateCoords(num, false, nullptr, nullptr, true);
        }

        /**
         * Computes the apparent coordinates for this SkyPoint for any epoch,
         * accounting for the effects of precession, nutation, and aberration.
         * Similar to updateCoords(), but the starting epoch need not be
         * J2000, and the target epoch need not be the present time.
         *
         * @param jd0 Julian Day which identifies the original epoch
         * @param jdf Julian Day which identifies the final epoch
         */
        void apparentCoord(long double jd0, long double jdf);

        /**
         * Computes the J2000.0 catalogue coordinates for this SkyPoint using the epoch
         * removing aberration, nutation and precession
         * Catalogue coordinates are in Ra0, Dec0 as well as Ra, Dec and lastPrecessJD is set to J2000.0
         *
         * @FIXME We do not undo nutation and aberration
         * @brief catalogueCoord converts observed to J2000 using epoch jdf
         * @param jdf Julian Day which identifies the current epoch
         * @return SpyPoint containing J2000 coordinates
         */

        SkyPoint catalogueCoord(long double jdf);


        /**
         * Apply the effects of nutation to this SkyPoint.
         *
         * @param num pointer to KSNumbers object containing current values of
         * time-dependent variables.
         * @param reverse bool, if true the nutation is removed
         */
        void nutate(const KSNumbers *num, const bool reverse = false);

        /**
         * @short Check if this sky point is close enough to the sun for
         * gravitational lensing to be significant
         */
        bool checkBendLight();

        /**
         * Correct for the effect of "bending" of light around the sun for
         * positions near the sun.
         *
         * General Relativity tells us that a photon with an impact
         * parameter b is deflected through an angle 1.75" (Rs / b) where
         * Rs is the solar radius.
         *
         * @return: true if the light was bent, false otherwise
         */
        bool bendlight();

        /**
         * @short Obtain a Skypoint with RA0 and Dec0 set from the RA, Dec
         * of this skypoint. Also set the RA0, Dec0 of this SkyPoint if not
         * set already and the target epoch is J2000.
         */
        SkyPoint deprecess(const KSNumbers *num, long double epoch = J2000);

        /**
         * Determine the effects of aberration for this SkyPoint.
         *
         * @param num pointer to KSNumbers object containing current values of
         * time-dependent variables.
         * @param reverse bool, if true the aberration is removed.
         */
        void aberrate(const KSNumbers *num, bool reverse = false);

        /**
         * General case of precession. It precess from an original epoch to a
         * final epoch. In this case RA0, and Dec0 from SkyPoint object represent
         * the coordinates for the original epoch and not for J2000, as usual.
         *
         * @param jd0 Julian Day which identifies the original epoch
         * @param jdf Julian Day which identifies the final epoch
         */
        void precessFromAnyEpoch(long double jd0, long double jdf);

        /**
         * Determine the E-terms of aberration
         * In the past, the mean places of stars published in catalogs included
         * the contribution to the aberration due to the ellipticity of the orbit
         * of the Earth. These terms, known as E-terms were almost constant, and
         * in the newer catalogs (FK5) are not included. Therefore to convert from
         * FK4 to FK5 one has to compute these E-terms.
         */
        SkyPoint Eterms(void);

        /**
         * Exact precession from Besselian epoch 1950 to epoch J2000. The
         * coordinates referred to the first epoch are in the
         * FK4 catalog, while the latter are in the Fk5 one.
         *
         * Reference: Smith, C. A.; Kaplan, G. H.; Hughes, J. A.; Seidelmann,
         * P. K.; Yallop, B. D.; Hohenkerk, C. Y.
         * Astronomical Journal, vol. 97, Jan. 1989, p. 265-279
         *
         * This transformation requires 4 steps:
         * - Correct E-terms
         * - Precess from B1950 to 1984, January 1st, 0h, using Newcomb expressions
         * - Add zero point correction in right ascension for 1984
         * - Precess from 1984, January 1st, 0h to J2000
         */
        void B1950ToJ2000(void);

        /**
         * Exact precession from epoch J2000 Besselian epoch 1950. The coordinates
         * referred to the first epoch are in the FK4 catalog, while the
         * latter are in the Fk5 one.
         *
         * Reference: Smith, C. A.; Kaplan, G. H.; Hughes, J. A.; Seidelmann,
         * P. K.; Yallop, B. D.; Hohenkerk, C. Y.
         * Astronomical Journal, vol. 97, Jan. 1989, p. 265-279
         *
         * This transformation requires 4 steps:
         * - Precess from J2000 to 1984, January 1st, 0h
         * - Add zero point correction in right ascension for 1984
         * - Precess from 1984, January 1st, 0h, to B1950 using Newcomb expressions
         * - Correct E-terms
         */
        void J2000ToB1950(void);

        /**
         * Coordinates in the FK4 catalog include the effect of aberration due
         * to the ellipticity of the orbit of the Earth. Coordinates in the FK5
         * catalog do not include these terms. In order to convert from B1950 (FK4)
         * to actual mean places one has to use this function.
         */
        void addEterms(void);

        /**
         * Coordinates in the FK4 catalog include the effect of aberration due
         * to the ellipticity of the orbit of the Earth. Coordinates in the FK5
         * catalog do not include these terms. In order to convert from
         * FK5 coordinates to B1950 (FK4) one has to use this function.
         */
        void subtractEterms(void);

        /**
         * Computes the angular distance between two SkyObjects. The algorithm
         * to compute this distance is:
         * cos(distance) = sin(d1)*sin(d2) + cos(d1)*cos(d2)*cos(a1-a2)
         * where a1,d1 are the coordinates of the first object and a2,d2 are
         * the coordinates of the second object.
         * However this algorithm is not accurate when the angular separation is small.
         * Meeus provides a different algorithm in page 111 which we implement here.
         *
         * @param sp SkyPoint to which distance is to be calculated
         * @param positionAngle if a non-null pointer is passed, the position angle [E of N]
         * in degrees from this SkyPoint to sp is computed and stored in the passed variable.
         * @return dms angle representing angular separation.
         **/
        dms angularDistanceTo(const SkyPoint *sp, double *const positionAngle = nullptr) const;

        /** @return returns true if _current_ epoch RA / Dec match */
        inline bool operator==(SkyPoint &p) const
        {
            return (ra() == p.ra() && dec() == p.dec());
        }

        /**
         * Computes the velocity of the Sun projected on the direction of the source.
         *
         * @param jd Epoch expressed as julian day to which the source coordinates refer to.
         * @return Radial velocity of the source referred to the barycenter of the solar system in km/s
         **/
        double vRSun(long double jd);

        /**
         * Computes the radial velocity of a source referred to the solar system barycenter
         * from the radial velocity referred to the
         * Local Standard of Rest, aka known as VLSR. To compute it we need the coordinates of the
         * source the VLSR and the epoch for the source coordinates.
         *
         * @param vlsr radial velocity of the source referred to the LSR in km/s
         * @param jd Epoch expressed as julian day to which the source coordinates refer to.
         * @return Radial velocity of the source referred to the barycenter of the solar system in km/s
         **/
        double vHeliocentric(double vlsr, long double jd);

        /**
         * Computes the radial velocity of a source referred to the Local Standard of Rest, also known as VLSR
         * from the radial velocity referred to the solar system barycenter
         *
         * @param vhelio radial velocity of the source referred to the LSR in km/s
         * @param jd Epoch expressed as julian day to which the source coordinates refer to.
         * @return Radial velocity of the source referred to the barycenter of the solar system in km/s
         **/
        double vHelioToVlsr(double vhelio, long double jd);

        /**
         * Computes the velocity of any object projected on the direction of the source.
         *
         * @param jd0 Julian day for which we compute the direction of the source
         * @return velocity of the Earth projected on the direction of the source kms-1
         */
        double vREarth(long double jd0);

        /**
         * Computes the radial velocity of a source referred to the center of the earth
         * from the radial velocity referred to the solar system barycenter
         *
         * @param vhelio radial velocity of the source referred to the barycenter of the
         *               solar system in km/s
         * @param jd     Epoch expressed as julian day to which the source coordinates refer to.
         * @return Radial velocity of the source referred to the center of the Earth in km/s
         **/
        double vGeocentric(double vhelio, long double jd);

        /**
         * Computes the radial velocity of a source referred to the solar system barycenter
         * from the velocity referred to the center of the earth
         *
         * @param vgeo   radial velocity of the source referred to the center of the Earth [km/s]
         * @param jd     Epoch expressed as julian day to which the source coordinates refer to.
         * @return Radial velocity of the source referred to the solar system barycenter in km/s
         **/
        double vGeoToVHelio(double vgeo, long double jd);

        /**
         * Computes the velocity of any object (observer's site) projected on the
         * direction of the source.
         *
         * @param vsite velocity of that object in cartesian coordinates
         * @return velocity of the object projected on the direction of the source kms-1
         */
        double vRSite(double vsite[3]);

        /**
         * Computes the radial velocity of a source referred to the observer site on the surface
         * of the earth from the geocentric velocity and the velocity of the site referred to the center
         * of the Earth.
         *
         * @param vgeo radial velocity of the source referred to the center of the earth in km/s
         * @param vsite Velocity at which the observer moves referred to the center of the earth.
         * @return Radial velocity of the source referred to the observer's site in km/s
         **/
        double vTopocentric(double vgeo, double vsite[3]);

        /**
         * Computes the radial velocity of a source referred to the center of the Earth from
         * the radial velocity referred to an observer site on the surface of the earth
         *
         * @param vtopo radial velocity of the source referred to the observer's site in km/s
         * @param vsite Velocity at which the observer moves referred to the center of the earth.
         * @return Radial velocity of the source referred the center of the earth in km/s
         **/
        double vTopoToVGeo(double vtopo, double vsite[3]);

        /**
         * Find the SkyPoint obtained by moving distance dist
         * (arcseconds) away from the givenSkyPoint
         *
         * @param dist Distance to move through in arcseconds
         * @param from The SkyPoint to move away from
         * @return a SkyPoint that is at the dist away from this SkyPoint in the direction away from
         */
        SkyPoint moveAway(const SkyPoint &from, double dist) const;

        /** @short Check if this point is circumpolar at the given geographic latitude */
        bool checkCircumpolar(const dms *gLat) const;

        /** Calculate refraction correction. Parameter and return value are in degrees */
        static double refractionCorr(double alt);

        /**
         * @short Apply refraction correction to altitude, depending on conditional
         *
         * @param alt altitude to be corrected, in degrees
         * @param conditional an optional boolean to decide whether to apply the correction or not
         * @note If conditional is false, this method returns its argument unmodified. This is a convenience feature as it is often needed to gate these corrections.
         * @return altitude after refraction correction (if applicable), in degrees
         */
        static double refract(const double alt, bool conditional = true);

        /**
         * @short Remove refraction correction, depending on conditional
         *
         * @param alt altitude from which refraction correction must be removed, in degrees
         * @param conditional an optional boolean to decide whether to undo the correction or not
         * @return altitude without refraction correction, in degrees
         * @note If conditional is false, this method returns its argument unmodified. This is a convenience feature as it is often needed to gate these corrections.
         */
        static double unrefract(const double alt, bool conditional = true);

        /**
         * @short Apply refraction correction to altitude. Overloaded method using
         * dms provided for convenience
         * @see SkyPoint::refract( const double alt )
         */
        static inline dms refract(const dms alt, bool conditional = true)
        {
            return dms(refract(alt.Degrees(), conditional));
        }

        /**
         * @short Remove refraction correction. Overloaded method using
         * dms provided for convenience
         * @see SkyPoint::unrefract( const double alt )
         */
        static inline dms unrefract(const dms alt, bool conditional = true)
        {
            return dms(unrefract(alt.Degrees(), conditional));
        }

        /**
         * @short Compute the altitude of a given skypoint hour hours from the given date/time
         *
         * @param p SkyPoint whose altitude is to be computed (const pointer, the method works on a clone)
         * @param dt Date/time that corresponds to 0 hour
         * @param geo GeoLocation object specifying the location
         * @param hour double specifying offset in hours from dt for which altitude is to be found
         * @return a dms containing (unrefracted?) altitude of the object at dt + hour hours at the given location
         *
         * @note This method is used in multiple places across KStars
         * @todo Fix code duplication in AltVsTime and KSAlmanac by using this method instead! FIXME.
         */
        static dms findAltitude(const SkyPoint *p, const KStarsDateTime &dt, const GeoLocation *geo, const double hour = 0);

        /**
         * @short returns a time-transformed SkyPoint. See SkyPoint::findAltitude() for details
         * @todo Fix this documentation.
         */
        static SkyPoint timeTransformed(const SkyPoint *p, const KStarsDateTime &dt, const GeoLocation *geo,
                                        const double hour = 0);

        /**
         * @short Critical height for atmospheric refraction
         * corrections. Below this, the height formula produces meaningless
         * results and the correction value is just interpolated.
         */
        static const double altCrit;

        /**
         * @short Return the object's altitude at the upper culmination for the given latitude
         *
         * @return the maximum altitude in degrees
         */
        double maxAlt(const dms &lat) const;

        /**
         * @short Return the object's altitude at the lower culmination for the given latitude
         *
         * @return the minimum altitude in degrees
         */
        double minAlt(const dms &lat) const;

#ifdef PROFILE_COORDINATE_CONVERSION
        static double cpuTime_EqToHz;
        static long unsigned eqToHzCalls;
#endif
        static bool implementationIsLibnova;

    protected:
        /**
         * Precess this SkyPoint's catalog coordinates to the epoch described by the
         * given KSNumbers object.
         *
         * @param num pointer to a KSNumbers object describing the target epoch.
         */
        void precess(const KSNumbers *num);

#ifdef UNIT_TEST
        friend class TestSkyPoint; // Test class
#endif

    private:
        CachingDms RA0, Dec0; //catalog coordinates
        CachingDms RA, Dec;   //current true sky coordinates
        dms Alt, Az;
        static KSSun *m_Sun;


        // long version of these epochs
#define J2000L          2451545.0L   //Julian Date for noon on Jan 1, 2000 (epoch J2000)
#define B1950L          2433282.4235L // Julian date for Jan 0.9235, 1950

    protected:
        double lastPrecessJD { 0 }; // JD at which the last coordinate  (see updateCoords) for this SkyPoint was done
};

#ifndef KSTARS_LITE
Q_DECLARE_METATYPE(SkyPoint)
QDBusArgument &operator<<(QDBusArgument &argument, const SkyPoint &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, SkyPoint &dest);
#endif
