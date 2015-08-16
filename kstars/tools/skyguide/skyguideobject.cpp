/***************************************************************************
                          skyguideobject.cpp  -  K Desktop Planetarium
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

#include <QtDebug>
#include <QJsonObject>
#include <kzip.h>

#include "kstarsdata.h"
#include "Options.h"
#include "skyguidemgr.h"
#include "skyguideobject.h"
#include "skymap.h"
#include "skycomponents/starcomponent.h"
#include "skycomponents/skymapcomposite.h"

SkyGuideObject::SkyGuideObject(const QString &path, const QVariantMap &map)
        : m_isValid(false)
        , m_path(path)
        , m_currentSlide(-1)
{
    if (map["formatVersion"].toInt() != SKYGUIDE_FORMAT_VERSION
        || !map.contains("header") || !map.contains("slides"))
    {
        // invalid!
        return;
    }

    // header fields
    QVariantMap headerMap = map.value("header").toMap();
    m_title = headerMap.value("title").toString();
    m_description = headerMap.value("description").toString();
    m_language = headerMap.value("language").toString();
    m_creationDate = headerMap.value("creationDate").toDate();
    m_version = headerMap.value("version").toInt();
    m_authors = headerMap.value("authors").toList();

    // slides
    QList<Slide> slides;
    foreach (const QVariant& slide, map.value("slides").toList()) {
        QVariantMap smap = slide.toMap();
        Slide s;
        s.title = smap.value("title").toString();
        s.text = smap.value("text").toString();
        s.image = smap.value("image").toString();
        s.centerPoint = smap.value("centerPoint").toString();
        s.skyDateTime = smap.value("skyDateTime").toDateTime();
        s.zoomFactor = smap.value("zoomFactor").toDouble();
        slides.append(s);
    }
    setSlides(slides);

    m_isValid = true;
}

void SkyGuideObject::setSlides(QList<Slide> slides) {
    m_contents.clear();
    int i = 1;
    foreach (Slide s, slides) {
        m_contents.append(QString("%1 - %2").arg(i).arg(s.title));
        ++i;
    }
    m_slides = slides;
}

void SkyGuideObject::setCurrentCenterPoint(QString objName) const {
    SkyObject* obj = KStarsData::Instance()->skyComposite()->findByName(objName);

    // is it a HD star?
    if(!obj && objName.startsWith("HD")) {
        objName.remove("HD");
        int hd = objName.toInt();
        if(hd) {
            obj = StarComponent::Instance()->findByHDIndex(hd);
        }
    }

    // nothing found?
    if (!obj) {
        QString message = "The current SkyGuide tried to select a non-existent "
                          "object named \"" + objName + "\"\n";
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideObject: " << message;
        return;
    }

    if (Options::isTracking()) {
        KStars::Instance()->slotTrack(); // call it once to untrack
    }

    KStars::Instance()->map()->setDestination(*obj);
    KStars::Instance()->map()->setClickedObject(obj);
    KStars::Instance()->map()->setFocusObject(obj);
    KStars::Instance()->slotTrack();
}

void SkyGuideObject::setCurrentZoomFactor(double factor) const {
    if (factor) {
        KStars::Instance()->map()->setZoomFactor(factor);
    }
}

void SkyGuideObject::setCurrentSkyDateTime(QDateTime dt) const {
    if (dt.isValid()) {
        KStars::Instance()->data()->changeDateTime(dt);
    }
}

void SkyGuideObject::setCurrentSlide(int slide) {
    if (slide > -1) {
        setCurrentSkyDateTime(m_slides.at(slide).skyDateTime);
        setCurrentCenterPoint(m_slides.at(slide).centerPoint);
        setCurrentZoomFactor(m_slides.at(slide).zoomFactor);
    }
    m_currentSlide = slide;
}

QString SkyGuideObject::slideTitle() {
    if (m_currentSlide == -1) {
        return "";
    }
    return m_slides.at(m_currentSlide).title;
}

QString SkyGuideObject::slideText() {
    if (m_currentSlide == -1) {
        return "";
    }
    return m_slides.at(m_currentSlide).text;
}

QString SkyGuideObject::slideImgPath() {
    if (m_currentSlide == -1) {
        return "";
    }
    return m_path + QDir::toNativeSeparators("/" + m_slides.at(m_currentSlide).image);
}

SkyGuideObject::Author SkyGuideObject::authorFromQVariant(QVariant var) {
    QVariantMap map = var.toMap();
    Author author;
    author.name = map.value("name").toString();
    author.email = map.value("email").toString();
    author.url = map.value("url").toString();
    return author;
}

QVariant SkyGuideObject::authorToQVariant(Author author) {
    QVariantMap map;
    map.insert("name", author.name);
    map.insert("email", author.email);
    map.insert("url", author.url);
    return map;
}

QJsonDocument SkyGuideObject::toJsonDocument() {
    QJsonArray authors;
    foreach (QVariant a, m_authors) {
        authors.append(QJsonObject::fromVariantMap(a.toMap()));
    }

    QJsonObject header;
    header.insert("title", m_title);
    header.insert("description", m_description);
    header.insert("language", m_language);
    header.insert("creationDate", m_creationDate.toString(Qt::ISODate));
    header.insert("version", m_version);
    header.insert("authors", authors);

    QJsonArray slides;
    foreach (Slide s, m_slides) {
        QJsonObject slide;
        slide.insert("title", s.title);
        slide.insert("image", s.image);
        slide.insert("text", s.text);
        slide.insert("centerPoint", s.centerPoint);
        slide.insert("skyDateTime", s.skyDateTime.toString());
        slide.insert("zoomFactor", s.zoomFactor);
        slides.append(slide);
    }

    QJsonObject jsonObj;
    jsonObj.insert("formatVersion", SKYGUIDE_FORMAT_VERSION);
    jsonObj.insert("header", header);
    jsonObj.insert("slides", slides);

    return QJsonDocument(jsonObj);
}

QString SkyGuideObject::toZip() {
    // Create a zip archive
    QString tmpZipPath = QDir::tempPath() + "/" + m_title + ".zip";
    QFile(tmpZipPath).remove();
    KZip archive(tmpZipPath);
    if (!archive.open(QIODevice::WriteOnly)) {
        QString message = "Unable to create a temporary zip archive!\n" + tmpZipPath;
        KMessageBox::sorry(0, message, "Warning!");
        qWarning() << "SkyGuideObject: " << message;
        return "";
    }

    mode_t perm = 0100644;

    // copy all images
    const int count = m_slides.size();
    for (int i = 0; i < count; ++i) {
        SkyGuideObject::Slide s = m_slides.at(i);
        if (s.image.isEmpty()) {
            continue;
        }

        QString imgPath;
        if (s.image.contains(QDir::separator())) {
            imgPath = s.image;
        } else {
            imgPath = m_path + "/" + s.image;
        }

        QFile img(imgPath);
        if (!img.open(QIODevice::ReadOnly)) {
            QString message = "Unable to read image file!\n" + imgPath;
            KMessageBox::sorry(0, message, "Warning!");
            qWarning() << "SkyGuideObject: " << message;
            continue;
        }

        // fix image name
        s.image = QString("img%1_%2").arg(i).arg(s.image.split(QDir::separator()).last());
        m_slides.replace(i, s);

        archive.writeFile(s.image, img.readAll(), perm);
        i++;
    }

    // creates the guide.json file
    archive.writeFile(JSON_NAME, toJsonDocument().toJson(), perm);

    archive.close();

    return tmpZipPath;
}
