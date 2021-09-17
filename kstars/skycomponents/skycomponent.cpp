/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skycomponent.h"

#include "Options.h"
#include "skycomposite.h"
#include "skyobjects/skyobject.h"

SkyComponent::SkyComponent(SkyComposite *parent) : m_parent(parent)
{
}

//Hand the message up to SkyMapComposite
void SkyComponent::emitProgressText(const QString &message)
{
    parent()->emitProgressText(message);
}

SkyObject *SkyComponent::findByName(const QString &)
{
    return nullptr;
}

SkyObject *SkyComponent::objectNearest(SkyPoint *, double &)
{
    return nullptr;
}

void SkyComponent::drawTrails(SkyPainter *)
{
}

void SkyComponent::objectsInArea(QList<SkyObject *> &, const SkyRegion &)
{
}

QHash<int, QStringList> &SkyComponent::getObjectNames()
{
    if (!parent())
    {
        // Use a fake list if there is no parent object
        static QHash<int, QStringList> temp;

        return temp;
    }
    return parent()->objectNames();
}

QHash<int, QVector<QPair<QString, const SkyObject *>>> &SkyComponent::getObjectLists()
{
    if (!parent())
    {
        // Use a fake list if there is no parent object
        static QHash<int, QVector<QPair<QString, const SkyObject *>>> temp;

        return temp;
    }
    return parent()->objectLists();
}

void SkyComponent::removeFromNames(const SkyObject *obj)
{
    QStringList &names = getObjectNames()[obj->type()];
    int i;
    i = names.indexOf(obj->name());
    if (i >= 0)
        names.removeAt(i);

    i = names.indexOf(obj->longname());
    if (i >= 0)
        names.removeAt(i);
}

void SkyComponent::removeFromLists(const SkyObject *obj)
{
    QVector<QPair<QString, const SkyObject *>> &names = getObjectLists()[obj->type()];
    int i;
    i = names.indexOf(QPair<QString, const SkyObject *>(obj->name(), obj));
    if (i >= 0)
        names.removeAt(i);

    i = names.indexOf(QPair<QString, const SkyObject *>(obj->longname(), obj));
    if (i >= 0)
        names.removeAt(i);
}
