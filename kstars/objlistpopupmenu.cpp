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
//#include "widgets/ksobjectlist.h"
#include "skyobjects/skyobject.h"

#include <config-kstars.h>

class KSObjectList;

ObjListPopupMenu::ObjListPopupMenu(KSObjectList *parent)
        : KMenu( 0 )
{
    m_KSObjList = parent;
}

ObjListPopupMenu::~ObjListPopupMenu() { }

void ObjListPopupMenu::init()
{
    ks = KStars::Instance();
    clear();
}

void ObjListPopupMenu::showAddToSession()
{
    addAction( i18n( "Add to session plan" ), m_KSObjList, SLOT( slotAddToSession() ) );
}


void ObjListPopupMenu::showCenter()
{
    addAction( i18n( "Center" ), m_KSObjList, SLOT( slotCenterObject() ) );
}

void ObjListPopupMenu::showScope()
{
    addAction( i18nc( "Show the selected object in the telescope", "Scope" ), m_KSObjList, SLOT( slotSlewToObject() ) );
}

void ObjListPopupMenu::showDetails()
{
    addAction( i18nc( "Show Detailed Information Dialog", "Details" ), m_KSObjList, SLOT( slotDetails() ) );
}

void ObjListPopupMenu::showAVT()
{
    addAction( i18n( "Altitude vs. Time" ), m_KSObjList, SLOT( slotAVT() ) );
}

/* 
 * The following actions are specific for ObservingList dialog. They have slots connected directly to
 * the ObservingList instance from KStars. These should not be used independently outside of the ObservingList dialog.
 */
void ObjListPopupMenu::showAddVisibleTonight()
{
    addAction( i18n( "Add objects visible tonight to session plan" ), ks->observingList(), SLOT( slotAddVisibleObj() ) );
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

/* End of ObservingList specific actions */

#include "objlistpopupmenu.moc"
