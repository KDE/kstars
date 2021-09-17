/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "skyobject.h"
#include "Options.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include "constellationartnode.h"
#include "constellationsart.h"
#include "nodes/pointnode.h"

#include "../rootnode.h"
#include "../labelsitem.h"
#include "labelnode.h"

ConstellationArtNode::ConstellationArtNode(ConstellationsArt *obj)
    : SkyNode(obj), m_art(obj), m_texture(new QSGSimpleTextureNode)
{
    m_texture->setTexture(
        SkyMapLite::Instance()->window()->createTextureFromImage(m_art->image(), QQuickWindow::TextureCanUseAtlas));
    m_texture->setFiltering(QSGTexture::Linear);
    m_opacity->appendChildNode(m_texture);
    update();
}

void ConstellationArtNode::update()
{
    double zoom = Options::zoomFactor();

    const Projector *m_proj = SkyMapLite::Instance()->projector();
    if (!m_proj->checkVisibility(m_art))
    {
        hide();
        return;
    }

    bool visible = false;
    m_art->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    QPointF constellationmidpoint = m_proj->toScreen(m_art, true, &visible);

    if (!visible || !m_proj->onScreen(constellationmidpoint))
    {
        hide();
        return;
    }

    m_opacity->setOpacity(0.7);

    //qDebug() << "o->pa() " << obj->pa();
    float positionangle = m_proj->findPA(m_art, constellationmidpoint.x(), constellationmidpoint.y());
    //qDebug() << " final PA " << positionangle;

    float w = m_art->getWidth() * 60 * dms::PI * zoom / 10800;
    float h = m_art->getHeight() * 60 * dms::PI * zoom / 10800;

    m_texture->setRect(0, 0, w, h);

    changePos(constellationmidpoint, positionangle);
}

void ConstellationArtNode::changePos(QPointF pos, double positionangle)
{
    QSizeF size = m_texture->rect().size();
    QMatrix4x4 m(1, 0, 0, pos.x(), 0, 1, 0, pos.y(), 0, 0, 1, 0, 0, 0, 0, 1);
    m.rotate(positionangle, 0, 0, 1);
    m.translate(-0.5 * size.width(), -0.5 * size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}

void ConstellationArtNode::hide()
{
    //m_point->hide();
    SkyNode::hide();
}
