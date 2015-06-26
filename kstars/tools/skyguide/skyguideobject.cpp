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

#include "skyguideobject.h"

SkyGuideObject::SkyGuideObject(const QVariantMap &map)
{
    // header fields
    m_title = map.value("title").toString();
    m_description = map.value("description").toString();
    m_language = map.value("language").toString();
    m_creationDate = map.value("creationDate").toDate();
    m_version = map.value("version").toInt();

    // authors
    if (map.contains("authors")) {
        foreach (const QVariant& author, map.value("authors").toList()) {
            QVariantMap amap = author.toMap();
            Author a;
            a.name = amap.value("name").toString();
            a.email = amap.value("email").toString();
            a.url = amap.value("url").toString();
            m_authors.append(a);
        }
    }

    // slides
    if (map.contains("slides")) {
        foreach (const QVariant& slide, map.value("slides").toList()) {
            QVariantMap smap = slide.toMap();
            Slide s;
            s.title = smap.value("title").toString();
            s.content = smap.value("content").toString();
            s.image = smap.value("image").toString();
            s.centerPoint = smap.value("centerPoint").toString();
            m_slides.append(s);
        }
    }
}
