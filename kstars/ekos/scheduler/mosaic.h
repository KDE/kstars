/*  Ekos Mosaic Tool
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef MOSAIC_H
#define MOSAIC_H

#include <QDialog>
#include <QGraphicsItem>

#include "skypoint.h"

#include "ui_mosaic.h"

namespace Ekos
{
class Scheduler;

typedef struct
{
    QPointF pos;
    QPointF center;
    QPointF center_rot;
    SkyPoint skyCenter;
} OneTile;

class MosaicTile : public QGraphicsItem
{
  public:
    MosaicTile();
    ~MosaicTile();

    void setPA(double positionAngle) { pa = positionAngle * -1; }
    void setDimension(int width, int height)
    {
        w = width;
        h = height;
    }
    void setFOV(double fov_x, double fov_y)
    {
        fovW = fov_x;
        fovH = fov_y;
    }
    void setOverlap(double value) { overlap = value; }

    int getWidth() { return w; }
    int getHeight() { return h; }
    double getOverlap() { return overlap; }
    double getPA() { return pa; }
    double getFOVW() { return fovW; }
    double getFOVH() { return fovH; }

    void updateTiles();
    OneTile *getTile(int row, int col);

    QRectF boundingRect() const;

    QPointF getTileCenter(int row, int col);

    QList<OneTile *> getTiles() const;

  protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);

  private:
    double overlap;
    int w, h;
    double fovW, fovH;
    double pa;

    QBrush brush;
    QPen pen;

    QBrush textBrush;
    QPen textPen;

    QList<OneTile *> tiles;

    QPointF rotatePoint(QPointF pointToRotate, QPointF centerPoint);
};

class Mosaic : public QDialog, public Ui::mosaicDialog
{
    Q_OBJECT

  public:
    Mosaic(Scheduler *scheduler);
    ~Mosaic();

    void setCameraSize(uint16_t width, uint16_t height);
    void setPixelSize(double pixelWSize, double pixelHSize);
    void setFocalLength(double focalLength);
    void setCenter(const SkyPoint &value);

    QString getJobsDir() { return jobsDir->text(); }
    QList<OneTile *> getJobs() { return mosaicTile->getTiles(); }

  protected:
    virtual void showEvent(QShowEvent *);
    virtual void resizeEvent(QResizeEvent *);

  public slots:

    void constructMosaic();

    void calculateFOV();

    void updateTargetFOV();

    void saveJobsDirectory();

    void resetFOV();

    void createJobs();

    void render();

  private:
    QPointF rotatePoint(QPointF pointToRotate, QPointF centerPoint);

    Scheduler *ekosScheduler;
    SkyPoint center;
    QImage *m_skyChart;
    QImage *m_skyImage;

    QPixmap targetPix;
    QGraphicsPixmapItem *targetItem;

    MosaicTile *mosaicTile;

    double pixelPerArcmin;

    QPointF screenPoint;

    QGraphicsScene scene;
};
}

#endif // MOSAIC_H
