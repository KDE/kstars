/***************************************************************************
                          skyobjitem.h  -  K Desktop Planetarium
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


#ifndef SKYOBJITEM_H
#define SKYOBJITEM_H

#include <QObject>
#include "skyobject.h"

class SkyObjItem : public QObject
{
    Q_OBJECT
public:
    enum SkyObjectRoles {DispNameRole = Qt::UserRole + 1 , CategoryRole };
    enum Positions {
        N, NNE, NE, ENE,
        E, ESE, SE, SSE,
        S, SSW, SW, WSW,
        W, WNW, NW, NNW
    };
    //explicit SkyObjItem( SkyObject *so=0, QString soname = QString(), QString sotype = QString(), QObject *parent = 0);
    explicit SkyObjItem( SkyObject *sobj=0, QObject *parent = 0);
    QVariant data(int role);
    QHash<int, QByteArray> roleNames() const;
    inline QString getName() { return name; }
    inline QString getType() { return type; }
    void setPosition(SkyObject* so);
private:
    QString name;
    QString type;
    QString position;
    SkyObject* so;
};

#endif // SKYOBJITEM_H
