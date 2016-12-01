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

#include <KActionCollection>
#include <KConfigDialog>

#include "opssolarsystem.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"

OpsSolarSystem::OpsSolarSystem()
        : QFrame(KStars::Instance())
{
    setupUi( this );

    connect( kcfg_ShowSolarSystem, SIGNAL( toggled(bool) ), SLOT( slotAllWidgets(bool) ) );
    connect( kcfg_ShowAsteroids, SIGNAL( toggled(bool) ), SLOT( slotAsteroidWidgets(bool) ) );
    connect( kcfg_ShowComets, SIGNAL( toggled(bool) ), SLOT( slotCometWidgets(bool) ) );
    connect( ClearAllTrails, SIGNAL( clicked() ), KStars::Instance(), SLOT( slotClearAllTrails() ) );
    connect( showAllPlanets, SIGNAL( clicked() ), this, SLOT( slotSelectPlanets() ) );
    connect( showNonePlanets, SIGNAL( clicked() ), this, SLOT( slotSelectPlanets() ) );

    kcfg_MagLimitAsteroid->setMaximum( 30.0 );
    kcfg_MaxRadCometName->setMaximum( 100.0 );

    slotAsteroidWidgets( kcfg_ShowAsteroids->isChecked() );
    slotCometWidgets( kcfg_ShowComets->isChecked() );

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "settings" );

    connect( m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL( clicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL( clicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog->button(QDialogButtonBox::Cancel), SIGNAL( clicked() ), SLOT( slotCancel() ) );

    connect( solarButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonPressed), this, [&](){ isDirty = true;});
}

OpsSolarSystem::~OpsSolarSystem()
{
}

void OpsSolarSystem::slotAllWidgets( bool on ) {
    MajorBodiesBox->setEnabled( on );
    MinorBodiesBox->setEnabled( on );
    TrailsBox->setEnabled( on );
}

void OpsSolarSystem::slotAsteroidWidgets( bool on ) {
    kcfg_MagLimitAsteroid->setEnabled( on );
    kcfg_ShowAsteroidNames->setEnabled( on );
    kcfg_AsteroidLabelDensity->setEnabled( on );
    textLabel3->setEnabled( on );
    textLabel6->setEnabled( on );
    LabelDensity->setEnabled( on );
}

void OpsSolarSystem::slotCometWidgets( bool on ) {
    kcfg_ShowCometNames->setEnabled( on );
    kcfg_MaxRadCometName->setEnabled( on );
    textLabel4->setEnabled( on );
}

void OpsSolarSystem::slotSelectPlanets() {
    bool b=true;
    if ( QString(sender()->objectName()) == "showNonePlanets" )
        b = false;

    kcfg_ShowSun->setChecked( b );
    kcfg_ShowMoon->setChecked( b );
    kcfg_ShowMercury->setChecked( b );
    kcfg_ShowVenus->setChecked( b );
    kcfg_ShowMars->setChecked( b );
    kcfg_ShowJupiter->setChecked( b );
    kcfg_ShowSaturn->setChecked( b );
    kcfg_ShowUranus->setChecked( b );
    kcfg_ShowNeptune->setChecked( b );
    //kcfg_ShowPluto->setChecked( b );
}

void OpsSolarSystem::slotApply()
{
    if (isDirty == false)
        return;

    isDirty = false;

    // update time for all objects because they might be not initialized
    // it's needed when using horizontal coordinates
    KStars::Instance()->data()->setFullTimeUpdate();
    KStars::Instance()->updateTime();
    KStars::Instance()->map()->forceUpdate();
}

