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

#include "skycomposite.h"

class KStarsData;

class SatelliteComposite : public SkyComposite 
{
	public:
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComponent object
		*/
		SatelliteComposite( SkyComponent *parent );

	/**
		*@short Initialize the Satellite composite
		*
		*@p data Pointer to the KStarsData object
		*/
		virtual void init( KStarsData *data );
};

#endif
