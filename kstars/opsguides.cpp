/***************************************************************************
                          opsguides.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 6 Feb 2005
    copyright            : (C) 2005 by Jason Harris
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

#include <qbuttongroup.h>
#include <qcheckbox.h>
#include "opsguides.h"

OpsGuides::OpsGuides( QWidget* parent, const char* name, WFlags fl )
    : OpsGuidesUI( parent, name, fl )
{
	connect( kcfg_ShowCNames, SIGNAL( clicked() ), 
					 this, SLOT( slotToggleConstellOptions() ) );
	connect( kcfg_ShowMilkyWay, SIGNAL( clicked() ), 
					 this, SLOT( slotToggleMilkyWayOptions() ) );
}

OpsGuides::~OpsGuides()
{}

void OpsGuides::slotToggleConstellOptions() {
	ConstellOptions->setEnabled( kcfg_ShowCNames->isChecked() );
}

void OpsGuides::slotToggleMilkyWayOptions() {
	kcfg_FillMilkyWay->setEnabled( kcfg_ShowMilkyWay->isChecked() );
}

#include "opsguides.moc"
