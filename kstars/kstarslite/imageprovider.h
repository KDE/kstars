/** *************************************************************************
                          imageprovider.h  -  K Desktop Planetarium
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
#ifndef IMAGEPROVIDER_H_
#define IMAGEPROVIDER_H_

#include <QQuickImageProvider>

    /**
     * @class ImageProvider
     * This class makes it possible to use QImages in QML
     *
     * @author Artem Fedoskin
     * @version 1.0
     */
class ImageProvider : public QQuickImageProvider
{
public:
    ImageProvider();
    virtual QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
    void addImage(const QString &id, QImage image);
private:
    QHash<QString, QImage> images;
};
#endif


