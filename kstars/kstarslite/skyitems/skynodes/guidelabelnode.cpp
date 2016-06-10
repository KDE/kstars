/** *************************************************************************
                          pointsourcenode.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 16/06/2016
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
#include "skyobject.h"
#include "Options.h"

#include <QSGSimpleTextureNode>

#include "skymaplite.h"
#include "guidelabelnode.h"

GuideLabelNode::GuideLabelNode(SkyObject * skyObject, LabelsItem::label_t type)
    :SkyNode(skyObject), m_textTexture(new QSGSimpleTextureNode)
{
    QColor color;
    switch(type) {
        case LabelsItem::label_t::CONSTEL_NAME_LABEL:
            color = KStarsData::Instance()->colorScheme()->colorNamed( "CNameColor" );
            break;
        default:
            color = KStarsData::Instance()->colorScheme()->colorNamed( "UserLabelColor" );
    }

    m_textTexture->setTexture(SkyMapLite::Instance()->textToTexture(skyObject->name(), color));
    m_opacity->appendChildNode(m_textTexture);

    m_textSize = m_textTexture->texture()->textureSize();
    QRectF oldRect = m_textTexture->rect();
    m_textTexture->setRect(QRect(oldRect.x(),oldRect.y(),m_textSize.width(),m_textSize.height()));
}

void GuideLabelNode::changePos(QPointF pos) {

   /* QFontMetricsF fontMetrics = SkyLabeler::Instance()->fontMetrics();
    // Create bounding rectangle by rotating the (height x width) rectangle
    qreal h = fontMetrics.height();
    qreal w = fontMetrics.width( text );

    float angle = 0.0;
    qreal s = 0;//sin( angle * dms::PI / 180.0 );
    qreal c = 1;//cos( angle * dms::PI / 180.0 );

    qreal w2 = w / 2.0;

    qreal top, bot, left, right;

    // These numbers really do depend on the sign of the angle like this
    if ( angle >= 0.0 ) {
        top   = o.y()           - s * w2;
        bot   = o.y() + c *  h  + s * w2;
        left  = o.x() - c * w2  - s * h;
        right = o.x() + c * w2;
    }
    else {
        top   = o.y()           + s * w2;
        bot   = o.y() + c *  h  - s * w2;
        left  = o.x() - c * w2;
        right = o.x() + c * w2  - s * h;
    }

    // return false if label would overlap existing label
    if ( ! markRegion( left, right, top, bot) )
        return false;

    // for debugging the bounding rectangle:
    //psky.drawLine( QPointF( left,  top ), QPointF( right, top ) );
    //psky.drawLine( QPointF( right, top ), QPointF( right, bot ) );
    //psky.drawLine( QPointF( right, bot ), QPointF( left,  bot ) );
    //psky.drawLine( QPointF( left,  bot ), QPointF( left,  top ) );

    // otherwise draw the label and return true
    m_p.save();
    m_p.translate( o );

    m_p.rotate( angle );                        //rotate the coordinate system
    m_p.drawText( QPointF( -w2, h ), text );
    m_p.restore();                              //reset coordinate system

    return true;

    //QSizeF size = m_point->size();
    QMatrix4x4 m (1,0,0,pos.x(),
                  0,1,0,pos.y(),
                  0,0,1,0,
                  0,0,0,1);
    //m.translate(-0.5*size.width(), -0.5*size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);*/
}

void GuideLabelNode::setLabelPos(QPointF pos) {
    show();
    //We need to subtract the height of texture from final y to follow the way QPainter draws the text
    labelPos = QPointF(pos.x() + m_skyObject->labelOffset(), pos.y() + m_skyObject->labelOffset() - m_textSize.height());
}

void GuideLabelNode::update() {
    changePos(labelPos);
}
