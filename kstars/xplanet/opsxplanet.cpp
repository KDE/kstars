/***************************************************************************
                          opsxplanet.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 26 Nov 2008
    copyright            : (C) 2008 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "opsxplanet.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"

OpsXplanet::OpsXplanet( KStars *_ks )
        : QFrame( _ks ), ksw(_ks)
{
    setupUi( this );

    #ifdef Q_OS_OSX
    connect(kcfg_xplanetIsInternal, SIGNAL(clicked()), this, SLOT(toggleXPlanetInternal()));
    kcfg_xplanetIsInternal->setToolTip(i18n("Internal or External XPlanet?"));

    if(Options::xplanetIsInternal())
        kcfg_XplanetPath->setEnabled(false);

    #else
     kcfg_xplanetIsInternal->setVisible(false);
    #endif

    // Init projections combobox
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "No projection"), "no projection");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Ancient"), "ancient");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Azimuthal"), "azimuthal");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Bonne"), "bonne");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Gnomonic"), "gnomonic");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Hemisphere"), "hemisphere");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Lambert"), "lambert");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Mercator"), "mercator");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Mollweide"), "mollweide");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Orthographic"), "orthographic");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Peters"), "peters");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Polyconic"), "polyconic");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "Rectangular"), "rectangular");
    kcfg_XplanetProjection->addItem( i18nc("Map projection method", "TSC"), "tsc");

    // Enable/Disable somme widgets
    connect( kcfg_XplanetWait, SIGNAL( toggled(bool) ), SLOT( slotUpdateWidgets(bool) ) );
    connect( kcfg_XplanetConfigFile, SIGNAL( toggled(bool) ), SLOT( slotConfigFileWidgets(bool) ) );
    connect( kcfg_XplanetStarmap, SIGNAL( toggled(bool) ), SLOT( slotStarmapFileWidgets(bool) ) );
    connect( kcfg_XplanetArcFile, SIGNAL( toggled(bool) ), SLOT( slotArcFileWidgets(bool) ) );
    connect( kcfg_XplanetLabel, SIGNAL( toggled(bool) ), SLOT( slotLabelWidgets(bool) ) );
    connect( kcfg_XplanetMarkerFile, SIGNAL( toggled(bool) ), SLOT( slotMarkerFileWidgets(bool) ) );
    connect( kcfg_XplanetMarkerBounds, SIGNAL( toggled(bool) ), SLOT( slotMarkerBoundsWidgets(bool) ) );
    connect( kcfg_XplanetProjection, SIGNAL( currentIndexChanged(int) ), SLOT( slotProjectionWidgets(int) ) );
    connect( kcfg_XplanetBackground, SIGNAL( toggled(bool) ), SLOT( slotBackgroundWidgets(bool) ) );

    kcfg_XplanetWaitValue->setEnabled( Options::xplanetWait() );
    textLabelXplanetSecondes->setEnabled( Options::xplanetWait() );
    kcfg_XplanetConfigFilePath->setEnabled( Options::xplanetConfigFile() );
    kcfg_XplanetStarmapPath->setEnabled( Options::xplanetStarmap() );
    kcfg_XplanetArcFilePath->setEnabled( Options::xplanetArcFile() );
    kcfg_XplanetLabelLocalTime->setEnabled( Options::xplanetLabel() );
    kcfg_XplanetLabelGMT->setEnabled( Options::xplanetLabel() );
    textLabelXplanetLabelString->setEnabled( Options::xplanetLabel() );
    kcfg_XplanetLabelString->setEnabled( Options::xplanetLabel() );
    textLabelXplanetDateFormat->setEnabled( Options::xplanetLabel() );
    kcfg_XplanetDateFormat->setEnabled( Options::xplanetLabel() );
    textLabelXplanetFontSize->setEnabled( Options::xplanetLabel() );
    kcfg_XplanetFontSize->setEnabled( Options::xplanetLabel() );
    textLabelXplanetColor->setEnabled( Options::xplanetLabel() );
    kcfg_XplanetColor->setEnabled( Options::xplanetLabel() );
    textLabelLabelPos->setEnabled( Options::xplanetLabel() );
    kcfg_XplanetLabelTL->setEnabled( Options::xplanetLabel() );
    kcfg_XplanetLabelTR->setEnabled( Options::xplanetLabel() );
    kcfg_XplanetLabelBR->setEnabled( Options::xplanetLabel() );
    kcfg_XplanetLabelBL->setEnabled( Options::xplanetLabel() );
    kcfg_XplanetMarkerFilePath->setEnabled( Options::xplanetMarkerFile() );
    kcfg_XplanetMarkerBounds->setEnabled( Options::xplanetMarkerFile() );
    if( Options::xplanetMarkerFile() && Options::xplanetMarkerBounds() )
        kcfg_XplanetMarkerBoundsPath->setEnabled( true );
    else
        kcfg_XplanetMarkerBoundsPath->setEnabled( false );
    if( Options::xplanetProjection() == 0 )
        groupBoxBackground->setEnabled( false );
    kcfg_XplanetBackgroundImage->setEnabled( Options::xplanetBackgroundImage() );
    kcfg_XplanetBackgroundImagePath->setEnabled( Options::xplanetBackgroundImage() );
    kcfg_XplanetBackgroundColor->setEnabled( Options::xplanetBackgroundImage() );
    kcfg_XplanetBackgroundColorValue->setEnabled( Options::xplanetBackgroundImage() );
    if( Options::xplanetProjection() == 0 )
        groupBoxBackground->setEnabled( false );
}

OpsXplanet::~OpsXplanet()
{}

void OpsXplanet::toggleXPlanetInternal()
{
    kcfg_XplanetPath->setEnabled(!kcfg_xplanetIsInternal->isChecked());
    if(kcfg_xplanetIsInternal->isChecked())
        kcfg_XplanetPath->setText("*Internal XPlanet*");
    else
        kcfg_XplanetPath->setText("/usr/local/bin/xplanet");
}

void OpsXplanet::slotUpdateWidgets( bool on ) {
    kcfg_XplanetWaitValue->setEnabled( on );
    textLabelXplanetSecondes->setEnabled( on );
}

void OpsXplanet::slotConfigFileWidgets( bool on ) {
    kcfg_XplanetConfigFilePath->setEnabled( on );
}

void OpsXplanet::slotStarmapFileWidgets( bool on ) {
    kcfg_XplanetStarmapPath->setEnabled( on );
}

void OpsXplanet::slotArcFileWidgets( bool on ) {
    kcfg_XplanetArcFilePath->setEnabled( on );
}

void OpsXplanet::slotLabelWidgets( bool on ) {
    kcfg_XplanetLabelLocalTime->setEnabled( on );
    kcfg_XplanetLabelGMT->setEnabled( on );
    textLabelXplanetLabelString->setEnabled( on );
    kcfg_XplanetLabelString->setEnabled( on );
    textLabelXplanetDateFormat->setEnabled( on );
    kcfg_XplanetDateFormat->setEnabled( on );
    textLabelXplanetFontSize->setEnabled( on );
    kcfg_XplanetFontSize->setEnabled( on );
    textLabelXplanetColor->setEnabled( on );
    kcfg_XplanetColor->setEnabled( on );
    textLabelLabelPos->setEnabled( on );
    kcfg_XplanetLabelTL->setEnabled( on );
    kcfg_XplanetLabelTR->setEnabled( on );
    kcfg_XplanetLabelBR->setEnabled( on );
    kcfg_XplanetLabelBL->setEnabled( on );
}

void OpsXplanet::slotMarkerFileWidgets( bool on ) {
    kcfg_XplanetMarkerFilePath->setEnabled( on );
    kcfg_XplanetMarkerBounds->setEnabled( on );
    if (kcfg_XplanetMarkerBounds->isChecked() )
        kcfg_XplanetMarkerBoundsPath->setEnabled( on );
}

void OpsXplanet::slotMarkerBoundsWidgets( bool on ) {
    kcfg_XplanetMarkerBoundsPath->setEnabled( on );
}

void OpsXplanet::slotProjectionWidgets( int index ) {
    if( index == 0 )
        groupBoxBackground->setEnabled( false );
    else
        groupBoxBackground->setEnabled( true );

    if( ! kcfg_XplanetBackground->isChecked() ) {
        kcfg_XplanetBackgroundImage->setEnabled( false );
        kcfg_XplanetBackgroundImagePath->setEnabled( false );
        kcfg_XplanetBackgroundColor->setEnabled( false );
        kcfg_XplanetBackgroundColorValue->setEnabled( false );
    }
}

void OpsXplanet::slotBackgroundWidgets( bool on ) {
    kcfg_XplanetBackgroundImage->setEnabled( on );
    kcfg_XplanetBackgroundImagePath->setEnabled( on );
    kcfg_XplanetBackgroundColor->setEnabled( on );
    kcfg_XplanetBackgroundColorValue->setEnabled( on );
}


