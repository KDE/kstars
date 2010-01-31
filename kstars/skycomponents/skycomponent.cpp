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
#include <QPainter>

#include "Options.h"
#include "ksnumbers.h"
#include "skyobjects/skyobject.h"

SkyComponent::SkyComponent( SkyComposite *parent ) :
    m_parent( parent )
{}

SkyComponent::~SkyComponent()
{
}

//Hand the message up to SkyMapComposite
void SkyComponent::emitProgressText( const QString &message ) {
    parent()->emitProgressText( message );
}

SkyObject* SkyComponent::findByName( const QString & ) { return 0; }

SkyObject* SkyComponent::objectNearest( SkyPoint *, double & ) { return 0; }

//Reimplemented in Solar system components
bool SkyComponent::addTrail( SkyObject * ) { return false; }
bool SkyComponent::removeTrail( SkyObject * ) { return false; }
void SkyComponent::clearTrailsExcept( SkyObject * ) { return; }
void SkyComponent::drawTrails( QPainter & ) { return; }
QHash<int, QStringList>& SkyComponent::objectNames() { return parent()->objectNames(); }
QStringList& SkyComponent::objectNames(int type) { return parent()->objectNames(type); }
