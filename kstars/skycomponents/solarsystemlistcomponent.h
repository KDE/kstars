/***************************************************************************
                solarsystemlistcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/22/09
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

#ifndef SOLARSYSTEMLISTCOMPONENT_H
#define SOLARSYSTEMLISTCOMPONENT_H

#include "listcomponent.h"

/**
 *@class SolarSystemListComponent
 *
 *@author Jason Harris
 *@version 1.0
 */
class SolarSystemListComponent : public ListComponent
{
 public:
  SolarSystemListComponent();
  ~SolarSystemListComponent();

  /**
   *@short Add a Trail to the specified SkyObject.
   *@p o Pointer to the SkyObject to which a Trail will be added
   */
  bool addTrail( SkyObject *o );
  bool removeTrail( SkyObject *o );
  
  QList<SkyObject*>& trailList() { return TrailList; }
  
 private:
  QList<SkyObject*> TrailList;

};

#endif
