/***************************************************************************
                          customcatalogcomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2008/07/07
    copyright            : (C) 2008 by Jason Harris
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

#ifndef CUSTOMCATALOGCOMPOSITE_H
#define CUSTOMCATALOGCOMPOSITE_H

#include "skycomposite.h"

/**
 * @class CustomCatalogComposite
 * 
 * A simple container for custom catalog components.
 * There is no simpler possible derivation of the abstract SkyComposite class,
 * All we do here is define the init() function.
 */
class CustomCatalogComposite : public SkyComposite
{
public:
    CustomCatalogComposite( SkyComponent *parent );
    virtual void init();    
};

#endif
