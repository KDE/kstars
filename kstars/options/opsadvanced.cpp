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

#include <qlabel.h>
#include <qcheckbox.h>

#include "Options.h"
#include "kstars.h"
#include "widgets/timestepbox.h"

OpsAdvanced::OpsAdvanced( KStars *_ks )
        : QFrame( _ks )
{
    setupUi( this );

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
}

#include "opsadvanced.moc"
