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

OpsGuides::OpsGuides( KStars *_ks )
        : QFrame( _ks ), ksw(_ks)
{
    setupUi( this );

    connect( kcfg_ShowCNames, SIGNAL( clicked() ),
             this, SLOT( slotToggleConstellOptions() ) );
    connect( kcfg_ShowMilkyWay, SIGNAL( clicked() ),
             this, SLOT( slotToggleMilkyWayOptions() ) );

    foreach( QString item,  ksw->data()->skyComposite()->getCultureNames() )
        kcfg_SkyCulture->addItem( i18nc("Sky Culture", item.toLocal8Bit().data() ) );

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
