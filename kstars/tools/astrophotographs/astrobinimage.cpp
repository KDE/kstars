/***************************************************************************
                          astrobinimage.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Aug 8 2012
    copyright            : (C) 2012 by Lukasz Jaskiewicz and Rafal Kulaga
    email                : lucas.jaskiewicz@gmail.com, rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "astrobinimage.h"

QUrl AstroBinImage::downloadUrl() const
{
    return QUrl("http://cdn.astrobin.com/images/" + fileName() + originalFileExtension());
}

QUrl AstroBinImage::downloadResizedUrl() const
{
    return QUrl("http://cdn.astrobin.com/images/" + fileName() + "_resized" + originalFileExtension());
}

QUrl AstroBinImage::downloadThumbnailUrl() const
{
    return QUrl("http://cdn.astrobin.com/images/" + fileName() + "_thumb.png");
}

QUrl AstroBinImage::regularImageUrl() const
{
    return QUrl(m_RegularImageUrl);
}

QUrl AstroBinImage::thumbImageUrl() const
{
    return QUrl(m_ThumbImageUrl);
}
