/***************************************************************************
                          skyobjitem.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/21/06
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


#include "skyobjitem.h"

SkyObjItem::SkyObjItem(QString soname, QString sotype, QObject* parent) : QObject(parent)
{
    name = soname;
    type = sotype;
}

QVariant SkyObjItem::data(int role)
{
    switch(role)
    {
        case DispNameRole:
            return getName();
        case CategoryRole:
            return getType();
        default:
            return QVariant();
    }
}

QHash< int, QByteArray > SkyObjItem::roleNames() const
{
    QHash<int, QByteArray > roles;
    roles[DispNameRole]="dispName";
    roles[CategoryRole] = "type";
    return roles;
}