/** *************************************************************************
                          fovsymbolnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 20/08/2016
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
#ifndef FOVSYMBOLNODE_H_
#define FOVSYMBOLNODE_H_

#include <QSGTransformNode>
#include <QSGSimpleRectNode>
#include "../fovitem.h"

class EllipseNode;
class RectNode;

/** @class FOVSymbolBase
 *
 * FOVSymbolBase is a virtual class that should be subclassed by every type of FOV symbol. It is derived
 * from QSGTransformNode to provide transform matrix for updating coordinates of FOV symbol.
 *
 *@short A QSGTransformNode derived base class of every type of FOV symbol
 *@author Artem Fedoskin
 *@version 1.0
 */

class FOVSymbolBase : public QSGTransformNode {
public:
    /**
     * @brief updateSymbol updates geometry (position, size) of elements of this FOV symbol
     */
    virtual void updateSymbol(QColor color, float pixelSizeX, float pixelSizeY ) =0;
    FOVItem::Shape type() { return m_shape; }
protected:
    /**
     * @param shape of the symbol. Each subclass sets its own type. Returned in type()
     */
    FOVSymbolBase(FOVItem::Shape shape);
/*    QImage m_image; //Not supported yet
    bool m_imageDisplay;*/
    FOVItem::Shape m_shape;
};

class SquareFOV : public FOVSymbolBase {
public:
    SquareFOV();
    virtual void updateSymbol( QColor color, float pixelSizeX, float pixelSizeY );
private:
    RectNode *rect1;
    RectNode *rect2;
    QSGGeometryNode *lines;
};
class CircleFOV : public FOVSymbolBase {
public:
    CircleFOV();
    virtual void updateSymbol( QColor color, float pixelSizeX, float pixelSizeY );
private:
    EllipseNode *el;
};

class CrosshairFOV : public FOVSymbolBase {
public:
    CrosshairFOV();
    virtual void updateSymbol( QColor color, float pixelSizeX, float pixelSizeY );
private:
    QSGGeometryNode *lines;
    EllipseNode *el1;
    EllipseNode *el2;
};

class BullsEyeFOV : public FOVSymbolBase {
public:
    BullsEyeFOV();
    virtual void updateSymbol( QColor color, float pixelSizeX, float pixelSizeY );
private:
    EllipseNode *el1;
    EllipseNode *el2;
    EllipseNode *el3;
};

class SolidCircleFOV : public FOVSymbolBase {
public:
    SolidCircleFOV();
    virtual void updateSymbol( QColor color, float pixelSizeX, float pixelSizeY );
private:
    EllipseNode *el;
};

/** @class FOVSymbolNode
 *
 * A SkyNode derived class used for displaying FOV symbol. FOVSymbolNade handles creation of FOVSymbolBase
 * and its update.
 *
 *@short A SkyNode derived class that is used for displaying FOV symbol
 *@author Artem Fedoskin
 *@version 1.0
 */

class FOVSymbolNode : public SkyNode {
public:
    FOVSymbolNode(const QString &name, float a, float b, float xoffset, float yoffset, float rot, FOVItem::Shape shape = FOVItem::SQUARE, const QString &color = "#FFFFFF");
    /**
     * @brief updates
     * @param zoomFactor
     */
    void update(float zoomFactor);

    QString getName() { return m_name; }
private:
    QString m_name, m_color;
    float   m_sizeX, m_sizeY;
    float   m_offsetX, m_offsetY;
    float   m_rotation;
    float   m_northPA;
    SkyPoint m_center;

    FOVSymbolBase *m_symbol;
};

#endif

