/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "imageprovider.h"

ImageProvider::ImageProvider() : QQuickImageProvider(QQmlImageProviderBase::Image)
{
}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(size)
    Q_UNUSED(requestedSize)
    return images.value(id);
}

void ImageProvider::addImage(const QString &id, QImage image)
{
    images.insert(id, image);
}
