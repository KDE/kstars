/***************************************************************************
                          skyguide.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Oct 10 2011
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

#ifndef SKYGUIDE_H
#define SKYGUIDE_H

#include "skyguides.h"

#include <QString>
#include <QList>
#include <QDate>

class Slide;

class SkyGuides::Guide
{
public:
    Guide(const QString &title, const QString &description, const QString &language, const QString &thumbnailImg,
             const QDate &creationDate, const QString &version)
    {
        setGuide(title, description, language, thumbnailImg, creationDate, version);
    }

    ~Guide();

    QString title() const { return m_Title; }
    QString description() const { return m_Description; }
    QString language() const { return m_Language; }
    // Authors
    QString thumbnailImg() const { return m_ThumbnailImage; }
    QDate creationDate() const { return m_CreationDate; }
    QString version() const { return m_Version; }
    QList<Slide*>* slides() { return &m_Slides; }

    void setGuide(const QString &title, const QString &description, const QString &language, const QString &thumbnailImg,
                  const QDate &creationDate, const QString &version);

private:
    QString m_Title;
    QString m_Description;
    QString m_Language;
    // Authors
    QString m_ThumbnailImage;
    QDate m_CreationDate;
    QString m_Version;
    QList<Slide*> m_Slides;
};

#endif // SKYGUIDE_H
