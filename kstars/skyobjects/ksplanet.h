/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ksplanetbase.h"

#include <QHash>
#include <QString>
#include <QVector>

class KSNumbers;

/**
 * @class KSPlanet
 * A subclass of KSPlanetBase for seven of the major planets in the solar system
 * (Earth and Pluto have their own specialized classes derived from KSPlanetBase).
 * @note The Sun is subclassed from KSPlanet.
 *
 * KSPlanet contains internal classes to manage the computations of a planet's position.
 * The position is computed as a series of sinusoidal sums, similar to a Fourier
 * transform.  See "Astronomical Algorithms" by Jean Meeus or the file README.planetmath
 * for details.
 * @short Provides necessary information about objects in the solar system.
 *
 * @author Jason Harris
 * @version 1.0
 */
class KSPlanet : public KSPlanetBase
{
  public:
    /**
     * Constructor.
     * @param s Name of planet
     * @param image_file filename of the planet's image
     * @param c the color for the planet
     * @param pSize physical diameter of the planet, in km
     */
    explicit KSPlanet(const QString &s = "unnamed", const QString &image_file = QString(), const QColor &c = Qt::white,
                      double pSize = 0);

    /**
     * Simplified constructor
     * @param n identifier of the planet to be created
     * @see PLANET enum
     */
    explicit KSPlanet(int n);

    KSPlanet *clone() const override;
    SkyObject::UID getUID() const override;

    ~KSPlanet() override = default;

    /**
     * @short return the untranslated name
     * This is a dirty way to solve a lot of localization-related trouble for the KDE 4.2 release
     * TODO: Change the whole architecture for names later
     */
    QString untranslatedName() const;

    /** @short Preload the data used by findPosition. */
    bool loadData() override;

    /**
     * Calculate the ecliptic longitude and latitude of the planet for
     * the given date (expressed in Julian Millenia since J2000).  A reference
     * to the ecliptic coordinates is returned as the second object.
     * @param jm Julian Millenia (=jd/1000)
     * @param ret The ecliptic coordinates are returned by reference through this argument.
     */
    virtual void calcEcliptic(double jm, EclipticPosition &ret) const;

  protected:
    /**
     * Calculate the geocentric RA, Dec coordinates of the Planet.
     * @note reimplemented from KSPlanetBase
     * @param num pointer to object with time-dependent values for the desired date
     * @param Earth pointer to the planet Earth (needed to calculate geocentric coords)
     * @return true if position was successfully calculated.
     */
    bool findGeocentricPosition(const KSNumbers *num, const KSPlanetBase *Earth = nullptr) override;

    /**
     * @class OrbitData
     * This class contains doubles A,B,C which represent a single term in a planet's
     * positional expansion sums (each sum-term is A*COS(B+C*T)).
     *
     * @author Mark Hollomon
     * @version 1.0
     */
    class OrbitData
    {
      public:
        /** Default constructor */
        OrbitData() : A(0.), B(0.), C(0.) {}
        /**
         * Constructor
         * @param a the A value
         * @param b the B value
         * @param c the C value
         */
        OrbitData(double a, double b, double c) : A(a), B(b), C(c) {}

        double A, B, C;
    };

    typedef QVector<OrbitData> OBArray[6];

    /**
     * OrbitDataColl contains three groups of six QVectors.  Each QVector is a
     * list of OrbitData objects, representing a single sum used in computing
     * the planet's position.  A set of six of these vectors comprises the large
     * "meta-sum" which yields the planet's Longitude, Latitude, or Distance value.
     *
     * @author Mark Hollomon
     * @version 1.0
     */
    class OrbitDataColl
    {
      public:
        /** Constructor */
        OrbitDataColl() = default;

        OBArray Lon;
        OBArray Lat;
        OBArray Dst;
    };

    /**
     * OrbitDataManager places the OrbitDataColl objects for all planets in a QDict
     * indexed by the planets' names. It also loads the positional data of each planet from disk.
     *
     * @author Mark Hollomon
     * @version 1.0
     */
    class OrbitDataManager
    {
      public:
        /** Constructor */
        OrbitDataManager();

        /**
         * Load orbital data for a planet from disk.
       	 * The data is stored on disk in a series of files named
         * "name.[LBR][0...5].vsop", where "L"=Longitude data, "B"=Latitude data,
         * and R=Radius data.
         * @param n the name of the planet whose data is to be loaded from disk.
         * @param odc reference to the OrbitDataColl containing the planet's orbital data.
         * @return true if data successfully loaded
         */
        bool loadData(OrbitDataColl &odc, const QString &n);

      private:
        /**
         * Read a single orbital data file from disk into an OrbitData vector.
         * The data files are named "name.[LBR][0...5].vsop", where
         * "L"=Longitude data, "B"=Latitude data, and R=Radius data.
         * @param fname the filename to be read.
         * @param vector pointer to the OrbitData vector to be filled with these data.
         */
        bool readOrbitData(const QString &fname, QVector<KSPlanet::OrbitData> *vector);

        QHash<QString, OrbitDataColl> hash;
    };

  private:
    void findMagnitude(const KSNumbers *) override;

  protected:
    bool data_loaded { false };
    static OrbitDataManager odm;
};
