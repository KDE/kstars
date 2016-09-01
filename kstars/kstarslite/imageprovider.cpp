/** *************************************************************************
                          imageprovider.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 22/07/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "imageprovider.h"

ImageProvider::ImageProvider()
    :QQuickImageProvider(QQmlImageProviderBase::Image)
{

}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    Q_UNUSED(size)
    Q_UNUSED(requestedSize)
    return images.value(id);
}

void ImageProvider::addImage(const QString &id, QImage image) {
    images.insert(id, image);
}
