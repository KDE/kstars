/***************************************************************************
                          objlistpopupmenu.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed June 9 2010
    copyright            : (C) 2010 by Victor Carbune
    email                : victor.carbune@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "objlistpopupmenu.h"

#include <KGlobal>
#include <KLocale>

#include "kstars.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"

#include <config-kstars.h>


ObjListPopupMenu::ObjListPopupMenu()
        : KMenu( 0 )
{}

ObjListPopupMenu::~ObjListPopupMenu() { }

void ObjListPopupMenu::init()
{
    ks = KStars::Instance();
    clear();
}

void ObjListPopupMenu::showAddToSession()
{
    addAction( i18n( "Add to session plan" ), ks->observingList(), SLOT( slotAddToSession() ) );
}

void ObjListPopupMenu::showAddVisibleTonight()
{
    addAction( i18n( "Add objects visible tonight to session plan" ), ks->observingList(), SLOT( slotAddVisibleObj() ) );
}

void ObjListPopupMenu::showCenter()
{
    addAction( i18n( "Center" ), ks->observingList(), SLOT( slotCenterObject() ) );
}

void ObjListPopupMenu::showScope()
{
    addAction( i18nc( "Show the selected object in the telescope", "Scope" ), ks->observingList(), SLOT( slotSlewToObject() ) );
}

void ObjListPopupMenu::showDetails()
{
    addAction( i18nc( "Show Detailed Information Dialog", "Details" ), ks->observingList(), SLOT( slotDetails() ) );
}

void ObjListPopupMenu::showAVT()
{
    addAction( i18n( "Altitude vs. Time" ), ks->observingList(), SLOT( slotAVT() ) );
}

void ObjListPopupMenu::showLinks()
{
    if( !ks->observingList()->currentObject()->isSolarSystem() ) {
        addAction( i18n( "Show SDSS image" ), ks->observingList(), SLOT( slotGetImage() ) );
        addAction( i18n( "Show DSS image" ), ks->observingList(), SLOT( slotDSS() ) );
    }

    addAction( i18n( "Show images from web " ), ks->observingList(), SLOT( slotGoogleImage() ) );
    addSeparator();
}

void ObjListPopupMenu::showRemoveFromWishList()
{
    addAction( i18n("Remove from WishList"), ks->observingList(), SLOT( slotRemoveSelectedObjects() ) );
}

void ObjListPopupMenu::showRemoveFromSessionPlan()
{
    addAction( i18n("Remove from Session Plan"), ks->observingList(), SLOT( slotRemoveSelectedObjects() ) );
}

void ObjListPopupMenu::initPopupMenu(bool showAddToSession, bool showCenter, bool showDetails, bool showScope, bool showRemove, bool showLinks, bool sessionView ) {
/*
    //Insert item for adding the object to the session view
    if( showAddToSession )
        addAction( i18n( "Add to session plan" ), ks->observingList(), SLOT( slotAddToSession() ) );
    if( !sessionView )
        addAction( i18n( "Add objects visible tonight to session plan" ), ks->observingList(), SLOT( slotAddVisibleObj() ) );
    addSeparator();

    //Insert item for centering on object
    if( showCenter )
        addAction( i18n( "Center" ), ks->observingList(), SLOT( slotCenterObject() ) );

    //Insert item for Slewing to the object
    if( showScope )
        addAction( i18nc( "Show the selected object in the telescope", "Scope" ), ks->observingList(), SLOT( slotSlewToObject() ) );
    addSeparator();

    //Insert item for Showing details dialog
    if( showDetails )
        addAction( i18nc( "Show Detailed Information Dialog", "Details" ), ks->observingList(), SLOT( slotDetails() ) );

    //Insert item for opening the Altitude vs time dialog
    addAction( i18n( "Altitude vs. Time" ), ks->observingList(), SLOT( slotAVT() ) );
    addSeparator();

    //Insert item for dowloading different images
    if( showLinks ) {
        if( ! ks->observingList()->currentObject()->isSolarSystem() ) {
            addAction( i18n( "Show SDSS image" ), ks->observingList(), SLOT( slotGetImage() ) );
            addAction( i18n( "Show DSS image" ), ks->observingList(), SLOT( slotDSS() ) );
        }
        addAction( i18n( "Show images from web " ), ks->observingList(), SLOT( slotGoogleImage() ) );
        addSeparator();
    }
    //Insert item for Removing the object(s)
    if( showRemove ) {
        if( ! sessionView )
            addAction( i18n("Remove from WishList"), ks->observingList(), SLOT( slotRemoveSelectedObjects() ) );
        else
            addAction( i18n("Remove from Session Plan"), ks->observingList(), SLOT( slotRemoveSelectedObjects() ) );
    }
*/
}

#include "objlistpopupmenu.moc"
