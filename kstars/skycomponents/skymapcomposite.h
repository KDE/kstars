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
#include "skyobject.h"

class SkyMesh;
class SkyLabeler;
class SkyMap;

class QPolygonF;

class CultureList;
class ConstellationBoundaryLines;
class ConstellationLines;
class ConstellationNamesComponent;
class EquatorialCoordinateGrid;
class HorizontalCoordinateGrid;
class DeepSkyComponent;
class Ecliptic;
class Equator;
class ArtificialHorizonComponent;
class FlagComponent;
class HorizonComponent;
class MilkyWay;
class SolarSystemComposite;
class StarComponent;
class DeepStarComponent;
class TargetListComponent;
class TargetListComponent;
class SatellitesComponent;
class SupernovaeComponent;
class ConstellationArtComponent;

class DeepSkyObject;
class KSPlanetBase;
class KSPlanet;
class ConstellationsArt;
class SyncedCatalogComponent;

/** @class SkyMapComposite
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
    	*/
    explicit SkyMapComposite(SkyComposite *parent);

    ~SkyMapComposite();

    virtual void update( KSNumbers *num=0 );

    /**
    	*@short Delegate planet position updates to the SolarSystemComposite
    	*
    	*Planet positions change over time, so they need to be recomputed 
    	*periodically, but not on every call to update().  This function 
    	*will recompute the positions of all solar system bodies except the 
    	*Earth's Moon, Jupiter's Moons AND Saturn Moons (because these objects' positions 
    	*change on a much more rapid timescale).
    	*@p num Pointer to the KSNumbers object
    	*@sa update()
    	*@sa updateMoons()
    	*@sa SolarSystemComposite::updatePlanets()
    	*/
    virtual void updateSolarSystemBodies( KSNumbers *num );

    /**
    	*@short Delegate moon position updates to the SolarSystemComposite
    	*
    	*Planet positions change over time, so they need to be recomputed 
    	*periodically, but not on every call to update().  This function 
    	*will recompute the positions of the Earth's Moon and Jupiter's four
    	*Galilean moons.  These objects are done separately from the other 
    	*solar system bodies, because their positions change more rapidly,
    	*and so updateMoons() must be called more often than updatePlanets().
    	*@p num Pointer to the KSNumbers object
    	*@sa update()
    	*@sa updatePlanets()
    	*@sa SolarSystemComposite::updateMoons()
    	*/
//    virtual void updateMoons( KSNumbers *num );

    /**
    	*@short Delegate draw requests to all sub components
    	*@p psky Reference to the QPainter on which to paint
    	*/
    virtual void draw( SkyPainter *skyp );

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
      *@return the list of objects in the region defined by skypoints 
      *@param p1 first sky point (top-left vertex of rectangular region)
      *@param p2 second sky point (bottom-right vertex of rectangular region)
      */     
    QList<SkyObject*> findObjectsInArea( const SkyPoint& p1, const SkyPoint& p2 );

    void addCustomCatalog( const QString &filename, int index );
    void removeCustomCatalog( const QString &name );

    bool addNameLabel( SkyObject *o );
    bool removeNameLabel( SkyObject *o );

    void reloadDeepSky();
    void reloadAsteroids();
    void reloadComets();
    void reloadCLines();
    void reloadCNames();
    void reloadConstellationArt();
#ifndef KSTARS_LITE
    FlagComponent* flags();
#endif
    SatellitesComponent* satellites();
    SupernovaeComponent* supernovaeComponent();
    ArtificialHorizonComponent* artificialHorizon();

    /** Getters for SkyComponents **/
    inline HorizonComponent* horizon() { return m_Horizon; }

    inline ConstellationBoundaryLines* constellationBoundary() { return m_CBoundLines; }
    inline ConstellationLines* constellationLines() { return m_CLines; }

    inline Ecliptic* ecliptic() { return m_Ecliptic; }
    inline Equator* equator() { return m_Equator; }

    inline EquatorialCoordinateGrid* equatorialCoordGrid() { return m_EquatorialCoordinateGrid; }
    inline HorizontalCoordinateGrid* horizontalCoordGrid() { return m_HorizontalCoordinateGrid; }

    inline ConstellationArtComponent * constellationArt() { return m_ConstellationArt; }

    inline SolarSystemComposite* solarSystemComposite() { return m_SolarSystem; }

    inline ConstellationNamesComponent* constellationNamesComponent() { return m_CNames; }

    inline DeepSkyComponent* deepSkyComponent() { return m_DeepSky; }

    inline MilkyWay *milkyWay() { return m_MilkyWay; }

    inline SyncedCatalogComponent* internetResolvedComponent() { return m_internetResolvedComponent; }
    inline SyncedCatalogComponent* manualAdditionsComponent() { return m_manualAdditionsComponent; }

    //Accessors for StarComponent
    SkyObject* findStarByGenetiveName( const QString name );
    virtual void emitProgressText( const QString &message );
    QList<SkyObject*>& labelObjects() { return m_LabeledObjects; }

    const QList<DeepSkyObject*>& deepSkyObjects() const;
    const QList<SkyObject*>& constellationNames() const;
    const QList<SkyObject*>& stars() const;
    const QList<SkyObject*>& asteroids() const;
    const QList<SkyObject*>& comets() const;
    const QList<SkyObject*>& supernovae() const;
    QList<SkyObject*> planets();
//    QList<SkyObject*> moons();

    const QList<SkyObject*> *getSkyObjectsList(SkyObject::TYPE t);

    KSPlanet* earth();
    KSPlanetBase* planet( int n );
    QStringList getCultureNames();
    QString getCultureName( int index );
    QString currentCulture();
    void setCurrentCulture( QString culture );
    bool isLocalCNames();

    QList<SkyComponent*> customCatalogs();

    inline TargetListComponent *getStarHopRouteList() { return m_StarHopRouteList; }
signals:
    void progressText( const QString &message );

private:
    virtual QHash<int, QStringList>& getObjectNames();
    virtual QHash<int, QVector<QPair<QString, const SkyObject*>>>& getObjectLists();
    
    CultureList                 *m_Cultures;
    ConstellationBoundaryLines  *m_CBoundLines;
    ConstellationNamesComponent *m_CNames;
    ConstellationLines          *m_CLines;
    ConstellationArtComponent   *m_ConstellationArt;
    EquatorialCoordinateGrid    *m_EquatorialCoordinateGrid;
    HorizontalCoordinateGrid    *m_HorizontalCoordinateGrid;
    DeepSkyComponent            *m_DeepSky;
    Equator                     *m_Equator;
    ArtificialHorizonComponent           *m_ArtificialHorizon;
    Ecliptic                    *m_Ecliptic;
    HorizonComponent            *m_Horizon;
    MilkyWay                    *m_MilkyWay;
    SolarSystemComposite        *m_SolarSystem;
    SkyComposite                *m_CustomCatalogs;
    StarComponent               *m_Stars;
#ifndef KSTARS_LITE
    FlagComponent               *m_Flags;
#endif
    TargetListComponent         *m_ObservingList;
    TargetListComponent         *m_StarHopRouteList;
    SatellitesComponent         *m_Satellites;
    SupernovaeComponent         *m_Supernovae;
    SyncedCatalogComponent      *m_internetResolvedComponent;
    SyncedCatalogComponent      *m_manualAdditionsComponent;

    SkyMesh*                m_skyMesh;
    SkyLabeler*             m_skyLabeler;

    KSNumbers               m_reindexNum;

    QList<DeepStarComponent *> m_DeepStars;

    QList<SkyObject*>       m_LabeledObjects;
    QHash<int, QStringList> m_ObjectNames;
    QHash<int, QVector<QPair<QString, const SkyObject*>>> m_ObjectLists;
    QHash<QString, QString> m_ConstellationNames;
    QString m_internetResolvedCat; // Holds the name of the internet resolved catalog
    QString m_manualAdditionsCat;
};

#endif
