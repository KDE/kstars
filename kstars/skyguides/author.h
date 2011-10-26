/***************************************************************************
                          author.h  -  K Desktop Planetarium
                             -------------------
    begin                : Fri Oct 7 2011
    copyright            : (C) 2011 by Łukasz Jaśkiewicz
    email                : lucas.jaskiewicz@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef AUTHOR_H
#define AUTHOR_H

#include "skyguides.h"

#include <QStringList>

class SkyGuides::Author
{
public:
    Author(const QString &name, const QString &surname, const QStringList &contacts)
    {
        setAuthor(name, surname, contacts);
    }

    QString name() const { return m_Name; }
    QString surname() const { return m_Surname; }
    QStringList contacts() const { return m_Contacts; }

    void setAuthor(const QString &name, const QString &surname, const QStringList &contacts);

private:
    QString m_Name;
    QString m_Surname;
    QStringList m_Contacts;
};

#endif // AUTHOR_H
