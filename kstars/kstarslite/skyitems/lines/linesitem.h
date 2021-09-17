/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../skyitem.h"
#include "../skyopacitynode.h"

class LineListIndex;

class LineIndexNode : public SkyOpacityNode
{
  public:
    explicit LineIndexNode(const QString &schemeColor = "");

    QString getSchemeColor() { return schemeColor; }

  private:
    QString schemeColor;
};

/**
 * @class LinesItem
 *
 * Class that handles lines (Constellation lines and boundaries and both coordinate grids) in
 * SkyMapLite.
 *
 * To display lines component use addLinesComponent.
 *
 * @note see RootNode::RootNode() for example of adding lines
 * @short Class that handles most of the lines in SkyMapLite
 * @author Artem Fedoskin
 * @version 1.0
 */
class LinesItem : public SkyItem
{
  public:
    /**
     * @short Constructor
     * @param rootNode parent RootNode that instantiated this object
     */
    explicit LinesItem(RootNode *rootNode);

    /**
     * @short adds LinesListIndex that is needed to be displayed to m_lineIndexes
     * @param linesComp LineListIndex derived object
     * @param color desired color of lines specified as name of entry in ColorScheme
     * @param width thickness of lines
     * @param style desired style (currently supports only Qt::SolidLine)
     */
    void addLinesComponent(LineListIndex *linesComp, QString color, int width, Qt::PenStyle style);

    /**
     * @short updates all trixels that are associated with LineListIndex or hide them if selected()
     * of this LineListIndex returns false
     */

    virtual void update();

  private:
    QMap<LineIndexNode *, LineListIndex *> m_lineIndexes;
};
