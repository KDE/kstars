/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skynode.h"

#include <QSGNode>

class DeepSkyObject;
class EllipseNode;

/**
 * @short A base class for all symbol nodes.
 */
class SymbolNode : public QSGNode
{
  public:
    /**
     * @short Update size and the symbol itself. Each SymbolNode should override this function,
     * and call SymbolNode::updateSymbol() in the beginning of overridden updateSymbol to
     * initialize values that are used across all symbols.
     */
    virtual void updateSymbol(float x, float y, float e, float size);

  protected:
    SymbolNode() {}
    float zoom { 0 };
    int isize { 0 };

    float dx1 { 0 };
    float dx2 { 0 };
    float dy1 { 0 };
    float dy2 { 0 };
    float x1 { 0 };
    float x2 { 0 };
    float y1 { 0 };
    float y2 { 0 };

    float dxa { 0 };
    float dxb { 0 };
    float dya { 0 };
    float dyb { 0 };
    float xa { 0 };
    float xb { 0 };
    float ya { 0 };
    float yb { 0 };
    float psize { 0 };
};

class StarSymbol : public SymbolNode
{
  public:
    explicit StarSymbol(const QColor &color = QColor());

    virtual void updateSymbol(float x, float y, float e, float size) override;

    EllipseNode *m_ellipse { nullptr };
};

class AsterismSymbol : public SymbolNode
{
  public:
    explicit AsterismSymbol(const QColor &color);

    virtual void updateSymbol(float x, float y, float e, float size) override;

    EllipseNode *e1 { nullptr };
    EllipseNode *e2 { nullptr };
    EllipseNode *e3 { nullptr };
    EllipseNode *e4 { nullptr };
    EllipseNode *e5 { nullptr };
    EllipseNode *e6 { nullptr };
    EllipseNode *e7 { nullptr };
    EllipseNode *e8 { nullptr };
};

class GlobularClusterSymbol : public SymbolNode
{
  public:
    explicit GlobularClusterSymbol(const QColor &color);

    virtual void updateSymbol(float x, float y, float e, float size) override;

    EllipseNode *e1 { nullptr };
    QSGGeometryNode *lines { nullptr };
};

class DarkNebulaSymbol : public SymbolNode
{
  public:
    explicit DarkNebulaSymbol(const QColor &color);

    virtual void updateSymbol(float x, float y, float e, float size) override;

    QSGGeometryNode *lines { nullptr };
};

class PlanetaryNebulaSymbol : public SymbolNode
{
  public:
    explicit PlanetaryNebulaSymbol(const QColor &color);

    virtual void updateSymbol(float x, float y, float e, float size) override;

    EllipseNode *e1 { nullptr };
    QSGGeometryNode *lines { nullptr };
};

class SupernovaRemnantSymbol : public SymbolNode
{
  public:
    explicit SupernovaRemnantSymbol(const QColor &color);

    virtual void updateSymbol(float x, float y, float e, float size) override;

    QSGGeometryNode *lines { nullptr };
};

class GalaxySymbol : public SymbolNode
{
  public:
    explicit GalaxySymbol(const QColor &color);

    virtual void updateSymbol(float x, float y, float e, float size) override;

    EllipseNode *e1 { nullptr };
};

class GalaxyClusterSymbol : public SymbolNode
{
  public:
    explicit GalaxyClusterSymbol(const QColor &color);

    virtual void updateSymbol(float x, float y, float e, float size) override;

    QSGGeometryNode *lines { nullptr };
};

/**
 * @class DSOSymbolNode
 * @short A SkyNode derived class used for Deep Sky symbols in SkyMapLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class DSOSymbolNode : public SkyNode
{
  public:
    /**
     * @short Constructor.
     * @param skyObject - DeepSkyObject, for which this symbol should be created
     * @param color of the symbol
     */
    explicit DSOSymbolNode(DeepSkyObject *skyObject, const QColor &color = QColor());

    /**
     * @short Changes position and rotation angle of the symbol
     * @param pos - new position
     * @param positionangle - rotation angle
     */
    void changePos(const QPointF &pos, float positionangle);

    /**
     * @short Update size and position of the symbol
     * @param size - new size of symbol
     * @param pos - new position
     * @param positionangle - new rotation angle
     */
    void update(float size, const QPointF &pos, float positionangle);

    /** Create SymbolNode based on the type of m_dso */
    void initSymbol();
    QColor getColor() { return m_color; }

  private:
    QColor m_color;
    //TODO deal setter for this when stars will be introduced
    DeepSkyObject *m_dso { nullptr };
    SymbolNode *m_symbol { nullptr };
    /// If the symbol needs rotation this should be set to true
    bool m_rotate { false };
};
