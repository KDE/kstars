/***************************************************************************
                          obslistpopupmenu.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun July 5 2009
    copyright            : (C) 2008 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "obslistpopupmenu.h"

#include <KGlobal>
#include <KLocale>

#include "kstars.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"

#include <config-kstars.h>

ObsListPopupMenu::ObsListPopupMenu()
        : KMenu( 0 )
{}

ObsListPopupMenu::~ObsListPopupMenu() { }

void ObsListPopupMenu::initPopupMenu( bool showAddToSession,
                                      bool showCenter,
                                      bool showDetails,
                                      bool showScope,
                                      bool showRemove,
                                      bool showLinks,
                                      bool sessionView )
{
    KStars* ks = KStars::Instance();

    clear();
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
    // Insert item for opening the eyepiece view tool
    addAction( i18n( "Eyepiece view (Beta)" ), ks->observingList(), SLOT( slotEyepieceView() ) );
    addSeparator();
    //Insert item for dowloading different images
    if( showLinks ) {
        if( ks->observingList()->currentObject() != NULL && ! ks->observingList()->currentObject()->isSolarSystem() )
        {
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
}

#include "obslistpopupmenu.moc"
