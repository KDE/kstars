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

class MosaicTile : public QGraphicsItem
{
public:
    MosaicTile(QGraphicsItem *parent);
    ~MosaicTile();

    void setPA(double positionAngle) { pa = positionAngle; }
    void setDimension(int width, int height) { w = width; h = height; }
    void setFOV(double fov_x, double fov_y) { fovW = fov_x; fovH = fov_y; }
    void setOverlap(double value) { overlap = value; }

    int getWidth() { return w;}
    int getHeight() { return h;}
    double getOverlap() { return overlap;}
    double getPA() { return pa; }

    void updateTiles();

protected:
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:

    double overlap;
    int w, h;
    double fovW, fovH;
    double pa;


    QBrush brush;
    QPen pen;

    QBrush textBrush;
    QPen textPen;

    struct OneTile
    {
        double x,y;
        double center_x, center_y;
        QGraphicsRectItem* rectItem;
    };

    QList<OneTile*> tiles;

    OneTile *getTile(int row, int col);

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



protected:
    virtual void resizeEvent(QResizeEvent *);

public slots:
    void constructMosaic();
    void calculateFOV();
    void updateTargetFOV();
    void drawOverlay();
    void resetFOV();
    void setPreset();

    /**
     * @short Re-renders the view
     */
    void render();

private:

    void createOverlay();
    Scheduler *ekosScheduler;
    SkyPoint center;
    QImage *m_skyChart;
    QImage *m_skyImage;

    QList<SkyPoint *> jobCenter;
    QPixmap targetPix;
    QGraphicsPixmapItem *targetItem;

    MosaicTile *mosaicTile;

    double pixelPerArcmin;

    QGraphicsScene scene;

};

}

#endif  // MOSAIC_H
