/***************************************************************************
                          skymapcomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYMAPCOMPOSITE_H
#define SKYMAPCOMPOSITE_H

#include <QList>

#include "skycomposite.h"
#include "ksnumbers.h"
#include "starcomponent.h"

class SkyMesh;
class SkyLabeler;
class SkyMap;

class QPolygonF;

class CultureList;
class ConstellationBoundary;
class ConstellationBoundaryLines;
class ConstellationLines;
class ConstellationNamesComponent;
class CoordinateGrid;
class DeepSkyComponent;
class Ecliptic;
class Equator;
class HorizonComponent;
class MilkyWay;
class MagellanicClouds;
class SolarSystemComposite;
class StarComponent;
class DeepStarComponent;
class SatelliteComposite;

class KStarsData;
class DeepSkyObject;
class KSPlanetBase;
class KSPlanet;

/**@class SkyMapComposite
*SkyMapComposite is the root object in the object hierarchy of the sky map.
*All requests to update, init, draw etc. will be done with this class.
*The requests will be delegated to it's children.
*The object hierarchy will created by adding new objects via addComponent().
*
*@author Thomas Kabelmann
*@version 0.1
*/
class SkyMapComposite : public QObject, public SkyComposite
{
    Q_OBJECT

public:
    /**
    	*Constructor
    	*@p parent pointer to the parent SkyComponent
    	*@p data pointer to the KStarsData object
    	*/
    SkyMapComposite(SkyComponent *parent, KStarsData *data);

    ~SkyMapComposite();

    virtual void update( KStarsData *data, KSNumbers *num=0 );

    /**
    	*@short Delegate planet position updates to the SolarSystemComposite
    	*
    	*Planet positions change over time, so they need to be recomputed 
    	*periodically, but not on every call to update().  This function 
    	*will recompute the positions of all solar system bodies except the 
    	*Earth's Moon and Jupiter's Moons (because these objects' positions 
    	*change on a much more rapid timescale).
    	*@p data Pointer to the KStarsData object
    	*@p num Pointer to the KSNumbers object
    	*@sa update()
    	*@sa updateMoons()
    	*@sa SolarSystemComposite::updatePlanets()
    	*/
    virtual void updatePlanets( KStarsData *data, KSNumbers *num );

    /**
    	*@short Delegate moon position updates to the SolarSystemComposite
    	*
    	*Planet positions change over time, so they need to be recomputed 
    	*periodically, but not on every call to update().  This function 
    	*will recompute the positions of the Earth's Moon and Jupiter's four
    	*Galilean moons.  These objects are done separately from the other 
    	*solar system bodies, because their positions change more rapidly,
    	*and so updateMoons() must be called more often than updatePlanets().
    	*@p data Pointer to the KStarsData object
    	*@p num Pointer to the KSNumbers object
    	*@sa update()
    	*@sa updatePlanets()
    	*@sa SolarSystemComposite::updateMoons()
    	*/
    virtual void updateMoons( KStarsData *data, KSNumbers *num );

    /**
    	*@short Delegate draw requests to all sub components
    	*@p psky Reference to the QPainter on which to paint
    	*/
    virtual void draw( QPainter& psky );

    /**
      *@return the object nearest a given point in the sky.
      *@param p The point to find an object near
      *@param maxrad The maximum search radius, in Degrees
			*@note the angular separation to the matched object is returned 
			*through the maxrad variable.
      */
    virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );

    /**
     *@return the star nearest a given point in the sky.
      *@param p The point to find a star near
      *@param maxrad The maximum search radius, in Degrees
			*@note the angular separation to the matched star is returned 
			*through the maxrad variable.
      */
    SkyObject* starNearest( SkyPoint *p, double &maxrad );

    /**
    	*@short Search the children of this SkyMapComposite for 
    	*a SkyObject whose name matches the argument.
    	*
    	*The objects' primary, secondary and long-form names will 
    	*all be checked for a match.
    	*@note Overloaded from SkyComposite.  In this version, we search 
    	*the most likely object classes first to be more efficient.
    	*@p name the name to be matched
    	*@return a pointer to the SkyObject whose name matches
    	*the argument, or a NULL pointer if no match was found.
    	*/
    virtual SkyObject* findByName( const QString &name );

    /**
    	*@short Add a Trail to the specified SkyObject
    	*Loop over all child SkyComponents; if the SkyObject
    	*in the argument is found, add a Trail to it.
    	*@p o Pointer to the SkyObject to which a Trail will be added
    	*@return true if the object was found and a Trail was added 
    	*/
    virtual bool addTrail( SkyObject *o );
    virtual bool hasTrail( SkyObject *o, bool &found );
    virtual bool removeTrail( SkyObject *o );
    virtual void clearTrailsExcept( SkyObject *o );

    void addCustomCatalog( const QString &filename, KStarsData *data, int index );
    void removeCustomCatalog( const QString &name );

    bool addDeepStarComponent( const QString &filename );
    void loadDeepStarCatalogs();

    bool addNameLabel( SkyObject *o );
    bool removeNameLabel( SkyObject *o );

    void reloadDeepSky( KStarsData *data );
    void reloadAsteroids( KStarsData *data );
    void reloadComets( KStarsData *data );
    void reloadCLines( KStarsData *data );
    void reloadCNames( KStarsData *data );

    //Accessors for StarComponent
    SkyObject* findStarByGenetiveName( const QString name );
    float faintStarMagnitude() const;
    void setStarColorMode( int newMode );
    int starColorMode() const;
    void setStarColorIntensity( int newIntensity );
    int starColorIntensity() const;
    inline StarComponent *getStarComponent() const { return m_Stars; }

    virtual void emitProgressText( const QString &message );
    virtual QHash<int, QStringList>& objectNames();
    virtual QStringList& objectNames( int k );
    QList<SkyObject*>& labelObjects() { return m_LabeledObjects; }

    QList<DeepSkyObject*>& deepSkyObjects();
    QList<SkyComponent*> solarSystem();
    QList<SkyObject*>& constellationNames();
    QList<SkyObject*>& stars();
    QList<SkyObject*>& asteroids();
    QList<SkyObject*>& comets();
    KSPlanet* earth();
    KSPlanetBase* planet( int n );
    QStringList getCultureNames();
    QString getCultureName( int index );
    QString currentCulture();
    void setCurrentCulture( QString culture );

    QList<SkyComponent*> customCatalogs();

signals:
    void progressText( const QString &message );

private:
    CultureList                 *m_Cultures;
    ConstellationBoundaryLines  *m_CBoundLines;
    ConstellationNamesComponent *m_CNames;
    ConstellationLines          *m_CLines;
    CoordinateGrid              *m_CoordinateGrid;
    DeepSkyComponent            *m_DeepSky;
    Equator                     *m_Equator;
    Ecliptic                    *m_Ecliptic;
    HorizonComponent            *m_Horizon;
    MilkyWay                    *m_MilkyWay;
    MagellanicClouds		*m_MagellanicClouds;
    SolarSystemComposite        *m_SolarSystem;
    SkyComposite                *m_CustomCatalogs;
    StarComponent               *m_Stars;
    SatelliteComposite          *m_Satellites;

    SkyMesh*                m_skyMesh;
    SkyLabeler*             m_skyLabeler;

    KSNumbers               m_reindexNum;

    QList<DeepStarComponent *> m_DeepStars;

    QList<SkyObject*>       m_LabeledObjects;
    QHash<int, QStringList> m_ObjectNames;
    QHash<QString, QString> m_ConstellationNames;
};

#endif
