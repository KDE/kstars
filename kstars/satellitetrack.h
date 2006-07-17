/***************************************************************************
                          satellitetrack.h  -  description
                             -------------------
    begin                : Fri 14 July 2006
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

#ifndef SATELLITETRACK_H_
#define SATELLITETRACK_H_

#include <QList>
#include "skyline.h"
#include "SatLib.h"

class dms;

/**
	*@class SatelliteTrack
	*
	*The path of a satellite across the sky, composed of a QList of SkyLines.
	*A SatelliteTrack stores the name of the satellite in addition to the path.
	*/
class SatelliteTrack {
	public:
	/**
		*Construct an empty satellite track
		*/
		SatelliteTrack();

	/**
		*Construct a satellite track from an array of SPositionSat structures.
		*
		*@param pSat An array of satellite positions from which the satellite track 
		*will be constructed.  The SPositionSat struct is defined in the SatLib 
		*library; it encodes the instantaneous position of a satellite.  Most often, 
		*the array of SPositionSats will be returned by the SatLib function 
		*SatFindPosition(), which returns the satellite's position for a number of 
		*instants over a given time interval.
		*@param nPos The number of positions in the pSat array
		*@param LST pointer to the local sideral time (needed to convert 
		*Horizontal coords to Equatorial coords)
		*@param lat pointer to the geographic latitude (needed to convert 
		*Horizontal coords to Equatorial coords)
		*/
		SatelliteTrack( SPositionSat *pSat[], long nPos, const dms *LST, const dms *lat );

		~SatelliteTrack();

		const QList<SkyLine*>& path() const;

	private:
		QList<SkyLine*> Path;

};

#endif
