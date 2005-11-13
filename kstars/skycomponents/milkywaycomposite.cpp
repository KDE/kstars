/***************************************************************************
                          milkywaycomposite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 12 Nov. 2005
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

#include <klocale.h>

#include "kstarsdata.h"

#include "milkywaycomposite.h"
#include "milkywaycomponent.h"

MilkyWayComposite::MilkyWayComposite( SkyComponent *parent, bool (*visibleMethod)() ) 
	: SkyComposite(parent) 
{
	for ( uint i=1; i <= NMWFILES; ++i ) {
		QString fname;
		if ( i < 10 ) fname = "mw0" + QString().setNum(i) + ".dat";
		else          fname = "mw" + QString().setNum(i) + ".dat";

		addComponent( new MilkyWayComponent( this, fname, visibleMethod ) );
	}
}
