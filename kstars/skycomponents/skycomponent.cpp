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

#include <QList>
#include <QPainter>

#include "skycomponent.h"
#include "skycomposite.h"

#include "Options.h"
#include "skymap.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skyobject.h"

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

void SkyComponent::drawNameLabel(QPainter &psky, SkyObject *obj, int x, int y, double scale)
{
	int size(0);

	QFont stdFont( psky.font() );
	QFont smallFont( stdFont );
	smallFont.setPointSize( stdFont.pointSize() - 2 );
	if ( Options::zoomFactor() < 10.*MINZOOM ) {
		psky.setFont( smallFont );
	} else {
		psky.setFont( stdFont );
	}

	// get size of object
	size = labelSize(obj);
	
	int offset = int( ( 0.5*size + 4 ) );
	psky.drawText( x+offset, y+offset, obj->translatedName() );

	//Reset font
	psky.setFont( stdFont );
}

//TODO: Implement addTrail
bool SkyComponent::addTrail( SkyObject * ) { return false; }

//TODO: Implement findByName
SkyObject* SkyComponent::findByName( const QString &name ) {
	return 0;
}
