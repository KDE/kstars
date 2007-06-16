/***************************************************************************
                          satellitecomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 22 Nov 2006
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

#ifndef SATELLITECOMPOSITE_H
#define SATELLITECOMPOSITE_H

#include <QStringList>
#include <QVector>

#include "skycomposite.h"
extern "C" {
#include "satlib/SatLib.h"
}

#define DT 10.0    //10-sec timesteps
#define NSTEPS 360 //360 steps == 1 hour of coverage

class KStarsData;

class SatelliteComposite : public SkyComposite 
{
	public:
	/**
		*@short Constructor
		*@param parent Pointer to the parent SkyComponent object
		*/
		SatelliteComposite( SkyComponent *parent );

		~SatelliteComposite();

	/**
		*@short Initialize the Satellite composite
		*
		*@param data Pointer to the KStarsData object
		*/
		virtual void init( KStarsData *data );

	/**
		*@short Update the satellite tracks
		*
		*@param data Pointer to the KStarsData object
		*@param num Pointer to the KSNumbers object
		*/
		virtual void update( KStarsData *data, KSNumbers *num=0 );

	private:
		QVector<SPositionSat*> pSat;
		QStringList SatelliteNames;
		long double JD_0;
};

#endif
