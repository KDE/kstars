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
    clear();
}

void ListComponent::clear()
{
    qDeleteAll(m_ObjectList);
    m_ObjectList.clear();
    m_ObjectHash.clear();
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
    auto data = KStarsData::Instance();
    for (auto &object : m_ObjectList)
    {
        if (num)
            object->updateCoords(num);
        object->EquatorialToHorizontal(data->lst(), data->geo()->lat());
    }
}

SkyObject *ListComponent::findByName(const QString &name, bool exact)
{
    auto object = m_ObjectHash[name.toLower()];
    if (object)
        return object;
    else if (!exact)
    {
        auto object = std::find_if(m_ObjectHash.begin(), m_ObjectHash.end(), [name](const auto & oneObject)
        {
            return oneObject && oneObject->name().contains(name, Qt::CaseInsensitive);
        });
        if (object != m_ObjectHash.end())
            return *object;
    }

    return nullptr;
}

SkyObject *ListComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    if (!selected())
        return nullptr;

    SkyObject *oBest = nullptr;
    for (auto &object : m_ObjectList)
    {
        double r = object->angularDistanceTo(p).Degrees();
        if (r < maxrad)
        {
            oBest  = object;
            maxrad = r;
        }
    }
    return oBest;
}
