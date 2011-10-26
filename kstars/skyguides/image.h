/***************************************************************************
                          image.h  -  K Desktop Planetarium
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

#ifndef IMAGE_H
#define IMAGE_H

#include "skyguides.h"

#include <QString>

class SkyGuides::Image
{
public:
    Image(const QString &title, const QString &description, const QString &url)
    {
        setImage(title, description, url);
    }

    QString title() const { return m_Title; }
    QString description() const { return m_Description; }
    QString url() const { return m_Url; }

    void setImage(const QString &title, const QString &description, const QString &url);

private:
    QString m_Title;
    QString m_Description;
    QString m_Url;
};

#endif // IMAGE_H
