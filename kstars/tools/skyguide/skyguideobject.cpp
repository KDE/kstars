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

#include <QJsonObject>

#include "skyguideobject.h"

SkyGuideObject::SkyGuideObject(const QVariantMap &map)
    : m_isValid(false),
      m_currentSlide(0)
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
    foreach (const QVariant& slide, map.value("slides").toList()) {
        QVariantMap smap = slide.toMap();
        Slide s;
        s.title = smap.value("title").toString();
        s.text = smap.value("text").toString();
        s.image = smap.value("image").toString();
        s.centerPoint = smap.value("centerPoint").toString();
        m_contents.append(s.title);
        m_slides.append(s);
    }

    m_isValid = true;
}
