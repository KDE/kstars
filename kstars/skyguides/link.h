/***************************************************************************
                          link.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Oct 9 2011
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

#ifndef LINK_H
#define LINK_H

#include "skyguides.h"

#include <QString>

class SkyGuides::Link
{
public:
    Link(const QString &label, const QString &description, const QString &url)
    {
        setLink(label, description, url);
    }

    QString label() const { return m_Label; }
    QString description() const { return m_Description; }
    QString url() const { return m_Url; }

    void setLink(const QString &label, const QString &description, const QString &url);

private:
    QString m_Label;
    QString m_Description;
    QString m_Url;
};

#endif // LINK_H
