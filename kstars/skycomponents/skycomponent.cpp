/***************************************************************************
                          skycomponent.cpp  -  K Desktop Planetarium
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

#include "skycomponent.h"
#include "skycomposite.h"

#include <QList>

#include "skymap.h" 

SkyComponent::SkyComponent(SkyComposite *parent)
{
	Parent = parent;
}

SkyComponent::~SkyComponent()
{
}

bool SkyComponent::isExportable()
{
	return true;
}

void SkyComponent::drawExportable(SkyMap *map, QPainter& psky, double scale)
{
	if (isExportable())
		draw(map, psky, scale);
}
