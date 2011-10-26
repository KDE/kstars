/***************************************************************************
                          slide.cpp  -  K Desktop Planetarium
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

#include "slide.h"

using namespace SkyGuides;

void Slide::setSlide(const QString &title, const QString &subtitle, const QString &text,
                     const QStringList &images, SkyPoint centerPoint)
{
    m_Title = title;
    m_Subtitle = subtitle;
    m_Text = text;
    m_Images = images;
    m_CenterPoint = centerPoint;
}
