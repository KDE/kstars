/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef CROSSHAIRNODE_H_
#define CROSSHAIRNODE_H_
#include "skynode.h"

/** @class CrossHairNode
 *
 *  @short Represents crosshair of telescope in SkyMapLite
 *  @version 1.0
 */

class EllipseNode;
class LineNode;
class LabelNode;
class QSGFlatColorMaterial;

class CrosshairNode : public SkyNode
{
  public:
    /**
         * @short Constructor. Initializes lines, ellipses and labels.
         * @param baseDevice - pointer to telescope
         * @param rootNode parent RootNode that instantiated this object
         */
    CrosshairNode(INDI::BaseDevice *baseDevice, RootNode *rootNode);

    /** Destructor. **/
    ~CrosshairNode();

    /** @short Update position and visibility of crosshair based on the Alt, Az (or Ra and Dec)
            of telescope **/
    virtual void update() override;
    virtual void hide() override;

    /** @short Set color of crosshair **/
    void setColor(QColor color);

  private:
    EllipseNode *el1;
    EllipseNode *el2;

    QSGGeometryNode *lines;
    QSGFlatColorMaterial *material;

    LabelNode *label;

    LabelsItem *labelsItem;

    INDI::BaseDevice *bd;
};

#endif
