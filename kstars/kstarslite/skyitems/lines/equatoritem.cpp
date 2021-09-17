/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "equatoritem.h"

#include "projections/projector.h"
#include "../rootnode.h"
#include "../labelsitem.h"
#include "../skynodes/labelnode.h"
#include "../skynodes/trixelnode.h"
#include "../skynodes/nodes/linenode.h"

#include <QSGNode>

EquatorItem::EquatorItem(Equator *equatorComp, RootNode *rootNode)
    : SkyItem(LabelsItem::label_t::EQUATOR_LABEL, rootNode), m_equatorComp(equatorComp)
{
    LineListHash *trixels = equatorComp->lineIndex();

    auto i = trixels->cbegin();

    while (i != trixels->cend())
    {
        std::shared_ptr<LineListList> linesList = *i;
        QList<std::shared_ptr<LineList>> addedLines;

        if (linesList->size())
        {
            QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed("EqColor");

            for (int c = 0; c < linesList->size(); ++c)
            {
                std::shared_ptr<LineList> lines = linesList->at(c);

                if (!addedLines.contains(lines))
                {
                    LineNode *ln = new LineNode(linesList->at(c).get(), 0, schemeColor, 1, Qt::SolidLine);

                    appendChildNode(ln);
                    addedLines.append(lines);
                }
            }
        }
        ++i;
    }

    //Add compass labels
    for (int ra = 0; ra < 23; ra += 2)
    {
        SkyPoint *o = new SkyPoint(ra, 0.0);

        QString label;
        label.setNum(o->ra().hour());

        LabelNode *compass = rootNode->labelsItem()->addLabel(label, labelType());
        m_compassLabels.insert(o, compass);
    }
}

void EquatorItem::update()
{
    if (m_equatorComp->selected())
    {
        show();
        QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed("EqColor");

        UpdateID updateID = KStarsData::Instance()->updateID();

        QSGNode *l = firstChild();
        while (l != 0)
        {
            LineNode *lines = static_cast<LineNode *>(l);
            lines->setColor(schemeColor);
            l = l->nextSibling();

            LineList *lineList = lines->lineList();
            if (lineList->updateID != updateID)
                m_equatorComp->JITupdate(lineList);

            lines->updateGeometry();
        }

        const Projector *proj = SkyMapLite::Instance()->projector();
        KStarsData *data      = KStarsData::Instance();

        QMap<SkyPoint *, LabelNode *>::iterator i = m_compassLabels.begin();

        while (i != m_compassLabels.end())
        {
            SkyPoint *c = i.key();
            c->EquatorialToHorizontal(data->lst(), data->geo()->lat());

            LabelNode *compass = (*i);

            bool visible;
            QPointF cpoint = proj->toScreen(c, false, &visible);
            if (visible && proj->checkVisibility(c))
            {
                compass->setLabelPos(cpoint);
            }
            else
            {
                compass->hide();
            }
            ++i;
        }
    }
    else
    {
        hide();
    }
}
