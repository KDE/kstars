/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyobject.h"
#include "Options.h"

#include <QSGSimpleTextureNode>
#include <QQuickWindow>

#include "skymaplite.h"
#include "labelnode.h"

LabelNode::LabelNode(SkyObject *skyObject, LabelsItem::label_t type)
    : SkyNode(skyObject), m_name(skyObject->labelString()), m_textTexture(new QSGSimpleTextureNode), m_labelType(type),
      m_fontSize(0), m_zoomFont(false)
{
    initialize();
}

LabelNode::LabelNode(QString name, LabelsItem::label_t type)
    : m_name(name), m_textTexture(new QSGSimpleTextureNode), m_labelType(type), m_fontSize(0), m_zoomFont(false)
{
    initialize();
}

LabelNode::~LabelNode()
{
    m_opacity->removeChildNode(m_textTexture);
    m_textTexture->setOwnsTexture(true);
    delete m_textTexture;
}

void LabelNode::initialize()
{
    switch (m_labelType)
    {
        case LabelsItem::label_t::PLANET_LABEL:
        case LabelsItem::label_t::SATURN_MOON_LABEL:
        case LabelsItem::label_t::JUPITER_MOON_LABEL:
        case LabelsItem::label_t::COMET_LABEL:
        case LabelsItem::label_t::RUDE_LABEL:
        case LabelsItem::label_t::ASTEROID_LABEL:
            m_schemeColor = "PNameColor";
            m_zoomFont    = true;
            break;
        case LabelsItem::label_t::DEEP_SKY_LABEL:
        case LabelsItem::label_t::DSO_OTHER_LABEL:
        case LabelsItem::label_t::CATALOG_DSO_LABEL:
            m_schemeColor = "DSNameColor";
            m_zoomFont    = true;
            break;
        case LabelsItem::label_t::TELESCOPE_SYMBOL:
            m_schemeColor = "TargetColor";
            m_zoomFont    = true;
            break;
        case LabelsItem::label_t::CONSTEL_NAME_LABEL:
            m_schemeColor = "CNameColor";
            break;
        case LabelsItem::label_t::STAR_LABEL:
        case LabelsItem::label_t::CATALOG_STAR_LABEL:
            m_schemeColor = "SNameColor";
            break;
        default:
            m_schemeColor = "UserLabelColor";
            break;
    }

    createTexture(KStarsData::Instance()->colorScheme()->colorNamed(m_schemeColor));
    m_opacity->appendChildNode(m_textTexture);
}

void LabelNode::createTexture(QColor color)
{
    switch (m_labelType)
    {
        case LabelsItem::label_t::SATURN_MOON_LABEL:
        case LabelsItem::label_t::JUPITER_MOON_LABEL:
            SkyLabeler::Instance()->shrinkFont(2);
            break;
        default:
            break;
    }

    QSGTexture *oldTexture = m_textTexture->texture();

    m_textTexture->setTexture(SkyMapLite::Instance()->textToTexture(m_name, color, m_zoomFont));
    m_color = color;

    if (m_zoomFont)
        m_fontSize = SkyLabeler::Instance()->drawFont().pointSize();

    switch (m_labelType)
    {
        case LabelsItem::label_t::SATURN_MOON_LABEL:
        case LabelsItem::label_t::JUPITER_MOON_LABEL:
            SkyLabeler::Instance()->resetFont();
            break;
        default:
            break;
    }

    if (oldTexture)
        delete oldTexture;

    m_textSize     = m_textTexture->texture()->textureSize();
    QRectF oldRect = m_textTexture->rect();
    qreal ratio    = SkyMapLite::Instance()->window()->effectiveDevicePixelRatio();

    m_textTexture->setRect(QRect(oldRect.x(), oldRect.y(), m_textSize.width() / ratio, m_textSize.height() / ratio));
}

void LabelNode::changePos(QPointF pos)
{
    QMatrix4x4 m(1, 0, 0, pos.x(), 0, 1, 0, pos.y(), 0, 0, 1, 0, 0, 0, 0, 1);
    //m.translate(-0.5*size.width(), -0.5*size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}

void LabelNode::setLabelPos(QPointF pos)
{
    show();
    qreal ratio = SkyMapLite::Instance()->window()->effectiveDevicePixelRatio();
    //We need to subtract the height of texture from final y to follow the way QPainter draws the text
    if (m_skyObject)
        labelPos = QPointF(pos.x() + m_skyObject->labelOffset(),
                           pos.y() + m_skyObject->labelOffset() - m_textSize.height() / ratio);
    else
        labelPos = QPointF(pos.x(), pos.y());
}

void LabelNode::update()
{
    QColor newColor = KStarsData::Instance()->colorScheme()->colorNamed(m_schemeColor);
    if ((m_zoomFont && m_fontSize != SkyLabeler::Instance()->skyFont().pointSize()) || m_color != newColor)
    {
        createTexture(newColor);
    }
    changePos(labelPos);
}
