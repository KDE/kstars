/***************************************************************************
                          skyguide.cpp  -  K Desktop Planetarium
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

#include "guide.h"
#include "slide.h"

using namespace SkyGuides;

Guide::~Guide()
{
    qDeleteAll(m_Authors);
    qDeleteAll(m_Slides);
}

void Guide::setGuide(const QString &title, const QString &description, const QString &language, const QList<Author*> &authors,
                     const QString &thumbnailImg, const QDate &creationDate, const QString &version, const QList<Slide*> &slides)
{
    m_Title = title;
    m_Description = description;
    m_Language = language;
    m_Authors = authors;
    m_ThumbnailImage = thumbnailImg;
    m_CreationDate = creationDate;
    m_Version = version;
    m_Slides = slides;
}
