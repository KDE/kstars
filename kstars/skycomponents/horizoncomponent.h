/***************************************************************************
                          horizoncomponent.h  -  K Desktop Planetarium
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

#ifndef HORIZONCOMPONENT_H
#define HORIZONCOMPONENT_H

#include "pointlistcomponent.h"

class SkyComposite;
class SkyMap;
class KSNumbers;

/**
	*@class HorizonComponent
	*Represents the horizon on the sky map.
	
	*@author Thomas Kabelmann
	*@version 0.1
	*/
class HorizonComponent: public PointListComponent
{
public:

    /**
     *@short Constructor
     *@p parent Pointer to the parent SkyComposite object
     */
    explicit HorizonComponent(SkyComposite *parent );

    /**
     *@short Destructor
     */
    virtual ~HorizonComponent();

    /**
     *@short Draw the Horizon on the Sky map
     *@p map Pointer to the SkyMap object
     *@p psky Reference to the QPainter on which to paint
     */
    virtual void draw( SkyPainter *skyp );

    virtual void update( KSNumbers* );

    bool selected();

private:
    void drawCompassLabels();

};

#endif
