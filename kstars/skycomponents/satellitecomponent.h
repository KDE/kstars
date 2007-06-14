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


#include "linelistcomponent.h"
extern "C" {
#include "satlib/SatLib.h"
}

class SatelliteComponent : public LineListComponent {
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
		*@short Initialize the component using a SPositionSat array
		*@p data Pointer to the KStarsData object
		*/
		void init(const QString &name, KStarsData *data, SPositionSat *pSat[], int nsteps);

		void draw( KStars *ks, QPainter &psky, double scale );

		QList<double>& jdList() { return JDList; }

	private:
		QStringList SatelliteNames;
		long double JulianDate;
		QList<double> JDList;
};

#endif
