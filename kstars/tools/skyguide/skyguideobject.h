/***************************************************************************
                          skyguideobject.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2015/06/24
    copyright            : (C) 2015 by Marcos Cardinot
    email                : mcardinot@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYGUIDEOBJECT_H
#define SKYGUIDEOBJECT_H

#include <QDate>
#include <QObject>
#include <QVariantMap>

class SkyGuideObject : public QObject
{
  Q_OBJECT

public:
    SkyGuideObject(const QVariantMap &map);

    QString title() const;
    void setTitle(const QString &title);

private:
    QString m_title;
    QString m_description;
    QString m_language;
    QDate m_creationDate;
    int m_version;
};

#endif // SKYGUIDEOBJECT_H
