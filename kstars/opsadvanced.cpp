/***************************************************************************
                          opsadvanced.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 14 Mar 2004
    copyright            : (C) 2004 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "opsadvanced.h"
#include "Options.h"
#include "kstars.h"
#include "timestepbox.h"

OpsAdvanced::OpsAdvanced( QWidget *p, const char *name, WFlags fl ) 
	: OpsAdvancedUI( p, name, fl ) 
{
	ksw = (KStars *)p;

	//Initialize the timestep value
	SlewTimeScale->tsbox()->changeScale( Options::slewTimeScale() );

	connect( SlewTimeScale, SIGNAL( scaleChanged( float ) ), this, SLOT( slotChangeTimeScale( float ) ) );
}

OpsAdvanced::~OpsAdvanced() {}

void OpsAdvanced::slotChangeTimeScale( float newScale ) {
	Options::setSlewTimeScale( newScale );
}

#include "opsadvanced.moc"
