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
    KStarsData* ksdata = KStarsData::Instance();

    clear();

    //Insert item for adding the object to the session view
    if( !sessionView ) {
        addAction( xi18n( "Add to session plan" ), ksdata->observingList(), SLOT( slotAddToSession() ) );
        addAction( xi18n( "Add objects visible tonight to session plan" ), ksdata->observingList(), SLOT( slotAddVisibleObj() ) );
    }

    addSeparator();

    if( !multiSelection )
        addAction( xi18n( "Center" ), ksdata->observingList(), SLOT( slotCenterObject() ) ); //Insert item for centering on object

    if( !multiSelection && showScope ) // Insert item for slewing telescope
        addAction( xi18nc( "Show the selected object in the telescope", "Scope" ), ksdata->observingList(), SLOT( slotSlewToObject() ) );

    addSeparator();


    if( !multiSelection ) {
        addAction( xi18nc( "Show Detailed Information Dialog", "Details" ), ksdata->observingList(), SLOT( slotDetails() ) ); // Insert item for showing details dialog
        addAction( xi18n( "Eyepiece view (Beta)" ), ksdata->observingList(), SLOT( slotEyepieceView() ) ); // Insert item for showing eyepiece view
    }

    //Insert item for opening the Altitude vs time dialog
    addAction( xi18n( "Altitude vs. Time" ), ksdata->observingList(), SLOT( slotAVT() ) );

    addSeparator();

    //Insert item for dowloading different images
    if( !multiSelection ) {
        if( ksdata->observingList()->currentObject() != NULL && ! ksdata->observingList()->currentObject()->isSolarSystem() )
        {
            addAction( xi18n( "Show SDSS image" ), ksdata->observingList(), SLOT( slotGetImage() ) );
            addAction( xi18n( "Show DSS image" ), ksdata->observingList(), SLOT( slotDSS() ) );
        }
        addAction( xi18n( "Show images from web " ), ksdata->observingList(), SLOT( slotSearchImage() ) );
        addSeparator();
    }

    //Insert item for Removing the object(s)
    if( !sessionView )
        addAction( xi18n("Remove from WishList"), ksdata->observingList(), SLOT( slotRemoveSelectedObjects() ) );
    else
        addAction( xi18n("Remove from Session Plan"), ksdata->observingList(), SLOT( slotRemoveSelectedObjects() ) );
}
