/***************************************************************************
                          opssupernovae.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Thu, 25 Aug 2011
    copyright            : (C) 2011 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "opssupernovae.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"
#include "skycomponents/supernovaecomponent.h"


OpsSupernovae::OpsSupernovae(KStars* _ks)
        : QFrame( _ks ), ksw(_ks)
{
    setupUi( this );

    // Signals and slots connections
    connect( supUpdateButton, SIGNAL( clicked() ), this, SLOT( slotUpdateRecentSupernovae() ) );
    connect( kcfg_ShowSupernovae, SIGNAL( toggled( bool ) ), this, SLOT( slotShowSupernovae( bool ) ) );
    connect( kcfg_UpdateSupernovaeOnStartup, SIGNAL( toggled(bool) ), this, SLOT( slotUpdateOnStartup (bool)));
    connect( kcfg_ShowSupernovaAlerts, SIGNAL( toggled(bool) ),this, SLOT( slotShowSupernovaAlerts( bool ) ) );
    connect( kcfg_MagnitudeLimitShowSupernovae, SIGNAL( valueChanged(double) ), this, SLOT( slotSetShowMagnitudeLimit( double )));
    connect( kcfg_MagnitudeLimitAlertSupernovae, SIGNAL( valueChanged(double) ), this, SLOT( slotSetAlertMagnitudeLimit(double)));
}

OpsSupernovae::~OpsSupernovae()
{}

void OpsSupernovae::slotUpdateRecentSupernovae()
{
    KStarsData::Instance()->skyComposite()->supernovaeComponent()->slotTriggerDataFileUpdate();
}

void OpsSupernovae::slotShowSupernovae( bool on )
{
    kcfg_ShowSupernovae->setChecked( on );
}

void OpsSupernovae::slotShowSupernovaAlerts(bool on)
{
    kcfg_ShowSupernovaAlerts->setChecked(on);
}

void OpsSupernovae::slotUpdateOnStartup(bool on)
{
    kcfg_UpdateSupernovaeOnStartup->setChecked(on);
}

void OpsSupernovae::slotSetShowMagnitudeLimit(double value)
{
    kcfg_MagnitudeLimitShowSupernovae->setValue(value);
}

void OpsSupernovae::slotSetAlertMagnitudeLimit(double value)
{
    kcfg_MagnitudeLimitAlertSupernovae->setValue(value);
}



#include "opssupernovae.moc"

