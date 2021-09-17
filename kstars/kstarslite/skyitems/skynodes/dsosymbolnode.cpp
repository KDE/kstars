/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dsosymbolnode.h"

#include "deepskyobject.h"
#include "Options.h"
#include "nodes/ellipsenode.h"

#include <QSGFlatColorMaterial>

void SymbolNode::updateSymbol(float x, float y, float e, float size)
{
    zoom  = Options::zoomFactor();
    x     = 0;
    y     = 0;
    isize = int(size);

    dx1 = -0.5 * size;
    dx2 = 0.5 * size;
    dy1 = -1.0 * e * size / 2.;
    dy2 = e * size / 2.;
    x1  = x + dx1;
    x2  = x + dx2;
    y1  = y + dy1;
    y2  = y + dy2;

    dxa = -size / 4.;
    dxb = size / 4.;
    dya = -1.0 * e * size / 4.;
    dyb = e * size / 4.;
    xa  = x + dxa;
    xb  = x + dxb;
    ya  = y + dya;
    yb  = y + dyb;
}

StarSymbol::StarSymbol(const QColor &color) : m_ellipse(new EllipseNode(color))
{
    appendChildNode(m_ellipse);
}

void StarSymbol::updateSymbol(float x, float y, float e, float size)
{
    SymbolNode::updateSymbol(x, y, e, size);
    if (size < 2.)
        size = 2.;
    if (Options::useAntialias())
        m_ellipse->updateGeometry(0, 0, size, e * size, false);
    else
        m_ellipse->updateGeometry(0, 0, int(size / 2), int(size / 2), false);
}

AsterismSymbol::AsterismSymbol(const QColor &color)
{
    e1 = new EllipseNode(color);
    e2 = new EllipseNode(color);
    e3 = new EllipseNode(color);
    e4 = new EllipseNode(color);
    e5 = new EllipseNode(color);
    e6 = new EllipseNode(color);
    e7 = new EllipseNode(color);
    e8 = new EllipseNode(color);
    appendChildNode(e1);
    appendChildNode(e2);
    appendChildNode(e3);
    appendChildNode(e4);
    appendChildNode(e5);
    appendChildNode(e6);
    appendChildNode(e7);
    appendChildNode(e8);
}

void AsterismSymbol::updateSymbol(float x, float y, float e, float size)
{
    SymbolNode::updateSymbol(x, y, e, size);
    psize = 2.;
    if (size > 50.)
        psize *= 2.;
    if (size > 100.)
        psize *= 2.;

    e1->updateGeometry(xa, y1, psize, psize, true);
    e2->updateGeometry(xb, y1, psize, psize, true);
    e3->updateGeometry(xa, y2, psize, psize, true);
    e4->updateGeometry(xb, y2, psize, psize, true);
    e5->updateGeometry(x1, ya, psize, psize, true);
    e6->updateGeometry(x1, yb, psize, psize, true);
    e7->updateGeometry(x2, ya, psize, psize, true);
    e8->updateGeometry(x2, yb, psize, psize, true);
}

GlobularClusterSymbol::GlobularClusterSymbol(const QColor &color) : e1(new EllipseNode(color))
{
    appendChildNode(e1);

    lines = new QSGGeometryNode;
    appendChildNode(lines);

    QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    geometry->setDrawingMode(GL_LINES);
    geometry->allocate(4);

    lines->setGeometry(geometry);
    lines->setFlag(QSGNode::OwnsGeometry);

    QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
    material->setColor(color);
    lines->setOpaqueMaterial(material);
    lines->setFlag(QSGNode::OwnsMaterial);
}

void GlobularClusterSymbol::updateSymbol(float x, float y, float e, float size)
{
    SymbolNode::updateSymbol(x, y, e, size);

    if (size < 2.)
        size = 2.;
    e1->updateGeometry(0, 0, size, e * size, false);

    QSGGeometry::Point2D *vertex = lines->geometry()->vertexDataAsPoint2D();
    //First line
    vertex[0].set(dx1, 0);
    vertex[1].set(dx2, 0);

    //Second line
    vertex[2].set(0, dy1);
    vertex[3].set(0, dy2);

    lines->markDirty(QSGNode::DirtyGeometry);
}

DarkNebulaSymbol::DarkNebulaSymbol(const QColor &color)
{
    lines = new QSGGeometryNode;
    appendChildNode(lines);

    QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    geometry->setDrawingMode(GL_LINES);
    geometry->allocate(8);

    lines->setGeometry(geometry);
    lines->setFlag(QSGNode::OwnsGeometry);

    QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
    material->setColor(color);
    lines->setOpaqueMaterial(material);
    lines->setFlag(QSGNode::OwnsMaterial);
}

void DarkNebulaSymbol::updateSymbol(float x, float y, float e, float size)
{
    SymbolNode::updateSymbol(x, y, e, size);

    QSGGeometry::Point2D *vertex = lines->geometry()->vertexDataAsPoint2D();
    //First line
    vertex[0].set(dx1, dy1);
    vertex[1].set(dx2, dy1);

    //Second line
    vertex[2].set(dx2, dy1);
    vertex[3].set(dx2, dy2);

    //Third line
    vertex[4].set(dx2, dy2);
    vertex[5].set(dx1, dy2);

    //Fourth line
    vertex[6].set(dx1, dy2);
    vertex[7].set(dx1, dy1);

    lines->markDirty(QSGNode::DirtyGeometry);
}

PlanetaryNebulaSymbol::PlanetaryNebulaSymbol(const QColor &color)
{
    e1 = new EllipseNode(color);
    appendChildNode(e1);

    lines = new QSGGeometryNode;
    appendChildNode(lines);

    QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    geometry->setDrawingMode(GL_LINES);
    geometry->allocate(8);

    lines->setGeometry(geometry);
    lines->setFlag(QSGNode::OwnsGeometry);

    QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
    material->setColor(color);
    lines->setOpaqueMaterial(material);
    lines->setFlag(QSGNode::OwnsMaterial);
}

void PlanetaryNebulaSymbol::updateSymbol(float x, float y, float e, float size)
{
    SymbolNode::updateSymbol(x, y, e, size);

    if (size < 2.)
        size = 2.;

    e1->updateGeometry(0, 0, size, e * size, false);

    QSGGeometry::Point2D *vertex = lines->geometry()->vertexDataAsPoint2D();
    //First line
    vertex[0].set(0., dy1);
    vertex[1].set(0., dy1 - e * size / 2.);

    //Second line
    vertex[2].set(0., dy2);
    vertex[3].set(0., dy2 + e * size / 2.);

    //Third line
    vertex[4].set(dx1, 0.);
    vertex[5].set(dx1 - size / 2., 0.);

    //Fourth line
    vertex[6].set(dx2, 0.);
    vertex[7].set(dx2 + size / 2., 0.);

    lines->markDirty(QSGNode::DirtyGeometry);
}

SupernovaRemnantSymbol::SupernovaRemnantSymbol(const QColor &color)
{
    lines = new QSGGeometryNode;
    appendChildNode(lines);

    QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    geometry->setDrawingMode(GL_LINES);
    geometry->allocate(8);

    lines->setGeometry(geometry);
    lines->setFlag(QSGNode::OwnsGeometry);

    QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
    material->setColor(color);
    lines->setOpaqueMaterial(material);
    lines->setFlag(QSGNode::OwnsMaterial);
}

void SupernovaRemnantSymbol::updateSymbol(float x, float y, float e, float size)
{
    SymbolNode::updateSymbol(x, y, e, size);

    QSGGeometry::Point2D *vertex = lines->geometry()->vertexDataAsPoint2D();

    //First line
    vertex[0].set(0., dy1);
    vertex[1].set(dx2, 0.);

    //Second line
    vertex[2].set(dx2, 0.);
    vertex[3].set(0., dy2);

    //Third line
    vertex[4].set(0., dy2);
    vertex[5].set(dx1, 0.);

    //Fourth line
    vertex[6].set(dx1, 0.);
    vertex[7].set(0., dy1);

    lines->markDirty(QSGNode::DirtyGeometry);
}

GalaxySymbol::GalaxySymbol(const QColor &color) : e1(new EllipseNode(color))
{
    appendChildNode(e1);
}

void GalaxySymbol::updateSymbol(float x, float y, float e, float size)
{
    SymbolNode::updateSymbol(x, y, e, size);

    if (size < 1. && zoom > 20 * MINZOOM)
        size = 3.; //force ellipse above zoomFactor 20
    if (size < 1. && zoom > 5 * MINZOOM)
        size = 1.; //force points above zoomFactor 5
    if (size > 2.)
    {
        e1->updateGeometry(0, 0, size, e * size, false);
    }
    else if (size > 0.)
    {
        e1->updateGeometry(0, 0, 1, 1, false);
    }
}

GalaxyClusterSymbol::GalaxyClusterSymbol(const QColor &color) : lines(new QSGGeometryNode)
{
    QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    geometry->setDrawingMode(GL_LINES);
    geometry->allocate(32);

    lines->setGeometry(geometry);
    lines->setFlag(QSGNode::OwnsGeometry);

    QSGFlatColorMaterial *material = new QSGFlatColorMaterial;
    material->setColor(color);
    lines->setOpaqueMaterial(material);
    lines->setFlag(QSGNode::OwnsMaterial);

    appendChildNode(lines);
}

void GalaxyClusterSymbol::updateSymbol(float x, float y, float e, float size)
{
    SymbolNode::updateSymbol(x, y, e, size);

    psize = 1.;
    if (size > 50.)
        psize *= 2.;

    QSGGeometry::Point2D *vertex = lines->geometry()->vertexDataAsPoint2D();

    vertex[0].set(xa - psize, y1);
    vertex[1].set(xa + psize, y1);
    vertex[2].set(xa, y1 - psize);
    vertex[3].set(xa, y1 + psize);
    vertex[4].set(xb - psize, y1);
    vertex[5].set(xb + psize, y1);
    vertex[6].set(xb, y1 - psize);
    vertex[7].set(xb, y1 + psize);
    vertex[8].set(xa - psize, y2);
    vertex[9].set(xa + psize, y2);
    vertex[10].set(xa, y2 - psize);
    vertex[11].set(xa, y2 + psize);
    vertex[12].set(xb - psize, y2);
    vertex[13].set(xb + psize, y2);
    vertex[14].set(xb, y2 - psize);
    vertex[15].set(xb, y2 + psize);
    vertex[16].set(x1 - psize, ya);
    vertex[17].set(x1 + psize, ya);
    vertex[18].set(x1, ya - psize);
    vertex[19].set(x1, ya + psize);
    vertex[20].set(x1 - psize, yb);
    vertex[21].set(x1 + psize, yb);
    vertex[22].set(x1, yb - psize);
    vertex[23].set(x1, yb + psize);
    vertex[24].set(x2 - psize, ya);
    vertex[25].set(x2 + psize, ya);
    vertex[26].set(x2, ya - psize);
    vertex[27].set(x2, ya + psize);
    vertex[28].set(x2 - psize, yb);
    vertex[29].set(x2 + psize, yb);
    vertex[30].set(x2, yb - psize);
    vertex[31].set(x2, yb + psize);

    lines->markDirty(QSGNode::DirtyGeometry);
}

DSOSymbolNode::DSOSymbolNode(DeepSkyObject *skyObject, const QColor &color)
    : m_color(color), m_dso(skyObject)
{
}

void DSOSymbolNode::initSymbol()
{
    if (!m_symbol)
    {
        int type = m_dso->type();
        switch (type)
        {
            case 0:
            case 1:
                //catalog star
                m_symbol = new StarSymbol(m_color);
                break;
            case 3:  //Open cluster; draw circle of points
            case 13: // Asterism
                m_symbol = new AsterismSymbol(m_color);
                break;
            case 4: //Globular Cluster
                m_symbol = new GlobularClusterSymbol(m_color);
                m_rotate = true;
                break;
            case 5:  //Gaseous Nebula
            case 15: // Dark Nebula
                m_symbol = new DarkNebulaSymbol(m_color);
                m_rotate = true;
                break;
            case 6: //Planetary Nebula
                m_symbol = new PlanetaryNebulaSymbol(m_color);
                m_rotate = true;
                break;
            case 7: //Supernova remnant
                m_symbol = new SupernovaRemnantSymbol(m_color);
                m_rotate = true;
                break;
            case 8:  //Galaxy
            case 16: // Quasar
                m_symbol = new GalaxySymbol(m_color);
                m_rotate = true;
                break;
            case 14: // Galaxy cluster - draw a circle of + marks
                m_symbol = new GalaxyClusterSymbol(m_color);
                m_rotate = true;
                break;
            default:
                break;
        }
        if (m_symbol)
            addChildNode(m_symbol);
    }
}

void DSOSymbolNode::changePos(const QPointF &pos, float positionangle)
{
    QMatrix4x4 m(1, 0, 0, pos.x(), 0, 1, 0, pos.y(), 0, 0, 1, 0, 0, 0, 0, 1);
    //FIXME: this is probably incorrect (inherited from drawDeepSkyImage())
    if (m_rotate)
    {
        m.rotate(positionangle, 0, 0, 1);
    }

    setMatrix(m);
    markDirty(QSGNode::DirtyMatrix);
}

void DSOSymbolNode::update(float size, const QPointF &pos, float positionangle)
{
    initSymbol();

    if (m_symbol)
    {
        m_symbol->updateSymbol(pos.x(), pos.y(), m_dso->e(), size);
        show();
        changePos(pos, positionangle);
    }
    else
    {
        qDebug() << "Symbol for object " << m_dso->name() << " wasn't created. Check DSOSymbolNode::initSymbol()";
    }
}
