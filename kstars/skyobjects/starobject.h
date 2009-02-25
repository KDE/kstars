/***************************************************************************
                          starobject.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Sep 18 2001
    copyright            : (C) 2001 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef STAROBJECT_H_
#define STAROBJECT_H_

#include <qpoint.h>
#include <QMap>
#include <QPixmap>

#include "skyobject.h"
#include "stardata.h"
#include "deepstardata.h"

class QPainter;
class QString;
class KSPopupMenu;
class KStarsData;

/**@class StarObject
        *This is a subclass of SkyObject.  It adds the Spectral type, and flags
        *for variability and multiplicity.
        *For stars, the primary name (n) is the latin name (e.g., "Betelgeuse").  The
        *secondary name (n2) is the genetive name (e.g., "alpha Orionis").
        *@short subclass of SkyObject specialized for stars.
        *@author Thomas Kabelmann
        *@version 1.0
        */

class StarObject : public SkyObject {
public:

    /* @short returns the reindex interval (in centuries!) for the given
     * magnitude of proper motion (in milliarcsec/year).  ASSUMING a 
     * 25 arc-minute margin for proper motion.
     */
    static double reindexInterval( double pm );
    /**
        *Copy constructor
        */
    StarObject(StarObject & o);

    /**
        *Constructor.  Sets sky coordinates, magnitude, latin name, genetive name, and
        *spectral type.
        *@param r Right Ascension
        *@param d Declination
        *@param m magnitude
        *@param n common name
        *@param n2 genetive name
        *@param sptype Spectral Type
        *@param pmra Proper motion in RA direction [mas/yr]
        *@param pmdec Proper motion in Dec direction [mas/yr]
        *@param par Parallax angle [mas]
        *@param mult Multiplicity flag (false=dingle star; true=multiple star)
        *@param var Variability flag (true if star is a known periodic variable)
        *@param hd Henry Draper Number
        */
    explicit StarObject( dms r=dms(0.0), dms d=dms(0.0), float m=0.0, const QString &n=QString(),
                         const QString &n2=QString(), const QString &sptype="--", double pmra=0.0, double pmdec=0.0,
                         double par=0.0, bool mult=false, bool var=false, int hd=0 );
    /**
        *Constructor.  Sets sky coordinates, magnitude, latin name, genetive name, and
        *spectral type.  Differs from above function only in data type of RA and Dec.
        *@param r Right Ascension
        *@param d Declination
        *@param m magnitude
        *@param n common name
        *@param n2 genetive name
        *@param sptype Spectral Type
        *@param pmra Proper motion in RA direction [mas/yr]
        *@param pmdec Proper motion in Dec direction [mas/yr]
        *@param par Parallax angle [mas]
        *@param mult Multiplicity flag (false=dingle star; true=multiple star)
        *@param var Variability flag (true if star is a known periodic variable)
        *@param hd Henry Draper Number
        */
    StarObject( double r, double d, float m=0.0, const QString &n=QString(),
                const QString &n2=QString(), const QString &sptype="--", double pmra=0.0, double pmdec=0.0,
                double par=0.0, bool mult=false, bool var=false, int hd=0 );

    /**
     * Destructor. (Empty)
     */
    ~StarObject() { }

    /**
     *@short  Initializes a StarObject to given data
     *
     * This is almost like the StarObject constructor itself, but it avoids
     * setting up name, gname etc for unnamed stars. If called instead of the
     * constructor, this method will be much faster for unnamed stars
     *
     *@param  stardata  Pointer to starData object containing required data (except name and gname)
     *@return Nothing
     */

    void init( const starData *stardata );
    
    /**
     *@short  Initializes a StarObject to given data
     *
     *@param  stardata  Pointer to deepStarData object containing the available data
     *@return Nothing
     */

    void init( const deepStarData *stardata );

    /**
     *@short  Sets the name, genetive name, and long name
     *
     *@param  name  Common name
     *@param  name2 Genetive name
     */

    void setNames( QString name, QString name2 );

    /**
     *@return true if the star has a name ("star" doesn't count)
     */
    inline bool hasName() const { return ( !Name.isEmpty() && Name!=starString ); }

    /**
        *If star is unnamed return "star" otherwise return the name
        */
    inline virtual QString name( void ) const { return hasName() ? Name : starString;}

    /**
        *If star is unnamed return "star" otherwise return the longname
        */
    inline virtual QString longname( void ) const { return hasLongName() ? LongName : starString; }
    /**
        *@return QColor corresponding to the star's Spectral Type
        */
    QColor color( void ) const;

    /**
        *Returns entire spectral type string
        *@return Spectral Type string
        */
    QString sptype( void ) const;

    /**
        *Returns the genetive name of the star.
        *@return genetive name of the star
        */
    QString gname( bool useGreekChars=true ) const;

    /**
        *Returns the greek letter portion of the star's genetive name.
        *Returns empty string if star has no genetive name defined.
        *@return greek letter portion of genetive name
        */
    QString greekLetter( bool useGreekChars=true ) const;

    /**@return the genitive form of the star's constellation.
        */
    QString constell( void ) const;

    /**Determine the current coordinates (RA, Dec) from the catalog
        *coordinates (RA0, Dec0), accounting for both precession and nutation.
        *@param num pointer to KSNumbers object containing current values of
        *time-dependent variables.
        *@param includePlanets does nothing in this implementation (see KSPlanetBase::updateCoords()).
        *@param lat does nothing in this implementation (see KSPlanetBase::updateCoords()).
        *@param LST does nothing in this implementation (see KSPlanetBase::updateCoords()).
        */
    virtual void updateCoords( KSNumbers *num, bool includePlanets=true, const dms *lat=0, const dms *LST=0 );

    /* @short fills ra and dec with the coordinates of the star with the proper
     * motion correction but without precesion and its friends.  It is used
     * in StarComponent to re-index all the stars.
     *
     * NOTE: ra and dec both in degrees.
     */
    void getIndexCoords( KSNumbers *num, double *ra, double *dec );

    /* @short added for JIT updates from both StarComponent and ConstellationLines
     */
    void JITupdate( KStarsData* data );

    /* @short returns the magnitude of the proper motion correction in milliarcsec/year
     */
    double pmMagnitude();

    /**@short Set the Ra and Dec components of the star's proper motion, in milliarcsec/year.
        *Note that the RA component is multiplied by cos(dec).
        *@param pmra the new RA propoer motion
        *@param pmdec the new Dec proper motion
        */
    inline void setProperMotion( double pmra, double pmdec ) { PM_RA = pmra; PM_Dec = pmdec; }

    /**@return the RA component of the star's proper motion, in mas/yr (multiplied by cos(dec))
        */
    inline double pmRA() const { return PM_RA; }

    /**@return the Dec component of the star's proper motion, in mas/yr
        */
    inline double pmDec() const { return PM_Dec; }

    /**@short set the star's parallax angle, in milliarcsec
        */
    inline void setParallax( double plx ) { Parallax = plx; }

    /**@return the star's parallax angle, in milliarcsec
        */
    inline double parallax() const { return Parallax; }

    /**@return the star's distance from the Sun in parsecs, as computed from the parallax.
        */
    inline double distance() const { return 1000./parallax(); }

    /**@short set the star's multiplicity flag (i.e., is it a binary or multiple star?)
        *@param m true if binary/multiple star system
        */
    inline void setMultiple( bool m ) { Multiplicity = m; }

    /**@return whether the star is a binary or multiple starobject
        */
    inline bool isMultiple() const { return Multiplicity; }

    /**@return the star's HD index
        */
    inline int getHDIndex() { return HD; }

    /**@short set the star's variability flag
        *@param v true if star is variable
        */
    inline void setVariable( bool v ) { Variability = v; }

    /**@return whether the star is a binary or multiple starobject
        */
    inline bool isVariable() const { return Variability; }

    //Not using VRange, VPeriod currently (to save memory)
    ///**@short set the range in brightness covered by the star's variability
    //  *@param r the range of brightness, in magnitudes
    //  */
    //  void setVRange( double r ) { VRange = r; }
    //
    ///**@return the range in brightness covered by the star's variability, in magnitudes
    //  */
    //  double vrange() const { return VRange; }
    //
    ///**@short set the period of the star's brightness variation, in days.
    //  */
    //  void setVPeriod( double p ) { VPeriod = p; }
    //
    ///**@return the period of the star's brightness variation, in days.
    //  */
    //  double vperiod() const { return VPeriod; }

    void draw( QPainter &psky, float x, float y, float size,
               bool useRealColors, int scIntensity, bool drawMultiple=true );

    /* @short returns the name, the magnitude or both.
     */
    QString nameLabel( bool drawName, bool drawMag ) const;

    /* @short does the same as above except when only the magnitude is selected
     * in which case it returns "$mag, name".  This prevents label overlap.
     */
    QString customLabel( bool drawName, bool drawMag );

    virtual QString labelString() const;

    /**
     *@return the pixel distance for offseting the star's name label
     *This takes the zoom level and the star's brightness into account.
     */
    virtual double labelOffset() const;

    /**Show star object popup menu.  Overloaded from virtual
        *SkyObject::showPopupMenu()
        *@param pmenu pointer to the KSPopupMenu object
        *@param pos QPojnt holding the x,y coordinates for the menu
        */
    virtual void showPopupMenu( KSPopupMenu *pmenu, const QPoint &pos );

    /**
        *Reet the colors which will be used to make realistic star colors.
        *Each star has a spectral type, a string whose first letter indicates 
        *its color (RGB triplets shown below):
        *@li O: (   0,   0, 255 )
        *@li B: (   0, 200, 255 )
        *@li A: (   0, 255, 255 )
        *@li F: ( 200, 255, 100 )
        *@li G: ( 255, 255,   0 )
        *@li K: ( 255, 100,   0 )
        *@li M: ( 255,   0,   0 )
        *
        *If the user has enabled antialiased drawing, then these are the RGB 
        *codes of the colors that will be assigned to each spectral type.
        *However, if antialiasing is disabled, it is more complicated, because 
        *the colored "rim" of the star image cannot be less than 1 pixel wide,
        *so the above RGB codes will produce very saturated star colors.
        *To fix this, we mix the above colors with white to desaturate the 
        *colors, effectively simulating an antialiased colored band with width 
        *less than 1 pixel.
        *
        *@note This function only needs to be called when the user modifies the 
        *star color saturation level, or when the user toggles 
        *Options::useAntialias().
        *
        *@param desaturateColors if true, then we need to desaturate the star colors.
        *@param saturation The saturation level for star colors (this will 
        *almost always be passed as data()->colorScheme()->starColorIntensity())
        */
    static void updateColors( bool desaturateColors, int saturation );

    static void initImages();

    quint64 updateID;
    quint64 updateNumID;

protected:
    static QMap<QString, QColor> ColorMap;
    static QHash<QString, QPixmap> StarImage;

    // DEBUG EDIT. For testing proper motion, uncomment this, and related blocks
    // See starobject.cpp for further info.
    //    static QVector<SkyPoint *> Trail;
    //    bool testStar;
    // END DEBUG

private:
    char SpType[2];

    double PM_RA, PM_Dec, Parallax;  //, VRange, VPeriod;
    bool Multiplicity, Variability;
    int HD;
};

#endif
