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
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skyobject.h"

SkyComponent::SkyComponent( SkyComponent *parent, bool (*visibleMethod)() )
{
	Parent = parent;
	visible = visibleMethod;
}

SkyComponent::~SkyComponent()
{
}

bool SkyComponent::isExportable()
{
	return true;
}

void SkyComponent::drawExportable(KStars *ks, QPainter& psky, double scale)
{
	if (isExportable())
		draw(ks, psky, scale);
}

//Hand the message up to SkyMapComposite
void SkyComponent::emitProgressText( const QString &message ) {
	parent()->emitProgressText( message );
}

SkyObject* SkyComponent::findByName( const QString & ) { return 0; }

SkyObject* SkyComponent::objectNearest( SkyPoint *, double & ) { return 0; }

//Reimplemented in Solar system components
bool SkyComponent::addTrail( SkyObject * ) { return false; }
bool SkyComponent::hasTrail( SkyObject *, bool & ) { return false; }
bool SkyComponent::removeTrail( SkyObject * ) { return false; }
void SkyComponent::clearTrailsExcept( SkyObject * ) { return; }
void SkyComponent::drawTrails( KStars *, QPainter &, double ) { return; }
