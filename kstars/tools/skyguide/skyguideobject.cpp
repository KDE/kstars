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
}
