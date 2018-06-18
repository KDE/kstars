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
    virtual ~VideoWG() override = default;

    bool newFrame(IBLOB *bp);

    bool save(const QString &filename, const char *format);

    void setSize(uint16_t w, uint16_t h);

  protected:
    virtual void resizeEvent(QResizeEvent *ev) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *) override;

  signals:
    void newSelection(QRect);
    void imageChanged(std::unique_ptr<QImage> & frame);

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
