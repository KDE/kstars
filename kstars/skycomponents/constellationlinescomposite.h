/***************************************************************************
               constellationlinescomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 25 Oct. 2005
    copyright            : (C) 2005 by Jason Harris
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

#ifndef CONSTELLATIONLINESCOMPOSITE_H
#define CONSTELLATIONLINESCOMPOSITE_H

#include "skycomposite.h"

/**
	*@class ConstellationLinesComposite
	*Collection of lines making the 88 constellations

	*@author Jason Harris
	*@version 0.1
	*/

class ConstellationLinesComposite : public SkyComposite 
{
	public:
	/**
		*@short Constructor
		*Simply adds all of the coordinate grid circles 
		*(meridians and parallels)
		*@p parent Pointer to the parent SkyComponent object
		*/
		ConstellationLinesComposite( SkyComponent *parent, KStarsData *data );

	/**
		*@short Initialize the Constellation lines composite
		*
		*Reads the constellation lines data from clines.dat.
		*Each line in the file contains a command character ("M" means move to 
		*this position without drawing a line, "D" means draw a line from 
		*the previous position to this one), followed by the genetive name of 
		*a star, which marks the position of the constellation node.
		*@p data Pointer to the KStarsData object
		*/
		virtual void init( KStarsData *data );
};


#endif
