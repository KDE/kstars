/*
    SPDX-FileCopyrightText: 2001-2005 Jason Harris <jharris@30doradus.org>
    SPDX-FileCopyrightText: 2003-2005 Pablo de Vicente <p.devicente@wanadoo.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KLocalizedString>

#include "cachingdms.h"
#include "timezonerule.h"
#include "kstarsdatetime.h"

/**
 * @class GeoLocation
 * Contains all relevant information for specifying a location
 * on Earth: City Name, State/Province name, Country Name,
 * Longitude, Latitude, Elevation, Time Zone, and Daylight Savings
 * Time rule.
 *
 * @short Relevant data about an observing location on Earth.
 * @author Jason Harris
 * @version 1.0
 */
class GeoLocation
{
    public:
        /** Constructor using dms objects to specify longitude and latitude.
             * @param lng the longitude
             * @param lat the latitude
             * @param name the name of the city/town/location
             * @param province the name of the province/US state
             * @param country the name of the country
             * @param TZ the base time zone offset from Greenwich, UK
             * @param TZrule pointer to the daylight savings time rule
             * @param elevation the elevation above sea level (in meters)
             * @param readOnly whether the location is read only or updatable.
             * @param iEllips type of geodetic ellipsoid model
             */
        GeoLocation(const dms &lng, const dms &lat, const QString &name = "Nowhere", const QString &province = "Nowhere",
                    const QString &country = "Nowhere", double TZ = 0, TimeZoneRule *TZrule = nullptr,
                    double elevation = -10, bool readOnly = false, int iEllips = 4);

        /** Constructor using doubles to specify X, Y and Z referred to the center of the Earth.
             * @param x the x-position, in m
             * @param y the y-position, in m
             * @param z the z-position, in m
             * @param name the name of the city/town/location
             * @param province the name of the province/US state
             * @param country the name of the country
             * @param TZ the base time zone offset from Greenwich, UK
             * @param TZrule pointer to the daylight savings time rule
             * @param elevation the elevation above sea level (in meters)
             * @param readOnly whether the location is read only or updatable.
             * @param iEllips type of geodetic ellipsoid model
             */
        GeoLocation(double x, double y, double z, const QString &name = "Nowhere", const QString &province = "Nowhere",
                    const QString &country = "Nowhere", double TZ = 0, TimeZoneRule *TZrule = nullptr,
                    double elevation = -10, bool readOnly = false, int iEllips = 4);

        /** @return pointer to the longitude dms object */
        const CachingDms *lng() const
        {
            return &Longitude;
        }

        /** @return pointer to the latitude dms object */
        const CachingDms *lat() const
        {
            return &Latitude;
        }

        /** @return elevation above seal level (meters) */
        double elevation() const
        {
            return Elevation;
        }

        /** @return X position in m */
        double xPos() const
        {
            return PosCartX;
        }

        /** @return Y position in m */
        double yPos() const
        {
            return PosCartY;
        }

        /** @return Z position in m */
        double zPos() const
        {
            return PosCartZ;
        }

        /** @return index identifying the geodetic ellipsoid model */
        int ellipsoid() const
        {
            return indexEllipsoid;
        }

        /** @return untranslated City name */
        QString name() const
        {
            return Name;
        }

        /** @return translated City name */
        QString translatedName() const;

        /** @return untranslated Province name */
        QString province() const
        {
            return Province;
        }

        /** @return translated Province name */
        QString translatedProvince() const;

        /** @return untranslated Country name */
        QString country() const
        {
            return Country;
        }

        /** @return translated Country name */
        QString translatedCountry() const;

        /** @return comma-separated city, province, country names (each localized) */
        QString fullName() const;

        /** @return time zone without DST correction */
        double TZ0() const
        {
            return TimeZone;
        }

        /** @return time zone, including any DST correction. */
        double TZ() const
        {
            if (TZrule)
                return TimeZone + TZrule->deltaTZ();
            return TimeZone;
        }

        /** @return pointer to time zone rule object */
        TimeZoneRule *tzrule()
        {
            return TZrule;
        }

        /** Set Time zone.
             * @param value the new time zone */
        void setTZ0(double value)
        {
            TimeZone = value;
        }

        /** Set Time zone rule.
             * @param value pointer to the new time zone rule */
        void setTZRule(TimeZoneRule *value)
        {
            TZrule = value;
        }

        /** Set longitude according to dms argument.
             * @param l the new longitude */
        void setLong(const dms &l)
        {
            Longitude = l;
            geodToCart();
        }

        /** Set latitude according to dms argument.
             * @param l the new latitude
             */
        void setLat(const dms &l)
        {
            Latitude = l;
            geodToCart();
        }

        /** Set elevation above sea level
             * @param hg the new elevation (meters)
             */
        void setElevation(double hg)
        {
            Elevation = hg;
            geodToCart();
        }

        /** Set X
             * @param x the new x-position (meters)
             */
        void setXPos(double x)
        {
            PosCartX = x;
            cartToGeod();
        }
        /** Set Y
             * @param y the new y-position (meters)
             */
        void setYPos(double y)
        {
            PosCartY = y;
            cartToGeod();
        }
        /** Set Z
             * @param z the new z-position (meters)
             */
        void setZPos(double z)
        {
            PosCartZ = z;
            cartToGeod();
        }

        /** Update Latitude, Longitude and Height according to new ellipsoid. These are
             * computed from XYZ which do NOT change on changing the ellipsoid.
             * @param i index to identify the ellipsoid
             */
        void changeEllipsoid(int i);

        /** Set City name according to argument.
             * @param n new city name
             */
        void setName(const QString &n)
        {
            Name = n;
        }

        /** Set Province name according to argument.
             * @param n new province name
             */
        void setProvince(const QString &n)
        {
            Province = n;
        }

        /** Set Country name according to argument.
             * @param n new country name
             */
        void setCountry(const QString &n)
        {
            Country = n;
        }

        /** Converts from cartesian coordinates in meters to longitude,
             * latitude and height on a standard geoid for the Earth. The
             * geoid is characterized by two parameters: the semimajor axis
             * and the flattening.
             *
             * @note The astronomical zenith is defined as the perpendicular to
             * the real geoid. The geodetic zenith is the perpendicular to the
             * standard geoid. Both zeniths differ due to local gravitational
             * anomalies.
             *
             * Algorithm is from "GPS Satellite Surveying", A. Leick, page 184.
             */
        void cartToGeod();

        /** Converts from longitude, latitude and height on a standard
             * geoid of the Earth to cartesian coordinates in meters. The geoid
             * is characterized by two parameters: the semimajor axis and the
             * flattening.
             *
             * @note The astronomical zenith is defined as the perpendicular to
             * the real geoid. The geodetic zenith is the perpendicular to the
             * standard geoid. Both zeniths differ due to local gravitational
             * anomalies.
             *
             * Algorithm is from "GPS Satellite Surveying", A. Leick, page 184.
             */
        void geodToCart();

        /** The geoid is an elliposid which fits the shape of the Earth. It is
             * characterized by two parameters: the semimajor axis and the
             * flattening.
             *
             * @param i is the index which allows to identify the parameters for the
             * chosen elliposid. 1="IAU76", 2="GRS80", 3="MERIT83", 4="WGS84",
             * 5="IERS89".
             */
        void setEllipsoid(int i);

        /**
         * @brief distanceTo Return the distance in km from this location to the given longitude and latitude
         * @param longitude Target site longitude
         * @param latitude Target site latitude
         * @return distance in kilometers between this site and the target site.
         */
        double distanceTo(const dms &longitude, const dms &latitude);

        dms GSTtoLST(const dms &gst) const
        {
            return dms(gst.Degrees() + Longitude.Degrees());
        }
        dms LSTtoGST(const dms &lst) const
        {
            return dms(lst.Degrees() - Longitude.Degrees());
        }

        KStarsDateTime UTtoLT(const KStarsDateTime &ut) const;
        KStarsDateTime LTtoUT(const KStarsDateTime &lt) const;

        /** Computes the velocity in km/s of an observer on the surface of the Earth
             * referred to a system whose origin is the center of the Earth. The X and
             * Y axis are contained in the equator and the X axis is towards the nodes
             * line. The Z axis is along the poles.
             *
             * @param vtopo[] Topocentric velocity. The resultant velocity is available
             *        in this array.
             * @param gt Greenwich sideral time for which we want to compute the topocentric velocity.
             */
        void TopocentricVelocity(double vtopo[], const dms &gt);

        /** @return Local Mean Sidereal Time.
             * @param jd Julian date
             */
        double LMST(double jd);

        bool isReadOnly() const;
        void setReadOnly(bool value);

    private:
        CachingDms Longitude, Latitude;
        QString Name, Province, Country;
        TimeZoneRule *TZrule;
        double TimeZone, Elevation;
        double axis, flattening;
        long double PosCartX, PosCartY, PosCartZ;
        int indexEllipsoid;
        bool ReadOnly;
};
