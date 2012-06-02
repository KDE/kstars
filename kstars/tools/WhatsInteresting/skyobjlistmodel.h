/***************************************************************************
                          skyobjlistmodel.h  -  K Desktop Planetarium
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

#ifndef SKYOBJ_LISTMODEL_H
#define SKYOBJ_LISTMODEL_H

#include "qabstractitemmodel.h"
#include "skyobject.h"

class SkyObjListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum SkyObjectRoles {DispNameRole = Qt::UserRole + 1 , CategoryRole };
    explicit SkyObjListModel(QObject* parent = 0);
    void addSkyObject(SkyObject *sobj);
    int rowCount( const QModelIndex& parent = QModelIndex()) const;
    QVariant data( const QModelIndex& index, int role = Qt::DisplayRole) const;

private:
    QList<SkyObject *> skyObjList;
};

#endif