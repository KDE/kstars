/** *************************************************************************
                          planetnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 05/05/2016
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

#include <QSGSimpleTextureNode>
#include <QImage>

#include <QQuickWindow>
#include "skymaplite.h"
#include "ksplanetbase.h"
#include "Options.h"
#include "projections/projector.h"
#include "planetnode.h"

PlanetNode::PlanetNode(KSPlanetBase* pb, RootNode* parentNode)
    :SkyNode(pb), m_planetPic(new QSGSimpleTextureNode), m_planetOpacity(new QSGOpacityNode)
{
    // Draw them as bright stars of appropriate color instead of images
    char spType;
    //FIXME: do these need i18n?
    if( pb->name() == i18n("Mars") ) {
        spType = 'K';
    } else if( pb->name() == i18n("Jupiter") || pb->name() == i18n("Mercury") || pb->name() == i18n("Saturn") ) {
        spType = 'F';
    } else {
        spType = 'B';
    }

    m_point = new PointNode(parentNode, spType);
    appendChildNode(m_point);

    appendChildNode(m_planetOpacity);
    //Add planet to opacity node so that we could hide the planet
    m_planetOpacity->appendChildNode(m_planetPic);
    m_planetPic->setTexture(SkyMapLite::Instance()->window()->createTextureFromImage(
                                pb->image(), QQuickWindow::TextureCanUseAtlas));
}

void PlanetNode::update() {
    if(0) {
        //if(!pNode->planet()->select) {
          //  pNode->hide(); //TODO
    } else {
        KSPlanetBase * planet = static_cast<KSPlanetBase *>(m_skyObject);
        const Projector * proj = projector();

        if( !proj->checkVisibility(planet) ) {
            hide();
            return;
        }

        bool visible = false;
        QPointF pos = proj->toScreen(planet,true,&visible);
        if( !visible || !proj->onScreen(pos) ) {
            hide();
            return;
        }
        float fakeStarSize = ( 10.0 + log10( Options::zoomFactor() ) - log10( MINZOOM ) ) * ( 10 - planet->mag() ) / 10;
        if( fakeStarSize > 15.0 )
            fakeStarSize = 15.0;

        float size = planet->angSize() * dms::PI * Options::zoomFactor()/10800.0;
        if( size < fakeStarSize && planet->name() != "Sun" && planet->name() != "Moon" ) {
            setPointSize(fakeStarSize);
            changePos(pos);
            showPoint();
        } else {
            float sizemin = 1.0;
            if( planet->name() == "Sun" || planet->name() == "Moon" )
                sizemin = 8.0;

            float size = planet->angSize() * dms::PI * Options::zoomFactor()/10800.0;
            if( size < sizemin )
                size = sizemin;
            //Options::showPlanetImages() &&
            if( !planet->image().isNull() ) {
                //Because Saturn has rings, we inflate its image size by a factor 2.5
                if( planet->name() == "Saturn" )
                    size = int(2.5*size);
                // Scale size exponentially so it is visible at large zooms
                else if (planet->name() == "Pluto")
                    size = int(size*exp(1.5*size));

                setPlanetPicSize(size);
                changePos(pos);
                showPlanetPic();
            } else { //Otherwise, draw a simple circle.
                //drawEllipse( pos, size, size );
            }
        }
    }
}

void PlanetNode::setPointSize(float size) {
    m_point->setSize(size);
}

void PlanetNode::setPlanetPicSize(float size) {
    m_planetPic->setRect(QRect(0,0,size,size));
    markDirty(QSGNode::DirtyGeometry);
}

void PlanetNode::showPoint() {
    if(m_planetOpacity->opacity()) {
        m_planetOpacity->setOpacity(0);
        m_planetOpacity->markDirty(QSGNode::DirtyOpacity);
    }
    m_point->show();
}

void PlanetNode::showPlanetPic() {
    if(!m_planetOpacity->opacity()) {
        m_planetOpacity->setOpacity(1);
        m_planetOpacity->markDirty(QSGNode::DirtyOpacity);
    }
    m_point->hide();
}

void PlanetNode::hide() {
    if(m_planetOpacity->opacity()) {
        m_planetOpacity->setOpacity(0);
        m_planetOpacity->markDirty(QSGNode::DirtyOpacity);
    }
    m_point->hide();
}

void PlanetNode::changePos(QPointF pos) {
    QSizeF size;
    QMatrix4x4 m (1,0,0,pos.x(),
                  0,1,0,pos.y(),
                  0,0,1,0,
                  0,0,0,1);

    if(m_planetOpacity->opacity()) {
        size = m_planetPic->rect().size();
        //Matrix has to be rotated between assigning x and y and translating it by the half
        //of size of the planet. Otherwise the image will don't rotate at all or rotate around
        //the top-left corner
        m.rotate(projector()->findPA( m_skyObject, pos.x(), pos.y()), 0, 0, 1);
    } else {
        size = m_point->size();
    }

    m.translate(-0.5*size.width(), -0.5*size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}
