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
#include "labelnode.h"

LabelNode::LabelNode(SkyObject * skyObject, LabelsItem::label_t type)
    :SkyNode(skyObject), m_textTexture(new QSGSimpleTextureNode)
{
    QColor color;
    switch(type) {
        case LabelsItem::label_t::PLANET_LABEL:
        case LabelsItem::label_t::SATURN_MOON_LABEL:
        case LabelsItem::label_t::JUPITER_MOON_LABEL:
        case LabelsItem::label_t::COMET_LABEL:
        case LabelsItem::label_t::RUDE_LABEL:
            color = KStarsData::Instance()->colorScheme()->colorNamed( "PNameColor" );
            break;
        case LabelsItem::label_t::ASTEROID_LABEL:
            color = QColor("gray");
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

void LabelNode::changePos(QPointF pos) {
    //QSizeF size = m_point->size();
    QMatrix4x4 m (1,0,0,pos.x(),
                  0,1,0,pos.y(),
                  0,0,1,0,
                  0,0,0,1);
    //m.translate(-0.5*size.width(), -0.5*size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}

void LabelNode::setLabelPos(QPointF pos) {
    show();
    //We need to subtract the height of texture from final y to follow the way QPainter draws the text
    labelPos = QPointF(pos.x() + m_skyObject->labelOffset(), pos.y() + m_skyObject->labelOffset() - m_textSize.height());
}

void LabelNode::update() {
    changePos(labelPos);
}
