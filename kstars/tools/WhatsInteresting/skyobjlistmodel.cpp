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
//#include "skyobjectitem.h"

SkyObjListModel::SkyObjListModel(QObject* parent): QAbstractListModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[DispNameRole] = "dispName";
    roles[CategoryRole] = "category";
    setRoleNames(roles);
}

void SkyObjListModel::addSkyObject(SkyObject* sobj)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    skyObjList.append(sobj);
    endInsertRows();
}

int SkyObjListModel::rowCount(const QModelIndex& parent) const
{
    return skyObjList.size();
}

QVariant SkyObjListModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() > rowCount())
        return QVariant();

    SkyObject *so = skyObjList[index.row()];
    if (role == DispNameRole)
        return so->name();
    else if (role == CategoryRole)
        return so->type();
    return QVariant();
}

QList< SkyObject* > SkyObjListModel::getSkyObjects()
{
    return skyObjList;
}

