/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guidelabelnode.h"

#include "Options.h"
#include "skymaplite.h"
#include "skyobject.h"

#include <QSGSimpleTextureNode>

GuideLabelNode::GuideLabelNode(QString name, LabelsItem::label_t type)
    : m_textTexture(new QSGSimpleTextureNode), m_name(name)
{
    appendChildNode(&debugRect);
    debugRect.setColor(QColor("green"));
    QColor color;
    switch (type)
    {
        case LabelsItem::label_t::CONSTEL_NAME_LABEL:
            color = KStarsData::Instance()->colorScheme()->colorNamed("CNameColor");
            break;
        case LabelsItem::label_t::HORIZON_LABEL:
            color = KStarsData::Instance()->colorScheme()->colorNamed("CompassColor");
            break;
        default:
            color = KStarsData::Instance()->colorScheme()->colorNamed("UserLabelColor");
    }

    m_textTexture->setTexture(SkyMapLite::Instance()->textToTexture(name, color));
    m_opacity->appendChildNode(m_textTexture);

    m_textSize     = m_textTexture->texture()->textureSize();
    QRectF oldRect = m_textTexture->rect();
    m_textTexture->setRect(QRect(oldRect.x(), oldRect.y(), m_textSize.width(), m_textSize.height()));
}

void GuideLabelNode::changePos(QPointF pos)
{
    // otherwise draw the label and return true
    //m_p.rotate( angle );                        //rotate the coordinate system
    //m_p.drawText( QPointF( -w2, h ), text );
    //m_p.restore();                              //reset coordinate system

    //return true;*/

    //QSizeF size = m_point->size();
    QMatrix4x4 m(1, 0, 0, pos.x(), 0, 1, 0, pos.y(), 0, 0, 1, 0, 0, 0, 0, 1);
    //m.translate(m_translatePos.x(), m_translatePos.y());
    m.rotate(m_angle, 0, 0, 1);

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}

void GuideLabelNode::setLabelPos(QPointF pos, float angle)
{
    show();
    //We need to subtract the height of texture from final y to follow the way QPainter draws the text
    m_angle        = angle;
    m_translatePos = pos;

    //QFontMetricsF fontMetrics = SkyLabeler::Instance()->fontMetrics();
    // Create bounding rectangle by rotating the (height x width) rectangle
    qreal h = m_textSize.height(); //fontMetrics.height();
    qreal w = m_textSize.width();  //fontMetrics.width( m_name );

    qreal s = sin(angle * dms::PI / 180.0);
    qreal c = cos(angle * dms::PI / 180.0);

    qreal w2 = w / 2.0;

    // These numbers really do depend on the sign of the angle like this
    if (angle >= 0.0)
    {
        top   = pos.y() - s * w2;
        bot   = pos.y() + c * h + s * w2;
        left  = pos.x() - c * w2 - s * h;
        right = pos.x() + c * w2;
    }
    else
    {
        top   = pos.y() + s * w2;
        bot   = pos.y() + c * h - s * w2;
        left  = pos.x() - c * w2;
        right = pos.x() + c * w2 - s * h;
    }

    //We need to translate matrix with the value of pos point

    labelPos = QPointF(pos.x() - w2, pos.y() + h);

    /*debugRect.setRect(QRectF(QPointF(left,top),QPointF(right,bot)));
     debugRect.markDirty(QSGNode::DirtyGeometry);*/

    // return false if label would overlap existing label
    //    if ( ! markRegion( left, right, top, bot) )
}

void GuideLabelNode::update()
{
    changePos(labelPos);
}
