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

#ifndef COORDINATEGRIDCOMPOSITE_H
#define COORDINATEGRIDCOMPOSITE_H

#include "skycomposite.h"

/**@class CoordinateGridComposite
*Collection of all the circles in the coordinate grid

*@author Jason Harris
*@version 0.1
*/

class CoordinateGridComposite : public SkyComposite 
{
 public:
  CoordinateGridComposite( SkyComposite* );
};


#endif
