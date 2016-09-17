/** *************************************************************************
                          crosshairnode.h  -  K Desktop Planetarium
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

class CrosshairNode : public SkyNode {
public:
    /**
     * @short Constructor. Initializes lines, ellipses and labels.
     * @param baseDevice - pointer to telescope
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


