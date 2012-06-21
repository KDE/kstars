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

SkyObjListModel::SkyObjListModel(SkyObjItem* soitem, QObject* parent): QAbstractListModel(parent)
{
    setRoleNames(soitem->roleNames());
}

void SkyObjListModel::addSkyObject(SkyObjItem* sobj)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    soItemList.append(sobj);
    endInsertRows();
}

int SkyObjListModel::rowCount(const QModelIndex& parent) const
{
    return soItemList.size();
}

QVariant SkyObjListModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() > rowCount())
        return QVariant();

    SkyObjItem *soitem = soItemList[index.row()];
    /*if (role == DispNameRole)
        return soitem->name();
    else if (role == CategoryRole)
        return soitem->type();*/
    //return QVariant();
    return soitem->data(role);
}

QList< SkyObjItem* > SkyObjListModel::getSkyObjItems()
{
    return soItemList;
}

SkyObjItem* SkyObjListModel::getSkyObjItem(int index)
{
    return soItemList[index];
}

