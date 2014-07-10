
/***************************************************************************
                          ksplanetbase.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jan 29 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSPLANETBASE_H_
#define KSPLANETBASE_H_


#include <QList>
#include <QImage>
#include <QColor>

#include <QDebug>

#include "trailobject.h"

class KSNumbers;
class KSPopupMenu;

/**
 *@class EclipticPosition
 *@short The ecliptic position of a planet (Longitude, Latitude, and distance from Sun).
 *@author Mark Hollomon
 *@version 1.0
 */
class EclipticPosition {
public:
    dms longitude;
    dms latitude;
    double radius;

    /**Constructor. */
    explicit EclipticPosition(dms plong = dms(), dms plat = dms(), double prad = 0.0) :
        longitude(plong), latitude(plat), radius(prad)
    {}
};

/**
  *@class KSPlanetBase
  *A subclass of TrailObject that provides additional information
  *needed for most solar system objects. This is a base class for 
  *KSSun, KSMoon, KSPlanet, KSAsteroid, and KSComet.  Those classes 
  *cover all solar system objects except planetary moons, which are 
  *derived directly from TrailObject
  *@short Provides necessary information about objects in the solar system.
  *@author Mark Hollomon
  *@version 1.0
  */
class KSPlanetBase : public TrailObject {
public:
    /**
      *Constructor.  Calls SkyObject constructor with type=2 (planet),
      *coordinates=0.0, mag=0.0, primary name s, and all other QStrings empty.
      *@param s Name of planet
      *@param image_file filename of the planet's image
      *@param c color of the symbol to use for this planet
      *@param pSize the planet's physical size, in km
      */
    explicit KSPlanetBase( const QString &s = i18n("unnamed"),
                           const QString &image_file=QString(),
                           const QColor &c=Qt::white, double pSize=0 );

    /** Destructor (empty) */
    virtual ~KSPlanetBase() {}

    void init(const QString &s, const QString &image_file, const QColor &c, double pSize );

    enum Planets { MERCURY=0, VENUS=1, MARS=2, JUPITER=3, SATURN=4, URANUS=5, NEPTUNE=6, PLUTO=7, SUN=8, MOON=9, UNKNOWN_PLANET };

    static KSPlanetBase* createPlanet( int n );

    static QVector<QColor> planetColor;

    virtual bool loadData() = 0;

    /** @return pointer to Ecliptic Longitude coordinate */
    const dms& ecLong() const { return ep.longitude; }

    /** @return pointer to Ecliptic Latitude coordinate */
    const dms& ecLat() const { return ep.latitude; }

    /**@short Set Ecliptic Geocentric Longitude according to argument.
     * @param elong Ecliptic Longitude
     */
    void setEcLong( dms elong ) { ep.longitude = elong; }

    /**@short Set Ecliptic Geocentric Latitude according to argument.
     * @param elat Ecliptic Latitude
     */
    void setEcLat( dms elat ) { ep.latitude = elat; }

    /**@return pointer to Ecliptic Heliocentric Longitude coordinate */
    const dms& helEcLong() const { return helEcPos.longitude; }

    /**@return pointer to Ecliptic Heliocentric Latitude coordinate */
    const dms& helEcLat() const { return helEcPos.latitude; }

    /**@short Convert Ecliptic logitude/latitude to Right Ascension/Declination.
     * @param Obliquity current Obliquity of the Ecliptic (angle from Equator)
     */
    void EclipticToEquatorial( const dms *Obliquity );

    /**@short Convert Right Ascension/Declination to Ecliptic logitude/latitude.
     * @param Obliquity current Obliquity of the Ecliptic (angle from Equator)
     */
    void EquatorialToEcliptic( const dms *Obliquity );

    /** @return pointer to this planet's texture */
    const QImage& image() const { return m_image; }

    /**@return distance from Sun, in Astronomical Units (1 AU is Earth-Sun distance) */
    double rsun() const { return ep.radius; }

    /**@short Set the solar distance in AU.
     * @param r the new solar distance in AU
     */
    void setRsun( double r ) { ep.radius = r; }

    /**@return distance from Earth, in Astronomical Units (1 AU is Earth-Sun distance) */
    double rearth() const { return Rearth; }

    /**@short Set the distance from Earth, in AU.
     * @param r the new earth-distance in AU
     */
    void setRearth( double r ) { Rearth = r; }

    /**@short compute and set the distance from Earth, in AU.
     * @param Earth pointer to the Earth from which to calculate the distance.
     */
    void setRearth( const KSPlanetBase *Earth );

    /**Update position of the planet (reimplemented from SkyPoint)
     * @param num current KSNumbers object
     * @param includePlanets this function does nothing if includePlanets=false
     * @param lat pointer to the geographic latitude; if NULL, we skip localizeCoords()
     * @param LST pointer to the local sidereal time; if NULL, we skip localizeCoords()
     */
    virtual void updateCoords( KSNumbers *num, bool includePlanets=true, const dms *lat=0, const dms *LST=0, bool forceRecompute = false );

    /**@short Find position, including correction for Figure-of-the-Earth.
     * @param num KSNumbers pointer for the target date/time
     * @param lat pointer to the geographic latitude; if NULL, we skip localizeCoords()
     * @param LST pointer to the local sidereal time; if NULL, we skip localizeCoords()
     * @param Earth pointer to the Earth (not used for the Moon)
     */
    void findPosition( const KSNumbers *num, const dms *lat=0, const dms *LST=0, const KSPlanetBase *Earth = 0 );

    /** @return the Planet's position angle. */
    virtual double pa() const { return PositionAngle; }

    /**@short Set the Planet's position angle.
     * @param p the new position angle
     */
    void setPA( double p ) { PositionAngle = p; }

    /** @return the Planet's angular size, in arcminutes */
    double angSize() const { return AngularSize; }

    /**@short set the planet's angular size, in km.
     * @param size the planet's size, in km
     */
    void setAngularSize( double size ) { AngularSize = size; }

    /** @return the Planet's physical size, in km */
    double physicalSize() const { return PhysicalSize; }

    /**@short set the planet's physical size, in km.
     * @param size the planet's size, in km
     */
    void setPhysicalSize( double size ) { PhysicalSize = size; }

    /** @return the phase angle of this planet */
    inline dms phase() { return dms( Phase ); }

    /** @return the color for the planet symbol */
    QColor& color() { return m_Color; }

    /** @short Set the color for the planet symbol */
    void setColor( const QColor &c ) { m_Color = c; }

    /** @return true if the KSPlanet is one of the eight major planets */
    bool isMajorPlanet() const;

    /** @return the pixel distance for offseting the object's name label */
    virtual double labelOffset() const;

protected:
    /** Big object. Planet, Moon, Sun. */
    static const UID UID_SOL_BIGOBJ;
    /** Asteroids */
    static const UID UID_SOL_ASTEROID;
    /** Comets */
    static const UID UID_SOL_COMET;

    /** Compute high 32-bits of UID. */
    inline UID solarsysUID(UID type) const { return (SkyObject::UID_SOLARSYS << 60) | (type << 56); }
   
    /**@short find the object's current geocentric equatorial coordinates (RA and Dec)
     * This function is pure virtual; it must be overloaded by subclasses.
     * This function is private; it is called by the public function findPosition()
     * which also includes the figure-of-the-earth correction, localizeCoords().
     * @param num pointer to current KSNumbers object
     * @param Earth pointer to planet Earth (needed to calculate geocentric coords)
     * @return true if position was successfully calculated.
     */
    virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL ) = 0;

    /** 
     * @short Computes the visual magnitude for the major planets.
     * @param num pointer to a ksnumbers object. Needed for the saturn rings contribution to 
     *            saturn's magnitude.
     */
    virtual void findMagnitude(const KSNumbers *num) = 0;

    /**Determine the position angle of the planet for a given date
     * (used internally by findPosition() )
     */
    void findPA( const KSNumbers *num );

    /** Determine the phase of the planet. */
    virtual void findPhase();

    // Geocentric ecliptic position, but distance to the Sun
    EclipticPosition ep;

    // Heliocentric ecliptic position referred to the equinox of the epoch
    // as obtained from VSOP.
    EclipticPosition helEcPos;
    double  Rearth;
    double Phase;
    QImage m_image;

private:
    /**
     * @short correct the position for the fact that the location is not at the center of the Earth,
     * but a position on its surface.  This causes a small parallactic shift in a solar system
     * body's apparent position.  The effect is most significant for the Moon.
     * This function is private, and should only be called from the public findPosition() function.
     * @param num pointer to a ksnumbers object for the target date/time
     * @param lat pointer to the geographic latitude of the location.
     * @param LST pointer to the local sidereal time.
     */
    void localizeCoords( const KSNumbers *num, const dms *lat, const dms *LST );

    double PositionAngle, AngularSize, PhysicalSize;
    QColor m_Color;
};

#endif
