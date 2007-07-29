/***************************************************************************
                          linelist.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-24
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skypoint.h"
#include "linelist.h"

void LineList::update( KStarsData* data )
{
    if ( updateID == data->updateID() ) return;
    updateID = data->updateID();

    if ( updateNumID != data->updateNumID() ) {
        updateNumID = data->updateNumID();
        KSNumbers* num = data->updateNum();

        for (int i = 0; i < pointList.size(); i++ ) {
            pointList.at( i )->updateCoords( num );
        }
    }

    for (int i = 0; i < pointList.size(); i++ ) {
        SkyPoint* p = pointList.at( i );
        p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}
