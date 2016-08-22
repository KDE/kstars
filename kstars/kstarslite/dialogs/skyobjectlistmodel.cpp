/***************************************************************************
                          skyobjectlistmodel.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 29 2016
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

#include "skyobjectlistmodel.h"
#include "skyobject.h"

SkyObjectListModel::SkyObjectListModel(QObject *parent)
    :QAbstractListModel(parent)
{

}

QHash<int, QByteArray> SkyObjectListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[SkyObjectRole] = "skyobject";
    return roles;
}

QVariant SkyObjectListModel::data(const QModelIndex &index, int role) const {
    if(!index.isValid()) {
        return QVariant();
    }
    if(role == NameRole) {
        return QVariant(skyObjects[index.row()].first);
    } else if(role == SkyObjectRole) {
        return qVariantFromValue((void *) skyObjects[index.row()].second);
    }
    return QVariant();
}

void SkyObjectListModel::setSkyObjectsList(QVector<QPair<QString, const SkyObject *>> sObjects) {
    emit beginResetModel();
    skyObjects = sObjects;
    /*foreach(SkyObject *s, sObjects) {
        QString name = s->name();
        if ( ! name.isEmpty() ) {
            skyObjects.append(QPair<QString, SkyObject *>(name, s));
        }

        QString longname = s->longname();
        if ( ! longname.isEmpty() && longname != name) {
            skyObjects.append(QPair<QString, SkyObject *>(longname, s));
        }
    }*/
    emit endResetModel();
}

