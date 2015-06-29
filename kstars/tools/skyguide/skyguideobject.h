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

#define SKYGUIDE_FORMAT_VERSION 1

class SkyGuideObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString slideTitle READ slideTitle CONSTANT)

    Q_PROPERTY(int currentSlide READ currentSlide WRITE setCurrentSlide NOTIFY slideChanged)

public:
    typedef struct
    {
        QString name;
        QString email;
        QString url;
    } Author;

    typedef struct
    {
        QString title;
        QString image;
        QString content;
        QString centerPoint;
    } Slide;

    SkyGuideObject(const QVariantMap &map);

    inline bool isValid() { return m_isValid; }
    inline int currentSlide() { return m_currentSlide; }
    inline QString title() { return m_title; }
    inline QString slideTitle() { return m_slides.at(m_currentSlide).title; }

    inline void setCurrentSlide(int slide) { m_currentSlide = slide; }

signals:
    void slideChanged();

private:
    bool m_isValid;
    int m_currentSlide;
    QString m_title;
    QString m_description;
    QString m_language;
    QDate m_creationDate;
    int m_version;
    QList<Author> m_authors;
    QList<Slide> m_slides;
};

#endif // SKYGUIDEOBJECT_H
