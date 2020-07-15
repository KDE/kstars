/*  Ekos Mosaic Tool
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "skypoint.h"
#include "ui_mosaic.h"

#include <QGraphicsItem>
#include <QDialog>

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

        void setPA(double positionAngle)
        {
            pa = positionAngle * -1;
        }
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
        void setOverlap(double value)
        {
            overlap = value;
        }

        int getWidth()
        {
            return w;
        }
        int getHeight()
        {
            return h;
        }
        double getOverlap()
        {
            return overlap;
        }
        double getPA()
        {
            return pa;
        }

        void updateTiles();
        OneTile *getTile(int row, int col);

        QRectF boundingRect() const override;

        QList<OneTile *> getTiles() const;

    protected:
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

    private:
        QPointF rotatePoint(QPointF pointToRotate, QPointF centerPoint);

        double overlap { 0 };
        int w { 1 };
        int h { 1 };
        double fovW { 0 };
        double fovH { 0 };
        double pa { 0 };

        QBrush brush;
        QPen pen;

        QBrush textBrush;
        QPen textPen;

        QList<OneTile *> tiles;
};

class Mosaic : public QDialog, public Ui::mosaicDialog
{
        Q_OBJECT

    public:
        Mosaic();
        ~Mosaic() override;

        void setCameraSize(uint16_t width, uint16_t height);
        void setPixelSize(double pixelWSize, double pixelHSize);
        void setFocalLength(double focalLength);
        void setCenter(const SkyPoint &value);

        QString getJobsDir()
        {
            return jobsDir->text();
        }
        QList<OneTile *> getJobs()
        {
            return mosaicTileItem->getTiles();
        }

    protected:
        virtual void showEvent(QShowEvent *) override;
        virtual void resizeEvent(QResizeEvent *) override;

    public slots:

        void constructMosaic();

        void calculateFOV();

        void updateTargetFOV();

        void saveJobsDirectory();

        void resetFOV();

        void createJobs();

        void render();

        void fetchINDIInformation();

    private:
        SkyPoint center;
        QImage *m_skyChart { nullptr };

        QPixmap targetPix;
        QGraphicsPixmapItem *skyMapItem { nullptr };

        MosaicTile *mosaicTileItem { nullptr };

        double pixelsPerArcminRA { 0 }, pixelsPerArcminDE;

        QPointF screenPoint;
        QGraphicsScene scene;

        bool rememberAltAzOption;
};
}
