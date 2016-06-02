/** *************************************************************************
                          TrixelNode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 16/05/2016
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
#ifndef TRIXELNODE_H_
#define TRIXELNODE_H_
#include <QSGSimpleTextureNode>
#include "skynode.h"
#include "linelist.h"

class PlanetItemNode;
class SkyMapLite;
class PointNode;
class LineNode;
class QSGOpacityNode;

/** @class TrixelNode
 *
 * A SkyNode derived class used for displaying PointNode with coordinates provided by SkyObject.
 *
 *@short A SkyNode derived class that represents stars and objects that are drawn as stars
 *@author Artem Fedoskin
 *@version 1.0
 */

class RootNode;

class TrixelNode : public SkyNode  {
public:
    /**
     * @short Constructor
     * @param skyObject pointer to SkyObject that has to be displayed on SkyMapLite
     * @param parentNode pointer to the top parent node, which holds texture cache
     * @param spType spectral class of PointNode
     * @param size initial size of PointNode
     */
    TrixelNode(Trixel trixel, LineListList *lineIndex);

    void setStyle(QString color, int width);

    virtual void changePos(QPointF pos) {}
    virtual void update() override;
    virtual void hide() override;
private:
    QSGOpacityNode *m_opacity;
    Trixel trixel;
    LineListList *m_linesLists;
};

#endif


