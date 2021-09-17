/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ksplanetbase.h"

/**
 * @class KSComet
 * @short A subclass of KSPlanetBase that implements comets.
 *
 * The orbital elements are stored as private member variables, and
 * it provides methods to compute the ecliptic coordinates for any
 * time from the orbital elements.
 *
 * All elements are in the heliocentric ecliptic J2000 reference frame.
 *
 * Check here for full description: https://ssd.jpl.nasa.gov/?sb_elem#legend
 *
 * The orbital elements are:
 * @li JD    Epoch of element values
 * @li q     perihelion distance (AU)
 * @li e     eccentricity of orbit
 * @li i     inclination angle (with respect to J2000.0 ecliptic plane)
 * @li w     argument of perihelion (w.r.t. J2000.0 ecliptic plane)
 * @li N     longitude of ascending node (J2000.0 ecliptic)
 * @li Tp    time of perihelion passage (YYYYMMDD.DDD)
 * @li M1    comet total magnitude parameter
 * @li M2    comet nuclear magnitude parameter
 * @li K1    total magnitude slope parameter
 * @li K2    nuclear magnitude slope parameter
 *
 * @author Jason Harris
 * @version 1.1
 */

class KSNumbers;
class dms;

class KSComet : public KSPlanetBase
{
  public:
    /**
     * Constructor.
     * @param s the name of the comet
     * @param image_file the filename for an image of the comet
     * @param q the perihelion distance of the comet's orbit (AU)
     * @param e the eccentricity of the comet's orbit
     * @param i the inclination angle of the comet's orbit
     * @param w the argument of the orbit's perihelion
     * @param N the longitude of the orbit's ascending node
     * @param Tp The date of the most proximate perihelion passage (YYYYMMDD.DDD)
     * @param M1 the comet total magnitude parameter
     * @param M2 the comet nuclear magnitude parameter
     * @param K1 the comet total magnitude slope parameter
     * @param K2 the comet nuclear magnitude slope parameter
     */
    KSComet(const QString &s, const QString &image_file, double q, double e, dms i, dms w, dms N,
            double Tp, float M1, float M2, float K1, float K2);

    KSComet *clone() const override;
    SkyObject::UID getUID() const override;

    /** Destructor (empty)*/
    ~KSComet() override = default;

    /**
     * Unused virtual function inherited from KSPlanetBase thus it's simply empty here.
     */
    bool loadData() override;

    /**
     * @short Returns the Julian Day of Perihelion passage
     * @return Julian Day of Perihelion Passage
     */
    inline long double getPerihelionJD() { return JDp; }

    /**
     * @short Returns Perihelion distance
     * @return Perihelion distance
     */
    inline double getPerihelion() { return q; }

    /** @return the comet total magnitude parameter */
    inline float getTotalMagnitudeParameter() { return M1; }

    /** @return the comet nuclear magnitude parameter */
    inline float getNuclearMagnitudeParameter() { return M2; }

    /** @return the total magnitude slope parameter */
    inline float getTotalSlopeParameter() { return K1; }

    /** @return the nuclear magnitude slope parameter */
    inline float getNuclearSlopeParameter() { return K2; }

    /** @short Sets the comet's tail length in km */
    void setTailSize(double tailsize) { TailSize = tailsize; }

    /** @return the estimated tail length in km */
    inline float getTailSize() { return TailSize; }

    /** @short Sets the comet's apparent tail length in degrees */
    void setComaAngSize(double comaAngSize) { ComaAngSize = comaAngSize; }

    /** @return the estimated angular size of the tail as a dms */
    inline dms getComaAngSize() { return dms(ComaAngSize); }

    /** @return the estimated diameter of the nucleus in km */
    inline float getNuclearSize() { return NuclearSize; }

    /** @short Sets the comet's earth minimum orbit intersection distance */
    void setEarthMOID(double earth_moid);

    /** @return the comet's earth minimum orbit intersection distance in km */
    inline double getEarthMOID() { return EarthMOID; }

    /** @short Sets the comet's orbit solution ID */
    void setOrbitID(QString orbit_id);

    /** @return the comet's orbit solution ID */
    inline QString getOrbitID() { return OrbitID; }

    /** @short Sets the comet's orbit class */
    void setOrbitClass(QString orbit_class);

    /** @return the comet's orbit class */
    inline QString getOrbitClass() { return OrbitClass; }

    /** @short Sets if the comet is a near earth object */
    void setNEO(bool neo);

    /** @return true if the comet is a near earth object */
    inline bool isNEO() { return NEO; }

    /** @short Sets the comet's albedo */
    void setAlbedo(float albedo);

    /** @return the comet's albedo */
    inline float getAlbedo() { return Albedo; }

    /** @short Sets the comet's diameter */
    void setDiameter(float diam);

    /** @return the comet's diameter */
    inline float getDiameter() { return Diameter; }

    /** @short Sets the comet's dimensions */
    void setDimensions(QString dim);

    /** @return the comet's dimensions */
    inline QString getDimensions() { return Dimensions; }

    /** @short Sets the comet's rotation period */
    void setRotationPeriod(float rot_per);

    /** @return the comet's rotation period */
    inline float getRotationPeriod() { return RotationPeriod; }

    /** @short Sets the comet's period */
    void setPeriod(float per);

    /** @return the comet's period */
    inline float getPeriod() { return Period; }

  protected:
    /**
     * Calculate the geocentric RA, Dec coordinates of the Comet.
     * @note reimplemented from KSPlanetBase
     * @param num time-dependent values for the desired date
     * @param Earth planet Earth (needed to calculate geocentric coords)
     * @return true if position was successfully calculated.
     */
    bool findGeocentricPosition(const KSNumbers *num, const KSPlanetBase *Earth = nullptr) override;

    /**
     * @short Estimate physical parameters of the comet such as coma size, tail length and size of the nucleus
     * @note invoked from findGeocentricPosition in order
     */
    void findPhysicalParameters();

  private:
    void findMagnitude(const KSNumbers *) override;

    long double JDp { 0 };
    double q { 0 };
    double e { 0 };
    double a { 0 };
    double P { 0 };
    double EarthMOID { 0 };
    double TailSize { 0 };
    double ComaAngSize { 0 };
    double ComaSize { 0 };
    double NuclearSize { 0 };
    float M1 { 0 };
    float M2 { 0 };
    float K1 { 0 };
    float K2 { 0 };
    float Albedo { 0 };
    float Diameter { 0 };
    float RotationPeriod { 0 };
    float Period { 0 };
    dms i, w, N;
    QString OrbitID, OrbitClass, Dimensions;
    bool NEO { false };
    /// Part of UID
    qint64 uidPart { 0 };
};
