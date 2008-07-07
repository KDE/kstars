/***************************************************************************
                          customcatalogcomposite.cpp  -  K Desktop Planetarium
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
#include "customcatalogcomposite.h"
#include "kstarsdata.h"

CustomCatalogComposite::CustomCatalogComposite( SkyComponent *parent )
    : SkyComposite(parent) 
{
}

void CustomCatalogComposite::init() {
    data = KStarsData::Instance();
    
     foreach ( SkyComponent *component, components() )
        component->init();
}
