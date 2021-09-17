/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../fovitem.h"

#include <QSGSimpleRectNode>
#include <QSGTransformNode>

class EllipseNode;
class RectNode;

/**
 * @class FOVSymbolBase
 *
 * FOVSymbolBase is a virtual class that should be subclassed by every type of FOV symbol. It is derived
 * from QSGTransformNode to provide transform matrix for updating coordinates of FOV symbol.
 *
 * @short A QSGTransformNode derived base class of every type of FOV symbol
 * @author Artem Fedoskin
 * @version 1.0
 */
class FOVSymbolBase : public QSGTransformNode
{
  public:
    /** @short updates geometry (position, size) of elements of this FOV symbol */
    virtual void updateSymbol(QColor color, float pixelSizeX, float pixelSizeY) = 0;

    /** @short return type of this FOV symbol **/
    FOVItem::Shape type() { return m_shape; }

  protected:
    /** @param shape of the symbol. Each subclass sets its own type. Returned in type() */
    FOVSymbolBase(FOVItem::Shape shape);
    /*    QImage m_image; //Not supported yet
            bool m_imageDisplay;*/
    FOVItem::Shape m_shape;
};

class SquareFOV : public FOVSymbolBase
{
  public:
    SquareFOV();
    virtual void updateSymbol(QColor color, float pixelSizeX, float pixelSizeY);

  private:
    RectNode *rect1 { nullptr };
    RectNode *rect2 { nullptr };
    QSGGeometryNode *lines { nullptr };
};
class CircleFOV : public FOVSymbolBase
{
  public:
    CircleFOV();
    virtual void updateSymbol(QColor color, float pixelSizeX, float pixelSizeY);

  private:
    EllipseNode *el { nullptr };
};

class CrosshairFOV : public FOVSymbolBase
{
  public:
    CrosshairFOV();
    virtual void updateSymbol(QColor color, float pixelSizeX, float pixelSizeY);

  private:
    QSGGeometryNode *lines { nullptr };
    EllipseNode *el1 { nullptr };
    EllipseNode *el2 { nullptr };
};

class BullsEyeFOV : public FOVSymbolBase
{
  public:
    BullsEyeFOV();
    virtual void updateSymbol(QColor color, float pixelSizeX, float pixelSizeY);

  private:
    EllipseNode *el1 { nullptr };
    EllipseNode *el2 { nullptr };
    EllipseNode *el3 { nullptr };
};

class SolidCircleFOV : public FOVSymbolBase
{
  public:
    SolidCircleFOV();
    virtual void updateSymbol(QColor color, float pixelSizeX, float pixelSizeY);

  private:
    EllipseNode *el { nullptr };
};

/**
 * @class FOVSymbolNode
 *
 * A SkyNode derived class used for displaying FOV symbol. FOVSymbolNade handles creation of FOVSymbolBase
 * and its update.
 *
 * @short A SkyNode derived class that is used for displaying FOV symbol
 * @author Artem Fedoskin
 * @version 1.0
 */
class FOVSymbolNode : public SkyNode
{
  public:
    /**
     * @short Constructor. Initialize m_symbol according to shape
     * @param name - name of the FOV symbol (used to switch it on/off through SkyMapLite from QML)
     * @param a - width
     * @param b - height
     * @param xoffset - x offset
     * @param yoffset - y offset
     * @param rot - rotation
     * @param shape - shape
     * @param color - RGB color
     */
    FOVSymbolNode(const QString &name, float a, float b, float xoffset, float yoffset, float rot,
                  FOVItem::Shape shape = FOVItem::SQUARE, const QString &color = "#FFFFFF");
    /** @short Update this FOV symbol according to the zoomFactor */
    void update(float zoomFactor);

    QString getName() { return m_name; }

  private:
    QString m_name;
    QString m_color;
    float m_sizeX { 0 };
    float m_sizeY { 0 };
    float m_offsetX { 0 };
    float m_offsetY { 0 };
    float m_rotation { 0 };
    float m_northPA { 0 };
    SkyPoint m_center;
    FOVSymbolBase *m_symbol { nullptr };
};
