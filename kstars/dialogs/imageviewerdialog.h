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

    enum DrawType { None = 0, Text = 1, Circle = 2 };

public slots:

    virtual void wheelEvent(QWheelEvent * event);

    virtual void mousePressEvent(QMouseEvent * event);

    //virtual void mouseMoveEvent(QMouseEvent * event);

    void zoomInImage();

    void zoomOutImage();

    void drawTextClicked();

    void drawCircleClicked();

    void undoClicked();

    void invertColor();

    void saveImage();

private:
    Ui::ImageViewerDialog *ui;
    QGraphicsScene* m_Scene;
    const QString m_ImagePath;
    int drawType;
    QImage* m_Image;
    QPixmap m_Pix;
    QPointF clickedPoint;
    QList<QGraphicsItem *> itemsList;
};

#endif // IMAGEVIEWERDIALOG_H
