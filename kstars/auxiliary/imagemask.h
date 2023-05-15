/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
 */


#pragma once

#include <stdint.h>
#include <QRect>
#include <QVector>

class ImageMask
{
public:
    /**
     * @brief ImageMask Create an image mask with image dimensions
     * @param width image width
     * @param height image height
     */
    ImageMask(const uint16_t width=0, const uint16_t height=0);
    virtual ~ImageMask(){};

    /**
     * @brief setImageGeometry set the image geometry
     * @param width image width
     * @param height image height
     */
    virtual void setImageGeometry(const uint16_t width, const uint16_t height);

    /**
     * @brief isVisible test if the given position is visible
     * @return in this basic version, a point is visible as long as it lies
     * within the image dimensions
     */
    virtual bool isVisible(uint16_t posX, uint16_t posY);

    /**
     * @brief translate which position on the frame has a point after masking
     * @param original point to be translated
     * @return same position as on the original image
     */
    QPointF translate(QPointF original) { return original; }

    /**
     * @brief active is the mask active (pure virtual)
     */
    virtual bool active() = 0;

    uint16_t width() const;

    uint16_t height() const;

protected:
    uint16_t m_width, m_height;

    virtual void refresh() = 0;
};

class ImageRingMask : public ImageMask
{
public:
    /**
     * @brief ImageRingMask Create an image mask with a ring form
     * @param innerRadius inner ring radius in percent
     * @param outerRadius outer ring radius in percent
     * @param width image width
     * @param height image height
     */
    ImageRingMask(const float innerRadius=0, const float outerRadius=1, const uint16_t width=0, const uint16_t height=0);

    virtual ~ImageRingMask() {};

    /**
     * @brief isVisible test if the given position is visible
     * @return check if the position is inside the outer radius and outside the inner radius
     */
    bool isVisible(uint16_t posX, uint16_t posY) override;

    /**
     * @brief active The mask is active if either the inner radius > 0% or the outer radius < 100%
     */
    bool active() override { return m_width > 0 && m_height > 0 && (m_innerRadius > 0 || m_outerRadius < 1.0); };

    float innerRadius() const;
    void setInnerRadius(float newInnerRadius);

    float outerRadius() const;
    void setOuterRadius(float newOuterRadius);

protected:
    // mask radius in percent of image diagonal
    float m_innerRadius {0};
    float m_outerRadius {1.0};
    // cached values for fast visibility calculation
    long m_InnerRadiusSquare, m_OuterRadiusSquare;
    // re-calculate the cached squares
    virtual void refresh() override;
};

class ImageMosaicMask : public ImageMask
{
public:
    /**
     * @brief ImageMosaicMask Creates a 3x3 mosaic mask
     * @param tileWidth width of a single mosaic tile (in percent of the image width)
     * @param space space between the mosaic tiles
     * @param width source image width
     * @param height source image height
     */
    ImageMosaicMask (const uint16_t tileWidth, const uint16_t space, const uint16_t width=0, const uint16_t height=0);

    /**
     * @brief isVisible test if the given position is visible
     * @return check if the position is inside one of the tiles
     */
    virtual bool isVisible(uint16_t posX, uint16_t posY) override;

    /**
     * @brief translate Calculate the new position of a point inside of the mosaic
     * @param original original position
     * @return position inside the mosaic
     */
    QPointF translate(QPointF original);

    /**
     * @brief active The mask is active if the mosaic raster is present
     */
    bool active() override { return m_width > 0 && m_height > 0 && m_tiles.length() > 0; };

    const QVector<QRect> tiles();

    float tileWidth() const;
    void setTileWidth(float newTileWidth);

    uint16_t space() const;
    void setSpace(uint16_t newSpace);


protected:
    // width of a single mosaic tile (in percent of the image width)
    float m_tileWidth;
    // space between the mosaic tiles
    uint16_t m_space;
    // 3x3 mosaic raster, sorted linewise top down
    QVector<QRect> m_tiles;
    // re-calculate the tiles
    virtual void refresh() override;
};
