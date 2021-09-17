/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eclipticitem.h"

#include "ecliptic.h"
#include "../labelsitem.h"
#include "../rootnode.h"
#include "../skynodes/labelnode.h"
#include "../skynodes/nodes/linenode.h"
#include "../skynodes/trixelnode.h"

EclipticItem::EclipticItem(Ecliptic *eclipticComp, RootNode *rootNode)
    : SkyItem(LabelsItem::label_t::ECLIPTIC_LABEL, rootNode), m_eclipticComp(eclipticComp)
{
    LineListHash *trixels = eclipticComp->lineIndex();

    auto i = trixels->cbegin();

    while (i != trixels->cend())
    {
        std::shared_ptr<LineListList> linesList = *i;

        QList<std::shared_ptr<LineList>> addedLines;

        if (linesList->size())
        {
            QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed("EclColor");

            for (int c = 0; c < linesList->size(); ++c)
            {
                std::shared_ptr<LineList> lines = linesList->at(c);

                if (!addedLines.contains(lines))
                {
                    LineNode *ln = new LineNode(lines.get(), 0, schemeColor, 1, Qt::SolidLine);

                    appendChildNode(ln);
                    addedLines.append(lines);
                }
            }
        }
        ++i;
    }

    KStarsData *data = KStarsData::Instance();
    KSNumbers num(data->ut().djd());
    dms elat(0.0), elng(0.0);
    QString label;

    for (int ra = 0; ra < 23; ra += 6)
    {
        elng.setH(ra);
        SkyPoint *o = new SkyPoint;
        o->setFromEcliptic(num.obliquity(), elng, elat);
        o->setRA0(o->ra());
        o->setDec0(o->dec());
        o->EquatorialToHorizontal(data->lst(), data->geo()->lat());

        label.setNum(o->ra().reduce().Degrees());

        LabelNode *compass = rootNode->labelsItem()->addLabel(label, labelType());
        m_compassLabels.insert(o, compass);
    }
}

void EclipticItem::update()
{
    if (m_eclipticComp->selected())
    {
        show();
        QColor schemeColor = KStarsData::Instance()->colorScheme()->colorNamed("EclColor");

        UpdateID updateID = KStarsData::Instance()->updateID();

        QSGNode *l = firstChild();
        while (l != 0)
        {
            LineNode *lines = static_cast<LineNode *>(l);
            lines->setColor(schemeColor);
            l = l->nextSibling();

            LineList *lineList = lines->lineList();
            if (lineList->updateID != updateID)
                m_eclipticComp->JITupdate(lineList);

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
