/***************************************************************************
                          skymapcomposite.cpp  -  K Desktop Planetarium
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

#include "skymapcomposite.h"

#include "horizoncomponent.h"
#include "milkywaycomponent.h"
#include "coordinategridcomponent.h"

SkyMapComposite::SkyMapComposite(SkyComposite *parent) : SkyComposite(parent)
{
	// beware the order of adding components
	// first added component will be drawn first
	// so horizon should be one of the last components
	addComponent(new MilkyWayComponent(this));
	addComponent(new HorizonComponent(this));
	addComponent(new CoordinateGridComponent(this));
	// add ecliptic
	// add solar system
	// ...
}
