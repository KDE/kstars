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

#include <qlabel.h>
#include <qcheckbox.h>
#include <qgroupbox.h>

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

	connect( kcfg_HideOnSlew, SIGNAL( clicked() ), this, SLOT( slotToggleHideOptions() ) );
}

OpsAdvanced::~OpsAdvanced() {}

void OpsAdvanced::slotChangeTimeScale( float newScale ) {
	Options::setSlewTimeScale( newScale );
}

void OpsAdvanced::slotToggleHideOptions() {
	textLabelHideTimeStep->setEnabled( kcfg_HideOnSlew->isChecked() );
	SlewTimeScale->setEnabled( kcfg_HideOnSlew->isChecked() );
	HideBox->setEnabled( kcfg_HideOnSlew->isChecked() );
/*	kcfg_MagLimitHideStar->setEnabled( kcfg_HideOnSlew->isChecked() );
	textLabelMag1->setEnabled( kcfg_HideOnSlew->isChecked() );
	kcfg_HidePlanets->setEnabled( kcfg_HideOnSlew->isChecked() );
	kcfg_HideMessier->setEnabled( kcfg_HideOnSlew->isChecked() );
	kcfg_HideNGC->setEnabled( kcfg_HideOnSlew->isChecked() );
	kcfg_HideIC->setEnabled( kcfg_HideOnSlew->isChecked() );
	kcfg_HideMilkyWay->setEnabled( kcfg_HideOnSlew->isChecked() );
	kcfg_HideCNames->setEnabled( kcfg_HideOnSlew->isChecked() );
	kcfg_HideCLines->setEnabled( kcfg_HideOnSlew->isChecked() );
	kcfg_HideCBounds->setEnabled( kcfg_HideOnSlew->isChecked() );
	kcfg_HideGrid->setEnabled( kcfg_HideOnSlew->isChecked() );*/
}

#include "opsadvanced.moc"
