/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "linesitem.h"

#include "linelist.h"
#include "linelistindex.h"
#include "projections/projector.h"
#include "../skynodes/nodes/linenode.h"
#include "../skynodes/trixelnode.h"

#include <QSGNode>

LineIndexNode::LineIndexNode(const QString &color) : schemeColor(color)
{
}

LinesItem::LinesItem(RootNode *rootNode) : SkyItem(LabelsItem::label_t::NO_LABEL, rootNode)
{
}

void LinesItem::addLinesComponent(LineListIndex *linesComp, QString color, int width, Qt::PenStyle style)
{
    LineIndexNode *node = new LineIndexNode(color);
    appendChildNode(node);

    m_lineIndexes.insert(node, linesComp);
    LineListHash *list = linesComp->lineIndex();
    //Sort by trixels
    QMap<Trixel, std::shared_ptr<LineListList>> trixels;

    auto s = list->cbegin();

    while (s != list->cend())
    {
        trixels.insert(s.key(), *s);
        s++;
    }

    auto i = trixels.cbegin();
    QList<std::shared_ptr<LineList>> addedLines;

    while (i != trixels.cend())
    {
        std::shared_ptr<LineListList> linesList = *i;

        if (linesList->size())
        {
            TrixelNode *trixel = new TrixelNode(i.key());

            node->appendChildNode(trixel);

            QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed(color);

            for (int c = 0; c < linesList->size(); ++c)
            {
                std::shared_ptr<LineList> list = linesList->at(c);

                if (!addedLines.contains(list))
                {
                    trixel->appendChildNode(new LineNode(linesList->at(c).get(), 0, schemeColor, width, style));
                    addedLines.append(list);
                }
            }
        }
        ++i;
    }
}

void LinesItem::update()
{
    QMap<LineIndexNode *, LineListIndex *>::iterator i = m_lineIndexes.begin();

    UpdateID updateID = KStarsData::Instance()->updateID();

    while (i != m_lineIndexes.end())
    {
        LineIndexNode *node = i.key();
        QColor schemeColor  = KStarsData::Instance()->colorScheme()->colorNamed(node->getSchemeColor());
        if (i.value()->selected())
        {
            node->show();

            QSGNode *n = node->firstChild();
            while (n != 0)
            {
                TrixelNode *trixel = static_cast<TrixelNode *>(n);
                trixel->show();

                QSGNode *l = trixel->firstChild();
                while (l != 0)
                {
                    LineNode *lines = static_cast<LineNode *>(l);
                    lines->setColor(schemeColor);
                    l = l->nextSibling();

                    LineList *lineList = lines->lineList();
                    if (lineList->updateID != updateID)
                        i.value()->JITupdate(lineList);

                    lines->updateGeometry();
                }
                n = n->nextSibling();
            }
        }
        else
        {
            node->hide();
        }
        ++i;
    }
}
