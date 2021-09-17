/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "listcomponent.h"

#include "kstarsdata.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#endif

ListComponent::ListComponent(SkyComposite *parent) : SkyComponent(parent)
{
}

ListComponent::~ListComponent()
{
    qDeleteAll(m_ObjectList);
    m_ObjectList.clear();
    m_ObjectHash.clear();

    clear();
}

void ListComponent::clear()
{
    while (!m_ObjectList.isEmpty())
    {
        SkyObject *o = m_ObjectList.takeFirst();
        removeFromNames(o);
        delete o;
    }
}

void ListComponent::appendListObject(SkyObject *object)
{
    // Append to the Object List
    m_ObjectList.append(object);

    // Insert multiple Names
    m_ObjectHash.insert(object->name().toLower(), object);
    m_ObjectHash.insert(object->longname().toLower(), object);
    m_ObjectHash.insert(object->name2().toLower(), object);
}

void ListComponent::update(KSNumbers *num)
{
    if (!selected())
        return;
    KStarsData *data = KStarsData::Instance();
    foreach (SkyObject *o, m_ObjectList)
    {
        if (num)
            o->updateCoords(num);
        o->EquatorialToHorizontal(data->lst(), data->geo()->lat());
    }
}

SkyObject *ListComponent::findByName(const QString &name)
{
    return m_ObjectHash[name.toLower()]; // == nullptr if not found.
}

SkyObject *ListComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    if (!selected())
        return nullptr;

    SkyObject *oBest = nullptr;
    foreach (SkyObject *o, m_ObjectList)
    {
        double r = o->angularDistanceTo(p).Degrees();
        if (r < maxrad)
        {
            oBest  = o;
            maxrad = r;
        }
    }
    return oBest;
}
