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

#include "opsguides.h"
#include "ksfilereader.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skycomponents/skymapcomposite.h"
#include "Options.h"

OpsGuides::OpsGuides( KStars *_ks )
        : QFrame( _ks ), ksw(_ks)
{
    setupUi( this );
    connect( kcfg_ShowCNames, SIGNAL( toggled( bool ) ),
             this, SLOT( slotToggleConstellOptions( bool ) ) );
    connect( kcfg_ShowMilkyWay, SIGNAL( toggled( bool ) ),
             this, SLOT( slotToggleMilkyWayOptions( bool ) ) );
    connect( kcfg_ShowGround, SIGNAL( toggled( bool ) ),
             this, SLOT( slotToggleOpaqueGround( bool ) ) );

    foreach( QString item,  ksw->data()->skyComposite()->getCultureNames() )
        kcfg_SkyCulture->addItem( i18nc("Sky Culture", item.toUtf8().constData() ) );

    // When setting up the widget, update the enabled status of the
    // checkboxes depending on the options.
    slotToggleOpaqueGround( Options::showGround() ); 
    slotToggleConstellOptions( Options::showCNames() );
    slotToggleMilkyWayOptions( Options::showMilkyWay() );

}

OpsGuides::~OpsGuides()
{}

void OpsGuides::slotToggleConstellOptions( bool state ) {
    ConstellOptions->setEnabled( state );
}

void OpsGuides::slotToggleMilkyWayOptions( bool state ) {
    kcfg_FillMilkyWay->setEnabled( state );
}

void OpsGuides::slotToggleOpaqueGround( bool state ) {
    kcfg_ShowHorizon->setEnabled( !state );
}

#include "opsguides.moc"
