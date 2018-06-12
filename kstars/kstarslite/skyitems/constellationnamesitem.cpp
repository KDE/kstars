/***************************************************************************
                          constellationnamesitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 10/06/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "constellationnamesitem.h"

#include "constellationnamescomponent.h"
#include "labelsitem.h"
#include "Options.h"
#include "rootnode.h"
#include "skymaplite.h"
#include "projections/projector.h"
#include "skynodes/labelnode.h"

ConstellationName::ConstellationName(SkyObject *skyObj) : obj(skyObj), latin(0), secondary(0)
{
}

void ConstellationName::hide()
{
    if (latin)
        latin->hide();
    if (secondary)
        secondary->hide();
}

ConstellationNamesItem::ConstellationNamesItem(ConstellationNamesComponent *constComp, RootNode *parent)
    : SkyItem(LabelsItem::label_t::CONSTEL_NAME_LABEL, parent), m_constelNamesComp(constComp)
{
    recreateList();
}

void ConstellationNamesItem::update()
{
    if (!m_constelNamesComp->selected())
    {
        hide();
        rootNode()->labelsItem()->hideLabels(labelType());
        return;
    }

    show();

    const Projector *proj = SkyMapLite::Instance()->projector();

    foreach (ConstellationName *constName, m_names)
    {
        SkyObject *p = constName->obj;
        if (!proj->checkVisibility(p))
        {
            constName->hide();
            continue;
        }

        bool visible = false;
        QPointF o    = proj->toScreen(p, false, &visible);
        if (!visible || !proj->onScreen(o))
        {
            constName->hide();
            continue;
        }

        QString name;

        if (Options::useLatinConstellNames() || Options::useLocalConstellNames())
        {
            name = p->name();

            if (!constName->latin)
            {
                constName->latin = rootNode()->labelsItem()->addLabel(name, labelType());
            }

            o.setX(o.x() - 5.0 * name.length());

            constName->latin->setLabelPos(o);
            if (constName->secondary)
                constName->secondary->hide();
        }
        else
        {
            name = p->name2();

            if (!constName->secondary)
            {
                constName->secondary = rootNode()->labelsItem()->addLabel(name, labelType());
            }

            o.setX(o.x() - 5.0 * name.length());

            constName->secondary->setLabelPos(o);
            if (constName->latin)
                constName->latin->hide();
        }
    }
}

void ConstellationNamesItem::recreateList()
{
    rootNode()->labelsItem()->deleteLabels(labelType());
    m_names.clear();

    foreach (SkyObject *skyObj, m_constelNamesComp->objectList())
    {
        m_names.append(new ConstellationName(skyObj));
    }
}
