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

#include <KLocalizedString>

#include "kstars.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"

#include <config-kstars.h>

ObsListPopupMenu::ObsListPopupMenu()
        : QMenu( 0 )
{}

ObsListPopupMenu::~ObsListPopupMenu() { }

void ObsListPopupMenu::initPopupMenu( bool sessionView, bool multiSelection, bool showScope )
{
    KStars* ks = KStars::Instance();

    clear();

    //Insert item for adding the object to the session view
    if( !sessionView ) {
        addAction( xi18n( "Add to session plan" ), ks->observingList(), SLOT( slotAddToSession() ) );
        addAction( xi18n( "Add objects visible tonight to session plan" ), ks->observingList(), SLOT( slotAddVisibleObj() ) );
    }

    addSeparator();

    if( !multiSelection )
        addAction( xi18n( "Center" ), ks->observingList(), SLOT( slotCenterObject() ) ); //Insert item for centering on object

    if( !multiSelection && showScope ) // Insert item for slewing telescope
        addAction( xi18nc( "Show the selected object in the telescope", "Scope" ), ks->observingList(), SLOT( slotSlewToObject() ) );

    addSeparator();


    if( !multiSelection ) {
        addAction( xi18nc( "Show Detailed Information Dialog", "Details" ), ks->observingList(), SLOT( slotDetails() ) ); // Insert item for showing details dialog
        addAction( xi18n( "Eyepiece view (Beta)" ), ks->observingList(), SLOT( slotEyepieceView() ) ); // Insert item for showing eyepiece view
    }

    //Insert item for opening the Altitude vs time dialog
    addAction( xi18n( "Altitude vs. Time" ), ks->observingList(), SLOT( slotAVT() ) );

    addSeparator();

    //Insert item for dowloading different images
    if( !multiSelection ) {
        if( ks->observingList()->currentObject() != NULL && ! ks->observingList()->currentObject()->isSolarSystem() )
        {
            addAction( xi18n( "Show SDSS image" ), ks->observingList(), SLOT( slotGetImage() ) );
            addAction( xi18n( "Show DSS image" ), ks->observingList(), SLOT( slotDSS() ) );
        }
        addAction( xi18n( "Show images from web " ), ks->observingList(), SLOT( slotGoogleImage() ) );
        addSeparator();
    }

    //Insert item for Removing the object(s)
    if( !sessionView )
        addAction( xi18n("Remove from WishList"), ks->observingList(), SLOT( slotRemoveSelectedObjects() ) );
    else
        addAction( xi18n("Remove from Session Plan"), ks->observingList(), SLOT( slotRemoveSelectedObjects() ) );
}
