/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"
#include "skyopacitynode.h"
#include "deepskycomponent.h"
#include "skynodes/trixelnode.h"

class DeepSkyComponent;
class SkyMesh;
class StarBlockFactory;
class MeshIterator;

/**
 * @short This class represents DSOs from particular catalog. To create a node, first create a DSOTrixelNode,
 * (if a node of this trixelID is not a child of m_trixels yet), append it to m_trixels and then append
 * DeepSkyNode to DSOTrixelNode
 */
class DSOIndexNode : public SkyOpacityNode
{
  public:
    DSOIndexNode(DeepSkyIndex *index, LabelsItem::label_t labelType, QString colorString);

    /** @short hides the catalog nodes and their labels */
    virtual void hide();

    /** @short shows the catalog nodes and their labels */
    virtual void show();

    DeepSkyIndex *m_index { nullptr };
    QSGNode *m_trixels { nullptr };
    /** @short m_labelType holds label type of this catalog */
    LabelsItem::label_t m_labelType;
    /** @short schemeColor holds the color, with which nodes of this catalog should be drawn */
    QString schemeColor;
};

/**
 * @short The DSOTrixelNode class represents trixel. Symbols should be appended to m_symbols, labels to
 * m_labels and DeepSkyNodes directly to DSOTrixelNode
 */
class DSOTrixelNode : public TrixelNode
{
  public:
    explicit DSOTrixelNode(Trixel trixelID);

    virtual void deleteAllChildNodes();

    TrixelNode *m_labels { nullptr };
    QSGNode *m_symbols { nullptr };
};

/**
 * @class DeepSkyItem
 *
 * @short Class that handles representation of Deep Sky Objects.
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class DeepSkyItem : public SkyItem
{
  public:
    /**
     * @short Constructor. Instantiates DSOIndexNodes for catalogs
     * @param dsoComp pointer to DeepSkyComponent that handles data
     * @param rootNode parent RootNode that instantiated this object
     */
    DeepSkyItem(DeepSkyComponent *dsoComp, RootNode *rootNode);

    /** @short Call update on all DSOIndexNodes (catalogs) */
    virtual void update();

    /**
     * @short update all nodes needed to represent DSO in the given DSOIndexNode
     * In this function we perform some tricks to reduce memory consumption:
     * 1. Each TrixelNode has hideCount() function that returns the number of updates, during which this TrixelNode
     * was hidden. Whenever TrixelNode becomes visible this counter is set to 0 and is not being incremented.
     * 2. Based on the zoom level we calculate the limit for hideCount. If hideCount() of particular TrixelNode is
     * larger than the limit, we delete all nodes of this TrixelNode.
     * 3. If DSOTrixelNode is visible, we iterate over its DeepSkyObjects and DeepSkyNodes. If hideCount of DeepSkyNode
     * is larger than the limit we delete it. If DeepSkyObject is visible but no DeepSkyNode to represent this object
     * is created, we instantiate a new one.
     * @param node - DSOIndexNode(catalog) that should be updated
     * @param drawObject - true if objects from this catalog should be drawn, false otherwise
     * @param region - MeshIterator that should be used to iterate over visible trixels
     * @param drawImage - true if images for objects should be drawn, false otherwise
     */
    void updateDeepSkyNode(DSOIndexNode *node, bool drawObject, MeshIterator *region, bool drawImage = false);

  private:
    DeepSkyComponent *m_dsoComp { nullptr };
    SkyMesh *m_skyMesh { nullptr };

    DSOIndexNode *m_Messier { nullptr };
    DSOIndexNode *m_NGC { nullptr };
    DSOIndexNode *m_IC { nullptr };
    DSOIndexNode *m_other { nullptr };
};
