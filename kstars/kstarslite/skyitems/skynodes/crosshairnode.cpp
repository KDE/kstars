/** *************************************************************************
                          crosshairnode.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 18/07/2016
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
#include "crosshairnode.h"
#include "Options.h"
#include "ksutils.h"
#include "nodes/ellipsenode.h"
#include "labelnode.h"
#include "../labelsitem.h"

#include <QSGFlatColorMaterial>

CrosshairNode::CrosshairNode(INDI::BaseDevice * baseDevice, RootNode * rootNode)
    :el1(new EllipseNode), el2(new EllipseNode), lines(new QSGGeometryNode), bd(baseDevice)
{
    addChildNode(el1);
    addChildNode(el2);

    QSGGeometry * linesGeo = new QSGGeometry (QSGGeometry::defaultAttributes_Point2D(),0);

    lines->setGeometry(linesGeo);
    //lines->setFlag(QSGNode::OwnsGeometry);

    material = new QSGFlatColorMaterial;

    lines->setMaterial(material);
    //lines->setFlag(QSGNode::OwnsMaterial);

    linesGeo->setDrawingMode(GL_LINES);
    linesGeo->allocate(8);

    addChildNode(lines);
    labelsItem = rootNode->labelsItem();
    label = labelsItem->addLabel(bd->getDeviceName(), LabelsItem::TELESCOPE_SYMBOL);
}

CrosshairNode::~CrosshairNode()
{
    labelsItem->deleteLabel(label);
    removeChildNode(lines);
    removeChildNode(el1);
    removeChildNode(el2);
    delete lines->geometry();
    delete material;
    delete lines;
    delete el1;
    delete el2;
}

void CrosshairNode::setColor(QColor color)
{
    if(material->color() != color)
    {
        material->setColor(color);
        lines->markDirty(QSGNode::DirtyMaterial);
    }
    el1->setColor(color);
    el2->setColor(color);
}

void CrosshairNode::update()
{
    SkyPoint indi_sp;
    KStarsData * m_KStarsData = KStarsData::Instance();

    //psky.setPen( QPen( QColor( m_KStarsData->colorScheme()->colorNamed("TargetColor" ) ) ) );
    float pxperdegree = Options::zoomFactor()/57.3;

    INumberVectorProperty * coordNP = bd->getNumber("EQUATORIAL_EOD_COORD");

    if (coordNP == nullptr)
    {
        coordNP = bd->getNumber("HORIZONTAL_COORD");
        if (coordNP == NULL)
        {
            hide();
        } else {
            INumber * np = IUFindNumber(coordNP, "AZ");

            if (np == nullptr)
                hide();
            indi_sp.setAz(np->value);

            np = IUFindNumber(coordNP, "ALT");
            if (np == nullptr)
                hide();
            indi_sp.setAlt(np->value);
            indi_sp.HorizontalToEquatorial( m_KStarsData->lst(), m_KStarsData->geo()->lat() );
        }
    }
    else
    {
        INumber * np = IUFindNumber(coordNP, "RA");

        if (np == nullptr)
            hide();
        indi_sp.setRA(np->value);

        np = IUFindNumber(coordNP, "DEC");
        if (np == nullptr)
        {
            hide();
        }
        indi_sp.setDec(np->value);
    }

    if ( Options::useAltAz() )
        indi_sp.EquatorialToHorizontal( m_KStarsData->lst(), m_KStarsData->geo()->lat() );


    if (std::isnan(indi_sp.ra().Degrees()) || std::isnan(indi_sp.dec().Degrees()))
        hide();

    QPointF P = SkyMapLite::Instance()->projector()->toScreen( &indi_sp );
    float s1 = 0.5*pxperdegree;
    float s2 = pxperdegree;
    float s3 = 2.0*pxperdegree;

    float x0 = P.x();
    float y0 = P.y();
    float x1 = x0 - 0.5*s1;
    float y1 = y0 - 0.5*s1;
    //float x2 = x0 - 0.5*s2;  float y2 = y0 - 0.5*s2;
    float x3 = x0 - 0.5*s3;
    float y3 = y0 - 0.5*s3;

    //Draw radial lines

    QSGGeometry::Point2D * vertex = lines->geometry()->vertexDataAsPoint2D();

    vertex[0].set(x1, y0);
    vertex[1].set(x3, y0);
    vertex[2].set(x0 + s2, y0);
    vertex[3].set(x0+0.5*s1, y0);
    vertex[4].set(x0, y1);
    vertex[5].set(x0, y3);
    vertex[6].set(x0, y0+0.5*s1);
    vertex[7].set(x0, y0+s2);

    lines->markDirty(QSGNode::DirtyGeometry);

    //material
    //Draw circles at 0.5 & 1 degrees*/
    el1->updateGeometry(x0,y0,s1,s1,false);
    el2->updateGeometry(x0,y0,s2,s2,false);

    label->setLabelPos(QPointF(x0+s2+2., y0));
}

void CrosshairNode::hide()
{
    SkyNode::hide();
    label->hide();
}
