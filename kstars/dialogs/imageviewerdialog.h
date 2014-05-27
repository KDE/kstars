/***************************************************************************
                          astrophotographsbrowser.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2014/05/27
    copyright            : (C) 2014 by Vijay Dhameliya
    email                : vijay.atwork13@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef IMAGEVIEWERDIALOG_H
#define IMAGEVIEWERDIALOG_H

#include <QDialog>
#include <QGraphicsScene>
#include <QImage>

namespace Ui {
class ImageViewerDialog;
}

class ImageViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImageViewerDialog(const QString filePath, QWidget *parent = 0);
    ~ImageViewerDialog();

public slots:

    void wheelEvent(QWheelEvent* event);

    void invertColor();

private:
    Ui::ImageViewerDialog *ui;
    QGraphicsScene* m_Scene;
    const QString m_ImagePath;
    QImage* m_Image;
    QPixmap m_Pix;
};

#endif // IMAGEVIEWERDIALOG_H
