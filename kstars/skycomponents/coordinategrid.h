/***************************************************************************
                          coordinategrid.h  -  K Desktop Planetarium
                             -------------------
    begin                : 15 Sept. 2005
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

#ifndef COORDINATEGRID_H
#define COORDINATEGRID_H

#include "noprecessindex.h"

/**
	*@class CoordinateGrid
	*Collection of all the circles in the coordinate grid

	*@author Jason Harris
	*@version 0.1
	*/
class CoordinateGrid : public NoPrecessIndex
{
public:
    /**
    	*@short Constructor
    	*Simply adds all of the coordinate grid circles 
    	*(meridians and parallels)
    	*@p parent Pointer to the parent SkyComposite object
    	*/
    CoordinateGrid( SkyComposite *parent );

    void preDraw( SkyPainter *skyp );

    bool selected();
};


#endif
