/***************************************************************************
                          singlecomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/01
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

#include "listcomponent.h"

#include <QList>

#include "skymap.h" 

SingleComponent::SingleComponent(SkyComposite *parent)
{
	Parent = parent;
}

SingleComponent::~SingleComponent()
{
}

bool SingleComponent::addTrail( SkyObject *o ) { 
  if ( skyObject() == o ) {
    HasTrail = true;
    return true;
  }
  return false; 
}

bool SingleComponent::removeTrail( SkyObject *o ) {
  if ( skyObject() == o ) {
    HasTrail = false;
    return true;
  }
  return false;
}

