/***************************************************************************
                          opssolarsystem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 22 Aug 2004
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
#include <qcheckbox.h>
#include <qlabel.h>
#include <kpushbutton.h>
#include "opssolarsystem.h"
#include "kstars.h"
#include "magnitudespinbox.h"

OpsSolarSystem::OpsSolarSystem( QWidget *parent, const char *name, WFlags fl )
 : OpsSolarSystemUI( parent, name, fl )
{
	ksw = (KStars *)parent;
	
	connect( kcfg_ShowAsteroids, SIGNAL( toggled(bool) ), SLOT( slotAsteroidWidgets(bool) ) );
	connect( kcfg_ShowComets, SIGNAL( toggled(bool) ), SLOT( slotCometWidgets(bool) ) );
	connect( ClearAllTrails, SIGNAL( clicked() ), ksw, SLOT( slotClearAllTrails() ) );

	slotAsteroidWidgets( kcfg_ShowAsteroids->isChecked() );
	slotCometWidgets( kcfg_ShowComets->isChecked() );
}


OpsSolarSystem::~OpsSolarSystem()
{
}

void OpsSolarSystem::slotAsteroidWidgets( bool on ) {
	kcfg_MagLimitAsteroid->setEnabled( on );
	kcfg_ShowAsteroidNames->setEnabled( on );
	kcfg_MagLimitAsteroidName->setEnabled( on );
	textLabel3->setEnabled( on );
	textLabel5->setEnabled( on );
	textLabel6->setEnabled( on );
}

void OpsSolarSystem::slotCometWidgets( bool on ) {
	kcfg_ShowCometNames->setEnabled( on );
	kcfg_MaxRadCometName->setEnabled( on );
	textLabel4->setEnabled( on );
}

#include "opssolarsystem.moc"
