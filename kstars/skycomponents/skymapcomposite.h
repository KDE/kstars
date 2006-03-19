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

#include "deepskyobject.h"
#include "ksplanet.h"

// #include "constellationboundarycomponent.h"
// #include "constellationlinescomposite.h"
#include "constellationnamescomponent.h"
// #include "coordinategridcomposite.h"
// #include "customcatalogcomponent.h"
#include "deepskycomponent.h"
// #include "equatorcomponent.h"
// #include "eclipticcomponent.h"
// #include "horizoncomponent.h"
// #include "jupitermoonscomponent.h"
// #include "milkywaycomposite.h"
#include "solarsystemcomposite.h"
#include "starcomponent.h"
#include "telescopecomponent.h"


class ConstellationBoundaryComponent;
class ConstellationLinesComposite;
class ConstellationNamesComponent;
class CoordinateGridComposite;
class CustomCatalogComponent;
//class DeepSkyComponent;
class EclipticComponent;
class EquatorComponent;
class HorizonComponent;
class MilkyWayComposite;
//class SolarSystemComposite;
//class StarComponent;
//class TelescopeComponent;


class KStarsData;

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
			*@p ks Pointer to the KStars object
			*@p psky Reference to the QPainter on which to paint
			*@p scale the scaling factor for drawing (1.0 for screen draws)
			*/
		virtual void draw(KStars *ks, QPainter& psky, double scale = 1.0);

		virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );

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
		virtual void addTelescopeMarker( SkyObject *o);
		virtual void removeTelescopeMarker( SkyObject *o);

		void addCustomCatalog( const QString &filename, bool (*visibleMethod)() );
		void removeCustomCatalog( const QString &name );

		bool addNameLabel( SkyObject *o );
		bool removeNameLabel( SkyObject *o );

		void reloadDeepSky( KStarsData *data );
		void reloadAsteroids( KStarsData *data );
		void reloadComets( KStarsData *data );

		//Accessors for StarComponent
		SkyObject* findStarByGenetiveName( const QString &name );
		void setFaintStarMagnitude( float newMag );
		float faintStarMagnitude() const { return m_Stars->faintMagnitude(); }
		void setStarColorMode( int newMode );
		int starColorMode() const { return m_Stars->starColorMode(); }
		void setStarColorIntensity( int newIntensity );
		int starColorIntensity() const { return m_Stars->starColorIntensity(); }

		QString constellation( SkyPoint *p );

		virtual void emitProgressText( const QString &message );
		virtual QStringList& objectNames() { return m_ObjectNames; }
		QList<SkyObject*>& labelObjects() { return m_LabeledObjects; }

		QList<DeepSkyObject*>& deepSkyObjects() { return m_DeepSky->objectList(); }
		QList<SkyComponent*> solarSystem() { return m_SolarSystem->components(); }
		QList<SkyObject*>& constellationNames() { return m_CNames->objectList(); }
		QList<SkyObject*>& stars() { return m_Stars->objectList(); }
		QList<SkyObject*>& asteroids() { return m_SolarSystem->asteroids(); }
		QList<SkyObject*>& comets() { return m_SolarSystem->comets(); }
		QList<SkyObject*>& telescopes() { return m_Telescopes->objectList(); }
		KSPlanet* earth() { return m_SolarSystem->earth(); }

		QList<SkyComponent*> customCatalogs() { 
			return m_CustomCatalogs->components(); 
		}

	signals:
		void progressText( const QString &message );

	private:
		ConstellationBoundaryComponent *m_CBounds;
		ConstellationNamesComponent *m_CNames;
		ConstellationLinesComposite *m_CLines;
		CoordinateGridComposite *m_CoordinateGrid;
		DeepSkyComponent *m_DeepSky;
		EquatorComponent *m_Equator;
		EclipticComponent *m_Ecliptic;
		HorizonComponent *m_Horizon;
		MilkyWayComposite *m_MilkyWay;
		SolarSystemComposite *m_SolarSystem;
		SkyComposite *m_CustomCatalogs;
		StarComponent *m_Stars;
		TelescopeComponent *m_Telescopes;
		QStringList m_ObjectNames;
		QList<SkyObject*> m_LabeledObjects;
};

#endif
