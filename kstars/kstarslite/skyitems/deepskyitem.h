/** *************************************************************************
                          deepskyitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 18/06/2016
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
#ifndef DEEPSKYITEM_H_
#define DEEPSKYITEM_H_

#include "skyitem.h"
#include "skyopacitynode.h"
#include "deepskycomponent.h"
#include "skynodes/trixelnode.h"

class DSOIndexNode : public SkyOpacityNode {
public:
    DSOIndexNode(DeepSkyIndex *index, LabelsItem::label_t labelType, QString colorString);

    QSGNode *m_trixels;
    DeepSkyIndex *m_index;

    LabelsItem::label_t m_labelType;
    QString schemeColor;

    virtual void hide();
    virtual void show();
};

class DSOTrixelNode : public TrixelNode {
public:
    DSOTrixelNode(Trixel trixelID);
    TrixelNode *m_labels;
    QSGNode *m_symbols;
};

    /** @class StarItem
     *
     *@short Class that handles Deep Sky Objects
     *@author Artem Fedoskin
     *@version 1.0
     */

class DeepSkyComponent;
class SkyMesh;
class StarBlockFactory;
class MeshIterator;

class DeepSkyItem : public SkyItem {
public:
    /**
     * @short Constructor.
     * @param rootNode parent RootNode that instantiated this object
     */
    DeepSkyItem(DeepSkyComponent *dsoComp, RootNode *rootNode);

    /**
     * @short updates all trixels that contain stars
     */
    virtual void update();

    void updateDeepSkyNode(DSOIndexNode *node, bool drawObject, MeshIterator *region,
                           bool drawImage = false);

private:
    DeepSkyComponent *m_dsoComp;
    SkyMesh *m_skyMesh;

    DSOIndexNode *m_Messier;
    DSOIndexNode *m_NGC;
    DSOIndexNode *m_IC;
    DSOIndexNode *m_other;
};
#endif

