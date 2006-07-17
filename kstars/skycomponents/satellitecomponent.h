/***************************************************************************
                          satellitecomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 14 July 2006
    copyright            : (C) 2006 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SATELLITECOMPONENT_H
#define SATELLITECOMPONENT_H

#include <QStringList>

#include "skycomponent.h"
#include "satellitetrack.h"

class SatelliteComponent : public SkyComponent {
	public:
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComponent object
		*/
		SatelliteComponent(SkyComponent*, bool (*visibleMethod)());
	/**
		*@short Destructor.  Delete list members
		*/
		~SatelliteComponent();
		
	/**
		*@short Initialize the component - load data from disk etc.
		*@p data Pointer to the KStarsData object
		*/
		virtual void init(KStarsData *data);

	/**
		*@short Update the sky positions of this component.
		*
		*This function usually just updates the Horizontal (Azimuth/Altitude)
		*coordinates of the objects in this component.  If the KSNumbers* 
		*argument is not NULL, this function also recomputes precession and
		*nutation for the date in KSNumbers.
		*@p data Pointer to the KStarsData object
		*@p num Pointer to the KSNumbers object
		*@note By default, the num parameter is NULL, indicating that 
		*Precession/Nutation computation should be skipped; this computation 
		*is only occasionally required.
		*/
		virtual void update( KStarsData *data, KSNumbers *num=0 );

	/**
		*@short Draw constellation lines on the sky map.
		*@p ks pointer to the KStars object
		*@p psky Reference to the QPainter on which to paint
		*@p scale scaling factor (1.0 for screen draws)
		*/
		virtual void draw( KStars *ks, QPainter& psky, double scale );

	/**
		*@short Clear the list of satellite tracks
		*/
		void clear();

		QList<SatelliteTrack*>& satList() { return SatList; }

	private:
		QList<SatelliteTrack*> SatList;
		QStringList SatelliteNames;
};

#endif
