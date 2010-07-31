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
#include <tqcheckbox.h>
#include <tqlabel.h>
#include <kpushbutton.h>
#include "opssolarsystem.h"
#include "kstars.h"
#include "magnitudespinbox.h"

OpsSolarSystem::OpsSolarSystem( TQWidget *parent, const char *name, WFlags fl )
 : OpsSolarSystemUI( parent, name, fl )
{
	ksw = (KStars *)parent;
	
	connect( kcfg_ShowAsteroids, TQT_SIGNAL( toggled(bool) ), TQT_SLOT( slotAsteroidWidgets(bool) ) );
	connect( kcfg_ShowComets, TQT_SIGNAL( toggled(bool) ), TQT_SLOT( slotCometWidgets(bool) ) );
	connect( ClearAllTrails, TQT_SIGNAL( clicked() ), ksw, TQT_SLOT( slotClearAllTrails() ) );
	connect( showAllPlanets, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotSelectPlanets() ) );
	connect( showNonePlanets, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotSelectPlanets() ) );

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

void OpsSolarSystem::slotSelectPlanets() {
	bool b=true;
	if ( sender()->name() == TQString( "showNonePlanets" ) ) b = false;
	
	kcfg_ShowSun->setChecked( b );
	kcfg_ShowMoon->setChecked( b );
	kcfg_ShowMercury->setChecked( b );
	kcfg_ShowVenus->setChecked( b );
	kcfg_ShowMars->setChecked( b );
	kcfg_ShowJupiter->setChecked( b );
	kcfg_ShowSaturn->setChecked( b );
	kcfg_ShowUranus->setChecked( b );
	kcfg_ShowNeptune->setChecked( b );
	kcfg_ShowPluto->setChecked( b );
}
#include "opssolarsystem.moc"
