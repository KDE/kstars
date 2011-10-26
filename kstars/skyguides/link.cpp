/***************************************************************************
                          link.cpp  -  K Desktop Planetarium
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

#include "link.h"

using namespace SkyGuides;

void Link::setLink(const QString &label, const QString &description, const QString &url)
{
    m_Label = label;
    m_Description = description;
    m_Url = url;
}
