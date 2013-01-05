/***************************************************************************
                      horizontalcoordinategrid.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 01 Mar 2012
    copyright            : (C) 2012 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef HORIZONTALCOORDINATEGRID_H
#define HORIZONTALCOORDINATEGRID_H

#include "coordinategrid.h"

/**
	*@class HorizontalCoordinateGrid
	*Collection of all the circles in the horizontal coordinate grid

	*@author Jérôme SONRIER
	*@version 0.1
	*/
class HorizontalCoordinateGrid : public CoordinateGrid
{
public:
    /**
    	*@short Constructor
    	*Simply adds all of the coordinate grid circles 
    	*(meridians and parallels)
    	*@p parent Pointer to the parent SkyComposite object
    	*/
    explicit HorizontalCoordinateGrid( SkyComposite *parent );

    void preDraw( SkyPainter *skyp );
    
    void update( KSNumbers* );

    bool selected();
};


#endif
