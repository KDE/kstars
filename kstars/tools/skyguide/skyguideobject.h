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
#include <QDir>
#include <QObject>
#include <QVariantMap>

#define SKYGUIDE_FORMAT_VERSION 1

class SkyGuideObject : public QObject
{
    Q_OBJECT

    // header
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QString language READ language CONSTANT)
    Q_PROPERTY(QString creationDate READ creationDate CONSTANT)
    Q_PROPERTY(QString version  READ version CONSTANT)

    // slides
    Q_PROPERTY(QStringList contents READ contents CONSTANT)
    Q_PROPERTY(QString slideTitle READ slideTitle CONSTANT)
    Q_PROPERTY(QString slideText READ slideText CONSTANT)
    Q_PROPERTY(QString slideImgPath READ slideImgPath CONSTANT)
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
        QString centerPoint;
        QString image;
        QString text;
        QString title;
    } Slide;

    SkyGuideObject(const QString &path, const QVariantMap &map);

    bool isValid() { return m_isValid; }
    int currentSlide() { return m_currentSlide; }
    void setCurrentSlide(int slide) { m_currentSlide = slide; }

    QString title() { return m_title; }
    QString description() { return m_description; }
    QString language() { return m_language; }
    QString creationDate() { return m_creationDate.toString("MMMM d, yyyy"); }
    QString version() { return QString::number(m_version); }

    QStringList contents() { return m_contents; }
    QString slideTitle() { return m_slides.at(m_currentSlide).title; }
    QString slideText() { return m_slides.at(m_currentSlide).text; }
    QString slideImgPath() {
        return m_path + QDir::toNativeSeparators("/" + m_slides.at(m_currentSlide).image);
    }

signals:
    void slideChanged();

private:
    bool m_isValid;
    QString m_path;
    int m_currentSlide;
    QString m_title;
    QString m_description;
    QString m_language;
    QDate m_creationDate;
    int m_version;
    QList<Author> m_authors;
    QList<Slide> m_slides;
    QStringList m_contents;
};

#endif // SKYGUIDEOBJECT_H
