/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QGraphicsItem>
#include <QBrush>
#include <QPen>

#include "skypoint.h"

namespace Ekos
{

class MosaicTilesManager : public QObject, public QGraphicsItem
{
    Q_OBJECT

    public:
        // TODO: make this struct a QGraphicsItem
        typedef struct
        {
            QPointF pos;
            QPointF center;
            SkyPoint skyCenter;
            double rotation;
            int index;
        } OneTile;

    public:
        MosaicTilesManager(QWidget *parent = nullptr);
        ~MosaicTilesManager();

    public:
        void setSkyCenter(SkyPoint center);
        void setPositionAngle(double positionAngle);
        void setGridDimensions(int width, int height);
        void setSingleTileFOV(double fov_x, double fov_y);
        void setMosaicFOV(double mfov_x, double mfov_y);
        void setOverlap(double value);
        void setPixelScale(const QSizeF &scale) {m_PixelScale = scale;}

    public:
        int getWidth()
        {
            return m_HorizontalTiles;
        }

        int getHeight()
        {
            return m_VerticalTiles;
        }

        double getOverlap()
        {
            return overlap;
        }

        double getPA()
        {
            return pa;
        }

        void setPainterAlpha(int v)
        {
            m_PainterAlpha = v;
        }

    public:
        /// @internal Returns scaled offsets for a pixel local coordinate.
        ///
        /// This uses the mosaic center as reference and the argument resolution of the sky map at that center.
        QSizeF adjustCoordinate(QPointF tileCoord);
        virtual QRectF boundingRect() const override;
        void updateTiles(QPointF skymapCenter, bool s_shaped);
        OneTile *getTile(int row, int col);

        QList<OneTile *> getTiles() const
        {
            return tiles;
        }

    protected:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;
        QPointF rotatePoint(QPointF pointToRotate, QPointF centerPoint, double paDegrees);

    signals:
        void newOffset(const QPointF &offset);

    private:
        SkyPoint skyCenter;
        double overlap { 0 };
        uint8_t m_HorizontalTiles { 1 };
        uint8_t m_VerticalTiles { 1 };
        double fovW { 0 };
        double fovH { 0 };
        double mfovW { 0 };
        double mfovH { 0 };
        double pa { 0 };

        QSizeF m_PixelScale;
        QBrush brush;
        QPen pen;

        QBrush textBrush;
        QPen textPen;

        int m_PainterAlpha { 50 };

        QPointF m_LastPosition;
        QList<OneTile *> tiles;
};

}
