/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAGEPROVIDER_H_
#define IMAGEPROVIDER_H_

#include <QQuickImageProvider>

/**
 * @class ImageProvider
 * This class makes it possible to use QImages from C++ in QML
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class ImageProvider : public QQuickImageProvider
{
    public:
        ImageProvider();
        /** @short Get image by id
             *  @return image of size requestedSize
             **/
        virtual QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
        /**
             * @short Add image to the list of images with the given id
             */
        void addImage(const QString &id, QImage image);

    private:
        QHash<QString, QImage> images;
};
#endif
