/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyobjectlistmodel.h"

#include "skyobject.h"

SkyObjectListModel::SkyObjectListModel(QObject *parent) : QAbstractListModel(parent)
{
}

QHash<int, QByteArray> SkyObjectListModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[Qt::DisplayRole] = "name";
    roles[SkyObjectRole]   = "skyobject";
    return roles;
}

int SkyObjectListModel::indexOf(const QString &objectName) const
{
    for (int i = 0; i < skyObjects.size(); ++i)
    {
        if (skyObjects[i].first == objectName)
        {
            return i;
        }
    }
    return -1;
}

QVariant SkyObjectListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }
    if (role == Qt::DisplayRole)
    {
        return QVariant(skyObjects[index.row()].first);
    }
    else if (role == SkyObjectRole)
    {
        return qVariantFromValue((void *)skyObjects[index.row()].second);
    }
    return QVariant();
}

QStringList SkyObjectListModel::filter(const QRegExp &regEx)
{
    QStringList filteredList;

    for (auto &item : skyObjects)
    {
        if (regEx.exactMatch(item.first))
        {
            filteredList.append(item.first);
        }
    }
    return filteredList;
}

void SkyObjectListModel::setSkyObjectsList(QVector<QPair<QString, const SkyObject *>> sObjects)
{
    beginResetModel();
    skyObjects = sObjects;
    endResetModel();
}

void SkyObjectListModel::removeSkyObject(SkyObject *object)
{
    for (int i = 0; i < skyObjects.size(); ++i)
    {
        if (skyObjects[i].second == object)
        {
            skyObjects.remove(i);
            return;
        }
    }
}
