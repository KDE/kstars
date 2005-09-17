/***************************************************************************
                          coordinategridcomposite.h  -  K Desktop Planetarium
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

#include "coordinategridcomposite.h"
#include "coordinategridcomponent.h"

CoordinateGridComposite::CoordinateGridComposite( SkyComposite *parent ) 
  : SkyComposite(parent) 
{
  //Parallels
  for ( double dec=-80.0; dec <= 80.0; dec += 10.0 ) 
    addComponent( new CoordinatGridComponent( this, true, dec ) );
  
  //Meridians
  for ( double ra=0.0; ra < 12.0; ra += 2.0 ) 
    addComponent( new CoordinatGridComponent( this, false, ra ) );
}

