/*  Video Stream Frame
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#pragma once

#include <indidevapi.h>

#include <QPixmap>
#include <QVector>
#include <QColor>
#include <QLabel>

#include <memory>

class QImage;
class QRubberBand;

class VideoWG : public QLabel
{
    Q_OBJECT

  public:
    explicit VideoWG(QWidget *parent = nullptr);
    ~VideoWG();

    bool newFrame(IBLOB *bp);

    bool save(const QString &filename, const char *format);

    void setSize(uint16_t w, uint16_t h);

  protected:
    virtual void resizeEvent(QResizeEvent *ev);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *);

  signals:
    void newSelection(QRect);

  private:
    uint16_t streamW { 0 };
    uint16_t streamH { 0 };
    uint32_t totalBaseCount { 0 };
    QVector<QRgb> grayTable;
    std::unique_ptr<QImage> streamImage;
    QPixmap kPix;
    QRubberBand *rubberBand { nullptr };
    QPoint origin;
};
