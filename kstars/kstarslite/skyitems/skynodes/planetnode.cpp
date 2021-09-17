/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSGSimpleTextureNode>
#include <QImage>

#include <QQuickWindow>
#include "skymaplite.h"
#include "ksplanetbase.h"
#include "Options.h"
#include "projections/projector.h"
#include "../rootnode.h"

#include "planetnode.h"
#include "nodes/pointnode.h"
#include "labelnode.h"

PlanetNode::PlanetNode(KSPlanetBase *pb, RootNode *parentNode, LabelsItem::label_t labelType)
    : SkyNode(pb), m_planetPic(new QSGSimpleTextureNode), m_planetOpacity(new SkyOpacityNode)
{
    // Draw them as bright stars of appropriate color instead of images
    char spType;
    //FIXME: do these need i18n?
    if (pb->name() == i18n("Mars"))
    {
        spType = 'K';
    }
    else if (pb->name() == i18n("Jupiter") || pb->name() == i18n("Mercury") || pb->name() == i18n("Saturn"))
    {
        spType = 'F';
    }
    else
    {
        spType = 'B';
    }

    m_point = new PointNode(parentNode, spType);
    appendChildNode(m_point);
    appendChildNode(m_planetOpacity);

    //Add planet to opacity node so that we could hide the planet
    m_planetOpacity->appendChildNode(m_planetPic);
    m_planetPic->setTexture(
        SkyMapLite::Instance()->window()->createTextureFromImage(pb->image(), QQuickWindow::TextureCanUseAtlas));
    m_planetPic->setOwnsTexture(true);
    m_label = parentNode->labelsItem()->addLabel(pb, labelType);
}

void PlanetNode::update()
{
    KSPlanetBase *planet  = static_cast<KSPlanetBase *>(skyObject());
    const Projector *proj = projector();

    if (!proj->checkVisibility(planet))
    {
        hide();
        return;
    }

    bool visible = false;
    QPointF pos  = proj->toScreen(planet, true, &visible);

    if (!visible || !proj->onScreen(pos))
    {
        hide();
        return;
    }

    //Set new position of the label
    m_label->setLabelPos(pos);

    float fakeStarSize = (10.0 + log10(Options::zoomFactor()) - log10(MINZOOM)) * (10 - planet->mag()) / 10;
    if (fakeStarSize > 15.0)
        fakeStarSize = 15.0;

    float size = planet->angSize() * dms::PI * Options::zoomFactor() / 10800.0;
    if (size < fakeStarSize && planet->name() != "Sun" && planet->name() != "Moon")
    {
        setPointSize(fakeStarSize);
        changePos(pos);
        showPoint();
    }
    else
    {
        float sizemin = 1.0;
        if (planet->name() == "Sun" || planet->name() == "Moon")
            sizemin = 8.0;

        float size = planet->angSize() * dms::PI * Options::zoomFactor() / 10800.0;
        if (size < sizemin)
            size = sizemin;
        //Options::showPlanetImages() &&
        if (!planet->image().isNull())
        {
            //Because Saturn has rings, we inflate its image size by a factor 2.5
            if (planet->name() == "Saturn")
                size = int(2.5 * size);
            // Scale size exponentially so it is visible at large zooms
            else if (planet->name() == "Pluto")
                size = int(size * exp(1.5 * size));

            setPlanetPicSize(size);
            showPlanetPic();
            changePos(pos);
        }
        else //Otherwise, draw a simple circle. Do we need it?
        {
            //drawEllipse( pos, size, size );
        }
    }
}

void PlanetNode::setPointSize(float size)
{
    m_point->setSize(size);
}

void PlanetNode::setPlanetPicSize(float size)
{
    m_planetPic->setRect(QRect(0, 0, size, size));
    markDirty(QSGNode::DirtyGeometry);
}

void PlanetNode::showPoint()
{
    m_planetOpacity->hide();
    m_point->show();
}

void PlanetNode::showPlanetPic()
{
    m_planetOpacity->show();
    m_point->hide();
}

void PlanetNode::hide()
{
    m_planetOpacity->hide();
    m_point->hide();
    m_label->hide();
}

void PlanetNode::changePos(QPointF pos)
{
    QSizeF size;
    //Check the bug with planet
    QMatrix4x4 m(1, 0, 0, pos.x(), 0, 1, 0, pos.y(), 0, 0, 1, 0, 0, 0, 0, 1);

    if (m_planetOpacity->visible())
    {
        size = m_planetPic->rect().size();
        //Matrix has to be rotated between assigning x and y and translating it by the half
        //of size of the planet. Otherwise the image will don't rotate at all or rotate around
        //the top-left corner
        m.rotate(projector()->findPA(skyObject(), pos.x(), pos.y()), 0, 0, 1);
    }
    else
    {
        size = m_point->size();
    }

    m.translate(-0.5 * size.width(), -0.5 * size.height());

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}
