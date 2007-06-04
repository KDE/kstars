/***************************************************************************
                          milkywaycomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
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

#ifndef MILKYWAYCOMPOSITE_H
#define MILKYWAYCOMPOSITE_H

#define NMWFILES  11

#include <QObject>
#include "skycomposite.h"


/**
	*@class MlkyWayComposite
	*The Milky Way is stored in 11 separate "chunks".
	*This composite stores a MilkyWayComponent for each chunk.
	*
	*@author Jason Harris
	*@version 0.1
	*/
class MilkyWayComposite : public SkyComposite
{
	public:
		/**
			*@short Constructor
			*@p parent pointer to the parent SkyComponent
			*/
		MilkyWayComposite( SkyComponent *parent, bool (*visibleMethod)() );
};

#endif
