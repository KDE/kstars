/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QPainter>

#include "mosaictilesmanager.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "ekos_scheduler_debug.h"

#include <QGraphicsSceneMouseEvent>

namespace Ekos
{
MosaicTilesManager::MosaicTilesManager(QWidget *parent) : QObject(parent), QGraphicsItem()
{
    brush.setStyle(Qt::NoBrush);
    QColor lightGray(200, 200, 200, 100);
    pen.setColor(lightGray);
    pen.setWidth(1);

    textBrush.setStyle(Qt::SolidPattern);
    textPen.setColor(Qt::red);
    textPen.setWidth(2);

    setFlags(QGraphicsItem::ItemIsMovable);
    setAcceptDrops(true);
}

MosaicTilesManager::~MosaicTilesManager()
{
    qDeleteAll(tiles);
}


void MosaicTilesManager::setSkyCenter(SkyPoint center)
{
    skyCenter = center;
}

void MosaicTilesManager::setPositionAngle(double positionAngle)
{
    pa = std::fmod(positionAngle * -1 + 360.0, 360.0);

    // Rotate the whole mosaic around its local center
    setTransformOriginPoint(QPointF());
    setRotation(pa);
}

void MosaicTilesManager::setGridDimensions(int width, int height)
{
    m_HorizontalTiles = width;
    m_VerticalTiles = height;
}

void MosaicTilesManager::setSingleTileFOV(double fov_x, double fov_y)
{
    fovW = fov_x;
    fovH = fov_y;
}

void MosaicTilesManager::setMosaicFOV(double mfov_x, double mfov_y)
{
    mfovW = mfov_x;
    mfovH = mfov_y;
}

void MosaicTilesManager::setOverlap(double value)
{
    overlap = (value < 0) ? 0 : (1 < value) ? 1 : value;
}


QSizeF MosaicTilesManager::adjustCoordinate(QPointF tileCoord)
{
    // Compute the declination of the tile row from the mosaic center
    double const dec = skyCenter.dec0().Degrees() + tileCoord.y() / m_PixelScale.height();

    // Adjust RA based on the shift in declination
    QSizeF const toSpherical(
        1 / (m_PixelScale.width() * cos(dec * dms::DegToRad)),
        1 / (m_PixelScale.height()));

    // Return the adjusted coordinates as a QSizeF in degrees
    return QSizeF(tileCoord.x() * toSpherical.width(), tileCoord.y() * toSpherical.height());
}

void MosaicTilesManager::updateTiles(QPointF skymapCenter, bool s_shaped)
{
    prepareGeometryChange();
    skymapCenter = QPointF(0, 0);

    qDeleteAll(tiles);
    tiles.clear();

    // Sky map has objects moving from left to right, so configure the mosaic from right to left, column per column

    // Offset is our tile size with an overlap removed
    double const xOffset = fovW * (1 - overlap);
    double const yOffset = fovH * (1 - overlap);

    // We start at top right corner, (0,0) being the center of the tileset
    double initX = +(fovW + xOffset * (m_HorizontalTiles - 1)) / 2.0 - fovW;
    double initY = -(fovH + yOffset * (m_VerticalTiles - 1)) / 2.0;

    double x = initX, y = initY;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mosaic Tile FovW" << fovW << "FovH" << fovH << "initX" << x << "initY" << y <<
                                   "Offset X " << xOffset << " Y " << yOffset << " rotation " << getPA() << " reverseOdd " << s_shaped;

    int index = 0;
    for (int col = 0; col < m_HorizontalTiles; col++)
    {
        y = (s_shaped && (col % 2)) ? (y - yOffset) : initY;

        for (int row = 0; row < m_VerticalTiles; row++)
        {
            OneTile *tile = new OneTile();

            if (!tile)
                continue;

            tile->pos.setX(x);
            tile->pos.setY(y);

            tile->center.setX(tile->pos.x() + (fovW / 2.0));
            tile->center.setY(tile->pos.y() + (fovH / 2.0));

            // The location of the tile on the sky map refers to the center of the mosaic, and rotates with the mosaic itself
            QPointF tileSkyLocation = skymapCenter - rotatePoint(tile->center, QPointF(), rotation());

            // Compute the adjusted location in RA/DEC
            QSizeF const tileSkyOffsetScaled = adjustCoordinate(tileSkyLocation);

            tile->skyCenter.setRA0((skyCenter.ra0().Degrees() + tileSkyOffsetScaled.width()) / 15.0);
            tile->skyCenter.setDec0(skyCenter.dec0().Degrees() + tileSkyOffsetScaled.height());
            tile->skyCenter.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());

            tile->rotation = tile->skyCenter.ra0().Degrees() - skyCenter.ra0().Degrees();

            // Large rotations handled wrong by the algorithm - prefer doing multiple mosaics
            if (abs(tile->rotation) <= 90.0)
            {
                tile->index = ++index;
                tiles.append(tile);
            }
            else
            {
                delete tile;
                tiles.append(nullptr);
            }

            y += (s_shaped && (col % 2)) ? -yOffset : +yOffset;
        }

        x -= xOffset;
    }
}

MosaicTilesManager::OneTile *MosaicTilesManager::getTile(int row, int col)
{
    int offset = row * m_HorizontalTiles + col;

    if (offset < 0 || offset >= tiles.size())
        return nullptr;

    return tiles[offset];
}

QRectF MosaicTilesManager::boundingRect() const
{
    const double xOffset = m_HorizontalTiles > 1 ? overlap : 0;
    const double yOffset = m_VerticalTiles > 1 ? overlap : 0;
    //    QRectF rect = QRectF(0, 0, fovW + xOffset * qMax(1, (m_HorizontalTiles - 1)), fovH + yOffset * qMax(1,
    //                         (m_VerticalTiles - 1)));
    const double xOverlapped = fovW * xOffset * m_HorizontalTiles;
    const double yOverlapped = fovH * yOffset * m_VerticalTiles;

    const double totalW = fovW * m_HorizontalTiles;
    const double totalH = fovH * m_VerticalTiles;
    QRectF rect = QRectF(-totalW / 2, -totalH / 2, totalW + xOverlapped, totalH + yOverlapped);
    return rect;
}

void MosaicTilesManager::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mousePressEvent(event);
    QPointF pos = event->screenPos();
    m_LastPosition = pos;
}

void MosaicTilesManager::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    QPointF delta = event->screenPos() - m_LastPosition;
    emit newOffset(delta);
}

void MosaicTilesManager::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (!tiles.size())
        return;

    QFont defaultFont = painter->font();
    QRect const oneRect(-fovW / 2, -fovH / 2, fovW, fovH);

    // HACK: all tiles should be QGraphicsItem instances so that texts would be scaled properly
    double const fontScale = 1 / log(tiles.size() < 4 ? 4 : tiles.size());

    // Draw a light background field first to help detect holes - reduce alpha as we are stacking tiles over this
    painter->setBrush(QBrush(QColor(255, 0, 0, (200 * m_PainterAlpha) / 100), Qt::SolidPattern));
    painter->setPen(QPen(painter->brush(), 2, Qt::PenStyle::DotLine));
    painter->drawRect(QRectF(QPointF(-mfovW / 2, -mfovH / 2), QSizeF(mfovW, mfovH)));

    // Fill tiles with a transparent brush to show overlaps
    QBrush tileBrush(QColor(0, 255, 0, (200 * m_PainterAlpha) / 100), Qt::SolidPattern);

    // Draw each tile, adjusted for rotation
    for (int row = 0; row < m_VerticalTiles; row++)
    {
        for (int col = 0; col < m_HorizontalTiles; col++)
        {
            OneTile const * const tile = getTile(row, col);
            if (tile)
            {
                QTransform const transform = painter->worldTransform();
                painter->translate(tile->center);
                painter->rotate(tile->rotation);

                painter->setBrush(tileBrush);
                painter->setPen(pen);

                painter->drawRect(oneRect);

                painter->setWorldTransform(transform);
            }
        }
    }

    // Overwrite with tile information
    for (int row = 0; row < m_VerticalTiles; row++)
    {
        for (int col = 0; col < m_HorizontalTiles; col++)
        {
            OneTile const * const tile = getTile(row, col);
            if (tile)
            {
                QTransform const transform = painter->worldTransform();
                painter->translate(tile->center);
                painter->rotate(tile->rotation);

                painter->setBrush(textBrush);
                painter->setPen(textPen);

                defaultFont.setPointSize(50 * fontScale);
                painter->setFont(defaultFont);
                painter->drawText(oneRect, Qt::AlignRight | Qt::AlignTop, QString("%1.").arg(tile->index));

                defaultFont.setPointSize(20 * fontScale);
                painter->setFont(defaultFont);
                painter->drawText(oneRect, Qt::AlignHCenter | Qt::AlignVCenter, QString("%1\n%2")
                                  .arg(tile->skyCenter.ra0().toHMSString())
                                  .arg(tile->skyCenter.dec0().toDMSString()));
                painter->drawText(oneRect, Qt::AlignHCenter | Qt::AlignBottom, QString("%1%2°")
                                  .arg(tile->rotation >= 0.01 ? '+' : tile->rotation <= -0.01 ? '-' : '~')
                                  .arg(abs(tile->rotation), 5, 'f', 2));

                painter->setWorldTransform(transform);
            }
        }
    }
}

QPointF MosaicTilesManager::rotatePoint(QPointF pointToRotate, QPointF centerPoint, double paDegrees)
{
    double angleInRadians = paDegrees * dms::DegToRad;
    double cosTheta       = cos(angleInRadians);
    double sinTheta       = sin(angleInRadians);

    QPointF rotation_point;

    rotation_point.setX((cosTheta * (pointToRotate.x() - centerPoint.x()) -
                         sinTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.x()));
    rotation_point.setY((sinTheta * (pointToRotate.x() - centerPoint.x()) +
                         cosTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.y()));

    return rotation_point;
}
}
