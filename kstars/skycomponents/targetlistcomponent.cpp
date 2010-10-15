/***************************************************************************
              targetlistcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Oct 14 2010 9:59 PM CDT
    copyright            : (C) 2010 Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "targetlistcomponent.h"
#include "skymap.h"
#include "Options.h"

TargetListComponent::TargetListComponent( SkyComposite *parent ) : SkyComponent( parent ) {
    list = 0;
    drawSymbols = 0;
    drawLabels = 0;
}

TargetListComponent::TargetListComponent( SkyComposite *parent, SkyObjectList *objectList, QPen _pen, 
                                          bool (*optionDrawSymbols)(void), bool (*optionDrawLabels)(void) ) 
    : SkyComponent( parent ), pen( _pen ) {
    
    list = objectList;
    drawSymbols = optionDrawSymbols;
    drawLabels = optionDrawLabels;
}

void TargetListComponent::drawTargetSymbol( QPainter &psky, SkyObject *obj ) {
    SkyMap *map = SkyMap::Instance(); // FIXME: Replace with projector upon merger of Harry's code
    if ( ! map->checkVisibility( obj ) )
        return;
    QPointF o = map->toScreen( obj ); // FIXME: Replace with appropriate call on the projector upon merger of Harry's code
    if( !drawSymbols || (*drawSymbols)() ) {
        if (o.x() >= 0. && o.x() <= map->width()*map->scale() && o.y() >=0. && o.y() <= map->height()*map->scale() ) {
            if ( Options::useAntialias() ) {
                float size = 20.*map->scale();
                float x1 = o.x() - 0.5*size;
                float y1 = o.y() - 0.5*size;
                psky.drawArc( QRectF(x1, y1, size, size), -60*16, 120*16 );
                psky.drawArc( QRectF(x1, y1, size, size), 120*16, 120*16 );
            } else {
                int size = 20*int(map->scale());
                int x1 = int(o.x()) - size/2;
                int y1 = int(o.y()) - size/2;
                psky.drawArc( QRect(x1, y1, size, size), -60*16, 120*16 );
                psky.drawArc( QRect(x1, y1, size, size), 120*16, 120*16 );
            }
            if ( drawLabels && (*drawLabels)() )
                obj->drawRudeNameLabel( psky, o );
        }
    }
}

void TargetListComponent::draw( QPainter &psky ) {
    psky.setPen( pen );
    if( drawSymbols && !(*drawSymbols)() )
        return;
    if( !list || list->count() <= 0 )
        return;
    foreach( SkyObject *obj, *list ) {
        drawTargetSymbol( psky, obj );
    }
}
