/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fovsymbolnode.h"

#include "nodes/rectnode.h"
#include "nodes/ellipsenode.h"

#include <QSGFlatColorMaterial>

FOVSymbolNode::FOVSymbolNode(const QString &name, float a, float b, float xoffset, float yoffset, float rot,
                             FOVItem::Shape shape, const QString &color)
    : m_symbol(nullptr)
{
    m_name  = name;
    m_sizeX = a;
    m_sizeY = (b < 0.0) ? a : b;

    m_offsetX  = xoffset;
    m_offsetY  = yoffset;
    m_rotation = rot;
    m_color    = color;
    m_northPA  = 0;
    m_center.setRA(0);
    m_center.setDec(0);

    switch (shape)
    {
        case FOVItem::SQUARE:
            m_symbol = new SquareFOV();
            break;
        case FOVItem::CIRCLE:
            m_symbol = new CircleFOV();
            break;
        case FOVItem::CROSSHAIRS:
            m_symbol = new CrosshairFOV();
            break;
        case FOVItem::BULLSEYE:
            m_symbol = new BullsEyeFOV();
            break;
        case FOVItem::SOLIDCIRCLE:
            m_symbol = new SolidCircleFOV();
            break;
        default:
            break;
    }

    if (m_symbol)
        addChildNode(m_symbol);
}

void FOVSymbolNode::update(float zoomFactor)
{
    show();
    float pixelSizeX = m_sizeX * zoomFactor / 57.3 / 60.0;
    float pixelSizeY = m_sizeY * zoomFactor / 57.3 / 60.0;

    float offsetXPixelSize = m_offsetX * zoomFactor / 57.3 / 60.0;
    float offsetYPixelSize = m_offsetY * zoomFactor / 57.3 / 60.0;
    SkyMapLite *map        = SkyMapLite::Instance();

    QMatrix4x4 newMatrix;

    if (m_center.ra().Degrees() > 0)
    {
        m_center.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
        QPointF skypoint_center = map->projector()->toScreen(&m_center);
        newMatrix.translate(skypoint_center.toPoint().x(), skypoint_center.toPoint().y());
    }
    else
    {
        QPoint center(map->width() / 2, map->height() / 2);
        newMatrix.translate(center.x(), center.y());
    }

    newMatrix.translate(offsetXPixelSize, offsetYPixelSize);
    newMatrix.rotate(m_rotation + m_northPA, 0, 0, 1);

    m_symbol->setMatrix(newMatrix);

    m_symbol->updateSymbol(m_color, pixelSizeX, pixelSizeY);
}

FOVSymbolBase::FOVSymbolBase(FOVItem::Shape shape) : m_shape(shape) //Will be changed by the subclass
{
}

SquareFOV::SquareFOV() : FOVSymbolBase(FOVItem::SQUARE)
{
    rect1 = new RectNode();
    appendChildNode(rect1);

    rect2 = new RectNode();
    appendChildNode(rect2);

    lines = new QSGGeometryNode;
    appendChildNode(lines);

    lines->setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0));
    lines->setOpaqueMaterial(new QSGFlatColorMaterial);
    lines->setFlag(QSGNode::OwnsGeometry);
    lines->setFlag(QSGNode::OwnsMaterial);
    lines->geometry()->allocate(6);
    lines->geometry()->setDrawingMode(GL_LINES);
}

void SquareFOV::updateSymbol(QColor color, float pixelSizeX, float pixelSizeY)
{
    QPoint center(0, 0);

    /*if (m_imageDisplay)
    {
        //QTransform imageT;
        //imageT.rotate(m_rotation+m_northPA);
        //p.drawImage(targetRect, m_image.transformed(imageT));
        p.drawImage(targetRect, m_image);
    }*/

    rect1->setRect(center.x() - pixelSizeX / 2, center.y() - pixelSizeY / 2, pixelSizeX, pixelSizeY);
    rect1->setColor(color);

    rect2->setRect(center.x(), center.y() - (3 * pixelSizeY / 5), pixelSizeX / 40, pixelSizeX / 10);
    rect2->setColor(color);

    QSGFlatColorMaterial *material = static_cast<QSGFlatColorMaterial *>(lines->opaqueMaterial());
    if (material->color() != color)
    {
        material->setColor(color);
        lines->markDirty(QSGNode::DirtyMaterial);
    }

    QSGGeometry::Point2D *vertex = lines->geometry()->vertexDataAsPoint2D();
    vertex[0].set(center.x() - pixelSizeX / 30, center.y() - (3 * pixelSizeY / 5));
    vertex[1].set(center.x() + pixelSizeX / 20, center.y() - (3 * pixelSizeY / 5));

    vertex[2].set(center.x() - pixelSizeX / 30, center.y() - (3 * pixelSizeY / 5));
    vertex[3].set(center.x() + pixelSizeX / 70, center.y() - (0.7 * pixelSizeY));

    vertex[4].set(center.x() + pixelSizeX / 20, center.y() - (3 * pixelSizeY / 5));
    vertex[5].set(center.x() + pixelSizeX / 70, center.y() - (0.7 * pixelSizeY));

    lines->markDirty(QSGNode::DirtyGeometry);
}

CircleFOV::CircleFOV() : FOVSymbolBase(FOVItem::CIRCLE)
{
    el = new EllipseNode();
    appendChildNode(el);
}

void CircleFOV::updateSymbol(QColor color, float pixelSizeX, float pixelSizeY)
{
    el->setColor(color);
    el->updateGeometry(0, 0, pixelSizeX / 2, pixelSizeY / 2, false);
}

CrosshairFOV::CrosshairFOV() : FOVSymbolBase(FOVItem::CROSSHAIRS)
{
    lines = new QSGGeometryNode;
    appendChildNode(lines);

    lines->setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0));
    lines->setOpaqueMaterial(new QSGFlatColorMaterial);
    lines->setFlag(QSGNode::OwnsGeometry);
    lines->setFlag(QSGNode::OwnsMaterial);
    lines->geometry()->allocate(8);
    lines->geometry()->setDrawingMode(GL_LINES);

    el1 = new EllipseNode;
    appendChildNode(el1);

    el2 = new EllipseNode;
    appendChildNode(el2);
}

void CrosshairFOV::updateSymbol(QColor color, float pixelSizeX, float pixelSizeY)
{
    QPoint center(0, 0);

    QSGGeometry::Point2D *vertex = lines->geometry()->vertexDataAsPoint2D();
    vertex[0].set(center.x() + 0.5 * pixelSizeX, center.y());
    vertex[1].set(center.x() + 1.5 * pixelSizeX, center.y());

    vertex[2].set(center.x() - 0.5 * pixelSizeX, center.y());
    vertex[3].set(center.x() - 1.5 * pixelSizeX, center.y());

    vertex[4].set(center.x(), center.y() + 0.5 * pixelSizeY);
    vertex[5].set(center.x(), center.y() + 1.5 * pixelSizeY);

    vertex[6].set(center.x(), center.y() - 0.5 * pixelSizeY);
    vertex[7].set(center.x(), center.y() - 1.5 * pixelSizeY);

    lines->markDirty(QSGNode::DirtyGeometry);

    //Draw circles at 0.5 & 1 degrees
    el1->setColor(color);
    el1->updateGeometry(center.x(), center.y(), 0.5 * pixelSizeX, 0.5 * pixelSizeY, false);

    el2->setColor(color);
    el1->updateGeometry(center.x(), center.y(), pixelSizeX, pixelSizeY, false);
}

BullsEyeFOV::BullsEyeFOV() : FOVSymbolBase(FOVItem::BULLSEYE)
{
    el1 = new EllipseNode;
    appendChildNode(el1);

    el2 = new EllipseNode;
    appendChildNode(el2);

    el3 = new EllipseNode;
    appendChildNode(el3);
}

void BullsEyeFOV::updateSymbol(QColor color, float pixelSizeX, float pixelSizeY)
{
    el1->setColor(color);
    el1->updateGeometry(0, 0, 0.5 * pixelSizeX, 0.5 * pixelSizeY, false);

    el2->setColor(color);
    el2->updateGeometry(0, 0, 2.0 * pixelSizeX, 2.0 * pixelSizeY, false);

    el3->setColor(color);
    el3->updateGeometry(0, 0, 4.0 * pixelSizeX, 4.0 * pixelSizeY, false);
}

SolidCircleFOV::SolidCircleFOV() : FOVSymbolBase(FOVItem::SOLIDCIRCLE)
{
    el = new EllipseNode;
    appendChildNode(el);
}

void SolidCircleFOV::updateSymbol(QColor color, float pixelSizeX, float pixelSizeY)
{
    QColor colorAlpha = color;
    colorAlpha.setAlpha(127);
    el->setColor(colorAlpha);
    el->updateGeometry(0, 0, pixelSizeX / 2, pixelSizeY / 2, false);
}
