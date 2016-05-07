/** *************************************************************************
                          PlanetsItem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 02/05/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "planetsitem.h"
#include "projections/projector.h"
#include "solarsystemsinglecomponent.h"
#include "ksplanet.h"

#include <QSGSimpleTextureNode>
#include <QQuickWindow>
#include "nodes/planetnode.h"
#include "nodes/planetitemnode.h"

#include "Options.h"


#include <QSGSimpleRectNode>

PlanetsItem::PlanetsItem(QQuickItem* parent)
    :SkyItem(parent)
{
    const SkyMapLite* skyMapLite = map();
    connect(skyMapLite, &SkyMapLite::zoomChanged, this, &QQuickItem::update);
}

void PlanetsItem::addPlanet(SolarSystemSingleComponent* planetComp) {
    if(!m_planetComponents.contains(planetComp) && !m_toAdd.contains(planetComp))
        m_toAdd.append(planetComp);
}

QSGNode* PlanetsItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) {
    PlanetItemNode *n = static_cast<PlanetItemNode*>(oldNode);
    QRectF rect = boundingRect();

    if (rect.isEmpty()) {
        delete n;
        return 0;
    }

    if(!n) {
        n = new PlanetItemNode; // If no PlanetItemNode exists create one
        int pCompLen = m_planetComponents.length();
        if(pCompLen > 0) {
            /* If there are some planets that have been already displayed once then recreate them
                 in new instance of PlanetItemNode*/
            for(int i = 0; i < pCompLen; ++i) {
                n->appendChildNode(new PlanetNode(m_planetComponents[i]->planet(), n));
            }
        }
    }

    int addLength = m_toAdd.length();
    if(addLength > 0) { // If there are some new planets to add
        for(int i = 0; i < addLength; ++i) {
            m_planetComponents.append(m_toAdd[i]);
            n->appendChildNode(new PlanetNode(m_toAdd[i]->planet(), n));
        }
        m_toAdd.clear();
    }

    //Traverse all children nodes of PlanetItemNode
    for(int i = 0; i < n->childCount(); ++i) {
        PlanetNode* pNode = static_cast<PlanetNode*>(n->childAtIndex(i));
        KSPlanetBase* planet = pNode->planet();

        if( !projector()->checkVisibility(planet) ) {
            pNode->hide();
            continue;
        }

        bool visible = false;
        QPointF pos = projector()->toScreen(planet,true,&visible);
        if( !visible || !projector()->onScreen(pos) ) {
            pNode->hide();
            continue;
        }

        float fakeStarSize = ( 10.0 + log10( Options::zoomFactor() ) - log10( MINZOOM ) ) * ( 10 - planet->mag() ) / 10;
        if( fakeStarSize > 15.0 )
            fakeStarSize = 15.0;

        float size = planet->angSize() * dms::PI * Options::zoomFactor()/10800.0;
        if( size < fakeStarSize && planet->name() != "Sun" && planet->name() != "Moon" ) {
            pNode->setPointSize(fakeStarSize);
            pNode->changePos(pos);
            pNode->showPoint();
        } else {
            float sizemin = 1.0;
            if( planet->name() == "Sun" || planet->name() == "Moon" )
                sizemin = 8.0;

            float size = planet->angSize() * dms::PI * Options::zoomFactor()/10800.0;
            if( size < sizemin )
                size = sizemin;

            if( Options::showPlanetImages() && !planet->image().isNull() ) {
                //Because Saturn has rings, we inflate its image size by a factor 2.5
                if( planet->name() == "Saturn" )
                    size = int(2.5*size);
                // Scale size exponentially so it is visible at large zooms
                else if (planet->name() == "Pluto")
                    size = int(size*exp(1.5*size));

                /*save();
                translate(pos);
                rotate( projector()->findPA( planet, pos.x(), pos.y() ) );
                drawImage( QRect(-0.5*size, -0.5*size, size, size),
                           planet->image() );
                restore();*/
                pNode->setPlanetPicSize(size);
                pNode->changePos(pos);
                pNode->showPlanetPic();
            } else { //Otherwise, draw a simple circle.
                //drawEllipse( pos, size, size );
            }
        }
    }
    return n;
}
