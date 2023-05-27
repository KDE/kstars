/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "imagemask.h"
#include "cmath"

ImageMask::ImageMask(const uint16_t width, const uint16_t height)
{
    m_width  = width;
    m_height = height;
}

void ImageMask::setImageGeometry(const uint16_t width, const uint16_t height)
{
    m_width  = width;
    m_height = height;
    refresh();
}

bool ImageMask::isVisible(uint16_t posX, uint16_t posY)
{
    return posX >= 0 && posX < m_width && posY >= 0 && posY < m_height;
}

uint16_t ImageMask::width() const
{
    return m_width;
}

uint16_t ImageMask::height() const
{
    return m_height;
}

ImageRingMask::ImageRingMask(const float innerRadius, const float outerRadius, const uint16_t width,
                             const uint16_t height) : ImageMask(width, height)
{
    m_innerRadius = innerRadius;
    m_outerRadius = outerRadius;
}

void ImageRingMask::refresh()
{
    long const sqDiagonal = (long) (m_width * m_width / 4 + m_height * m_height / 4);
    m_InnerRadiusSquare = std::lround(sqDiagonal * innerRadius() * innerRadius());
    m_OuterRadiusSquare = std::lround(sqDiagonal * outerRadius() * outerRadius());
}

bool ImageRingMask::isVisible(uint16_t posX, uint16_t posY)
{
    bool result = ImageMask::isVisible(posX, posY);
    // outside of the image?
    if (result == false)
        return false;

    double const x = posX - m_width / 2;
    double const y = posY - m_height / 2;

    double const sqRadius = x * x + y * y;

    return sqRadius >= m_InnerRadiusSquare && sqRadius < m_OuterRadiusSquare;
}

float ImageRingMask::innerRadius() const
{
    return m_innerRadius;
}

void ImageRingMask::setInnerRadius(float newInnerRadius)
{
    m_innerRadius = newInnerRadius;
    refresh();
}

float ImageRingMask::outerRadius() const
{
    return m_outerRadius;
}

void ImageRingMask::setOuterRadius(float newOuterRadius)
{
    m_outerRadius = newOuterRadius;
    refresh();
}

ImageMosaicMask::ImageMosaicMask(const uint16_t tileWidth, const uint16_t space, const uint16_t width,
                                 const uint16_t height) : ImageMask(width, height)
{
    m_tileWidth = tileWidth;
    m_space = space;
    // initialize the tiles
    for (int i = 0; i < 9; i++)
        m_tiles.append(QRect());
}

bool ImageMosaicMask::isVisible(uint16_t posX, uint16_t posY)
{
    for (auto it = m_tiles.begin(); it != m_tiles.end(); it++)
        if (it->contains(posX, posY))
            return true;
    // no matching tile found
    return false;
}

QPointF ImageMosaicMask::translate(QPointF original)
{
    int pos = 0;
    const auto tileWidth = std::lround(m_width * m_tileWidth / 100);
    const float spacex = (m_width - 3 * tileWidth - 2 * m_space) / 2;
    const float spacey = (m_height - 3 * tileWidth - 2 * m_space) / 2;
    for (QRect tile : m_tiles)
    {
        // matrix tile position
        int posx = pos % 3;
        int posy = pos++ / 3;
        if (tile.contains(original.x(), original.y()))
            return QPointF(original.x() - posx * spacex, original.y() - posy * spacey);
    }
    // this should not happen for filtered positions
    return QPointF(-1, -1);
}

const QVector<QRect> ImageMosaicMask::tiles()
{
    return m_tiles;
}

float ImageMosaicMask::tileWidth() const
{
    return m_tileWidth;
}

void ImageMosaicMask::setTileWidth(float newTileWidth)
{
    m_tileWidth = newTileWidth;
    refresh();
}

uint16_t ImageMosaicMask::space() const
{
    return m_space;
}

void ImageMosaicMask::setSpace(uint16_t newSpace)
{
    m_space = newSpace;
    refresh();
}

void ImageMosaicMask::refresh()
{
    m_tiles.clear();
    uint16_t tileWidth = std::lround(m_width * m_tileWidth / 100);
    if (m_width > 0 && m_height > 0)
    {
        const auto x1 = std::lround((m_width - tileWidth) / 2);
        const auto x2 = m_width - tileWidth - 1;
        const auto y1 = std::lround((m_height - tileWidth) / 2);
        const auto y2 = m_height - tileWidth - 1;

        m_tiles.append(QRect(0, 0, tileWidth, tileWidth));
        m_tiles.append(QRect(x1, 0, tileWidth, tileWidth));
        m_tiles.append(QRect(x2, 0, tileWidth, tileWidth));
        m_tiles.append(QRect(0, y1, tileWidth, tileWidth));
        m_tiles.append(QRect(x1, y1, tileWidth, tileWidth));
        m_tiles.append(QRect(x2, y1, tileWidth, tileWidth));
        m_tiles.append(QRect(0, y2, tileWidth, tileWidth));
        m_tiles.append(QRect(x1, y2, tileWidth, tileWidth));
        m_tiles.append(QRect(x2, y2, tileWidth, tileWidth));
    }
}
