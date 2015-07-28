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

#include "kstarsdata.h"
#include "skyguideobject.h"
#include "skymap.h"
#include "skycomponents/starcomponent.h"
#include "skycomponents/skymapcomposite.h"

SkyGuideObject::SkyGuideObject(const QString &path, const QVariantMap &map)
    : m_isValid(false),
      m_path(path),
      m_currentSlide(-1)
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

    // authors
    if (headerMap.contains("authors")) {
        foreach (const QVariant& author, headerMap.value("authors").toList()) {
            QVariantMap amap = author.toMap();
            Author a;
            a.name = amap.value("name").toString();
            a.email = amap.value("email").toString();
            a.url = amap.value("url").toString();
            m_authors.append(a);
        }
    }

    // slides
    int i = 1;
    foreach (const QVariant& slide, map.value("slides").toList()) {
        QVariantMap smap = slide.toMap();
        Slide s;
        s.title = smap.value("title").toString();
        s.text = smap.value("text").toString();
        s.image = smap.value("image").toString();
        s.centerPoint = smap.value("centerPoint").toString();
        s.zoomFactor = smap.value("zoomFactor").toDouble();
        m_contents.append(QString("%1 - %2").arg(i).arg(s.title));
        m_slides.append(s);
        i++;
    }

    m_isValid = true;
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
        qWarning() << "SkyGuideObject: No object named " + objName + " found.";
        return;
    }

    KStars::Instance()->map()->setDestination(*obj);
    KStars::Instance()->map()->setFocusObject(obj);
    KStars::Instance()->slotTrack();
}

void SkyGuideObject::setCurrentZoomFactor(double factor) const {
    if (factor) {
        KStars::Instance()->map()->setZoomFactor(factor);
    }
}

void SkyGuideObject::setCurrentSlide(int slide) {
    if (slide > -1) {
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
