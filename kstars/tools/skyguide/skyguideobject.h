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
    Q_PROPERTY(QString creationDateStr READ creationDateStr CONSTANT)
    Q_PROPERTY(int version  READ version CONSTANT)

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
        QDateTime skyDateTime;
        QString text;
        QString title;
        double zoomFactor;
    } Slide;

    SkyGuideObject(const QString &path, const QVariantMap &map);

    QJsonDocument toJsonDocument();

    QString toZip();

    bool isValid() { return m_isValid; }

    QString path() { return m_path; }
    void setPath(QString path) { m_path = path; }

    int currentSlide() { return m_currentSlide; }
    void setCurrentSlide(int slide);

    QString title() { return m_title; }
    void setTitle(QString title) { m_title = title; }

    QString description() { return m_description; }
    void setDescription(QString desc) { m_description = desc; }

    QString language() { return m_language; }
    void setLanguage(QString lang) { m_language = lang; }

    QDate creationDate() { return m_creationDate; }
    QString creationDateStr() { return m_creationDate.toString("MMMM d, yyyy"); }
    void setCreationDate(QDate date) { m_creationDate = date; }

    int version() { return m_version; }
    void setVersion(int version) { m_version = version; }

    QList<Author> authors() { return m_authors; }
    void setAuthors(QList<Author> authors) { m_authors = authors; }

    QList<Slide> slides() { return m_slides; }
    void setSlides(QList<Slide> slides);

    QStringList contents() { return m_contents; }
    QString slideTitle();
    QString slideText();
    QString slideImgPath();

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

    void setCurrentCenterPoint(QString objName) const;
    void setCurrentSkyDateTime(QDateTime dt) const;
    void setCurrentZoomFactor(double factor) const;
};

#endif // SKYGUIDEOBJECT_H
