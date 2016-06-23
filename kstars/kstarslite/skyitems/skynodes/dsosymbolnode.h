/** *************************************************************************
                          dsosymbolnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 20/05/2016
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
#ifndef DSOSYMBOLNODE_H_
#define DSOSYMBOLNODE_H_
#include "skynode.h"
#include "../labelsitem.h"

class PlanetItemNode;
class SkyMapLite;
class PointNode;
class LabelNode;
class QSGSimpleTextureNode;
class EllipseNode;
class LineNode;

/** @class DSOSymbolNode
 *
 * A SkyNode derived class used for displaying PointNode with coordinates provided by SkyObject.
 *
 *@short A SkyNode derived class that represents stars and objects that are drawn as stars
 *@author Artem Fedoskin
 *@version 1.0
 */

class RootNode;

class SymbolNode : public QSGNode {
public:
    virtual void updateSymbol(float x, float y, float e, float size);
protected:
    SymbolNode() { }
    float zoom;
    int isize;

    float dx1;
    float dx2;
    float dy1;
    float dy2;
    float x1;
    float x2;
    float y1;
    float y2;

    float dxa;
    float dxb;
    float dya;
    float dyb;
    float xa;
    float xb;
    float ya;
    float yb;
    float psize;
};

class StarSymbol : public SymbolNode {
public:
    StarSymbol(QColor color = QColor());
    virtual void updateSymbol(float x, float y, float e, float size) override;
    EllipseNode *m_ellipse;
};

class AsterismSymbol : public SymbolNode {
public:
    AsterismSymbol(QColor color);
    virtual void updateSymbol(float x, float y, float e, float size) override;

    EllipseNode *e1;
    EllipseNode *e2;
    EllipseNode *e3;
    EllipseNode *e4;
    EllipseNode *e5;
    EllipseNode *e6;
    EllipseNode *e7;
    EllipseNode *e8;
};

class GlobularClusterSymbol : public SymbolNode {
public:
    GlobularClusterSymbol(QColor color);
    virtual void updateSymbol(float x, float y, float e, float size) override;

    EllipseNode *e1;
    QSGGeometryNode *lines;
};

class DarkNebulaSymbol : public SymbolNode {
public:
    DarkNebulaSymbol(QColor color);
    virtual void updateSymbol(float x, float y, float e, float size) override;

    QSGGeometryNode *lines;
};

class PlanetaryNebulaSymbol : public SymbolNode {
public:
    PlanetaryNebulaSymbol(QColor color);
    virtual void updateSymbol(float x, float y, float e, float size) override;

    EllipseNode *e1;
    QSGGeometryNode *lines;
};

class SupernovaRemnantSymbol : public SymbolNode {
public:
    SupernovaRemnantSymbol(QColor color);
    virtual void updateSymbol(float x, float y, float e, float size) override;

    QSGGeometryNode *lines;
};

class GalaxySymbol : public SymbolNode {
public:
    GalaxySymbol(QColor color);
    virtual void updateSymbol(float x, float y, float e, float size) override;

    EllipseNode *e1;
};

class GalaxyClusterSymbol : public SymbolNode {
public:
    GalaxyClusterSymbol(QColor color);
    virtual void updateSymbol(float x, float y, float e, float size) override;

    QSGGeometryNode *lines;
};

class DSOSymbolNode : public SkyNode  {
public:
    /**
     * @short Constructor
     * @param skyObject pointer to SkyObject that has to be displayed on SkyMapLite
     * @param parentNode pointer to the top parent node, which holds texture cache
     * @param spType spectral class of PointNode
     * @param size initial size of PointNode
     */
    DSOSymbolNode(DeepSkyObject *skyObject, QColor color = QColor());
    void changePos(float size, const QPointF &pos, float positionangle);
    void update(float size, const QPointF &pos, float positionangle);
private:
    //TODO deal setter for this when stars will be introduced
    DeepSkyObject *m_dso;
    SymbolNode *m_symbol;
    bool m_rotate; //If the symbol needs rotation this should be set to true
};

#endif

