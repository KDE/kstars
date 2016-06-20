/***************************************************************************
                          skyobjlistmodel.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/26/05
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skyobjlistmodel.h"

SkyObjListModel::SkyObjListModel(SkyObjItem *soitem, QObject *parent): QAbstractListModel(parent)
{
    //FIXME Needs porting to KF5
    //Fixed in roleNames(). setRoleNames is not a member of QAbstractListModel anymore
    //setRoleNames(soitem->roleNames());
    Q_UNUSED(soitem);
}

QHash<int, QByteArray> SkyObjListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SkyObjItem::DispNameRole] = "dispName";
    roles[SkyObjItem::CategoryRole] = "type";
    roles[SkyObjItem::CategoryNameRole] = "typeName";
    return roles;
}

void SkyObjListModel::addSkyObject(SkyObjItem *soitem)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_SoItemList.append(soitem);
    endInsertRows();
}

int SkyObjListModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)

    return m_SoItemList.size();
}

QVariant SkyObjListModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() > rowCount())
        return QVariant();

    SkyObjItem *soitem = m_SoItemList[index.row()];

    return soitem->data(role);
}

QList< SkyObjItem *> SkyObjListModel::getSkyObjItems()
{
    return m_SoItemList;
}

SkyObjItem* SkyObjListModel::getSkyObjItem(int index)
{
    return m_SoItemList[index];
}

void SkyObjListModel::resetModel()
{
    m_SoItemList.clear();
}
