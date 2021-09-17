/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyobjlistmodel.h"

#include "skyobject.h"
#include "skyobjitem.h"

SkyObjListModel::SkyObjListModel(SkyObjItem *soitem, QObject *parent) : QAbstractListModel(parent)
{
    //FIXME Needs porting to KF5
    //Fixed in roleNames(). setRoleNames is not a member of QAbstractListModel anymore
    //setRoleNames(soitem->roleNames());
    Q_UNUSED(soitem);
}

QHash<int, QByteArray> SkyObjListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SkyObjItem::DispNameRole]     = "dispName";
    roles[SkyObjItem::DispImageRole]    = "imageSource";
    roles[SkyObjItem::DispSummaryRole]  = "dispObjSummary";
    roles[SkyObjItem::CategoryRole]     = "type";
    roles[SkyObjItem::CategoryNameRole] = "typeName";
    return roles;
}

void SkyObjListModel::addSkyObject(SkyObjItem *soitem)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_SoItemList.append(soitem);
    endInsertRows();
}

int SkyObjListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return m_SoItemList.size();
}

QVariant SkyObjListModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() > rowCount())
        return QVariant();

    SkyObjItem *soitem = m_SoItemList[index.row()];

    return soitem->data(role);
}

QList<SkyObjItem *> SkyObjListModel::getSkyObjItems()
{
    return m_SoItemList;
}

SkyObjItem *SkyObjListModel::getSkyObjItem(int index)
{
    if (m_SoItemList.size() > index)
        return m_SoItemList[index];
    else
        return nullptr;
}

int SkyObjListModel::getSkyObjIndex(SkyObjItem *item)
{
    for (int i = 0; i < m_SoItemList.size(); i++)
    {
        if (item->getName() == m_SoItemList[i]->getName())
            return i;
    }
    return -1;
}

void SkyObjListModel::resetModel()
{
    m_SoItemList.clear();
}
