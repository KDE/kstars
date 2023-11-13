/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "sensorgraphic.h"
#include <QPainter>
#include <QPen>
#include <QFileInfo>

namespace Ekos
{

SensorGraphic::SensorGraphic(const QWidget *parent, const int SW, const int SH, const int tileSize) :
    m_Parent(parent), m_SW(SW), m_SH(SH), m_tileSize(tileSize)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowStaysOnTopHint);

    // Scale the Picture to 100 pixels in the largest dimension
    m_SW = std::max(100, m_SW);
    m_SH = std::max(100, m_SH);
    double scaling = static_cast<double>(std::max(m_SW, m_SH)) / 100.0;
    m_SW /= scaling;
    m_SH /= scaling;
    m_tileSize /= scaling;

    // Check if the picture resource exists
    QFileInfo check_file(m_picturePath);
    if (check_file.exists() || check_file.isFile())
    {
        m_PW = m_SW * 1.35;
        m_PH = m_SH * 1.4;
    }
    else
    {
        m_PW = m_SW;
        m_PH = m_SH;
        m_picturePath = "";
    }
    resize(m_PW, m_PH);
}

SensorGraphic::~SensorGraphic()
{
}

void SensorGraphic::paintEvent(QPaintEvent *)
{
    // Draw the picture with the correct width / height aspect
    QRectF sensor(0.0, 0.0, m_PW, m_PH);
    QPainter painter(this);
    // Check if the picture resource exists; if so use it otherwise draw a black box
    if (m_picturePath.isEmpty())
        painter.fillRect(sensor, "black");
    else
        painter.drawImage(sensor, QImage(m_picturePath));

    // Setup the 9 rectangular tiles of the mosaic relative to sensor (later we will adjust this relative to the picture)
    QRectF tiles[NUM_TILES];
    tiles[0].setRect(0, 0, m_tileSize, m_tileSize);
    tiles[1].setRect((m_SW - m_tileSize) / 2, 0, m_tileSize, m_tileSize);
    tiles[2].setRect(m_SW - m_tileSize, 0, m_tileSize, m_tileSize);
    tiles[3].setRect(0, (m_SH - m_tileSize) / 2, m_tileSize, m_tileSize);
    tiles[4].setRect((m_SW - m_tileSize) / 2, (m_SH - m_tileSize) / 2, m_tileSize, m_tileSize);
    tiles[5].setRect(m_SW - m_tileSize, (m_SH - m_tileSize) / 2, m_tileSize, m_tileSize);
    tiles[6].setRect(0, m_SH - m_tileSize, m_tileSize, m_tileSize);
    tiles[7].setRect((m_SW - m_tileSize) / 2, m_SH - m_tileSize, m_tileSize, m_tileSize);
    tiles[8].setRect(m_SW - m_tileSize, m_SH - m_tileSize, m_tileSize, m_tileSize);

    // Setup an offset point to translate from sensor coordinates to picture coordinates
    QPointF offset = QPointF((m_PW - m_SW) / 2, (m_PH - m_SH) / 2);

    // Loop through each mosaic tile painting it appropriately
    for (int i = 0; i < NUM_TILES; i++)
    {
        // If not interested in this tile do nothing
        if (!m_useTile[i])
            continue;

        // Translate from sensor coordinates to picture coordinates
        tiles[i].translate(offset);

        // Paint the tile grey to start with
        painter.fillRect(tiles[i], "white");

        if (i == m_Highlight)
        {
            painter.setPen(TILE_COLOUR[m_Highlight]);
            painter.drawRect(tiles[i]);
        }
        else
            painter.setPen("black");

        painter.drawText(tiles[i], Qt::AlignCenter, TILE_NAME[i]);
    }
}

void SensorGraphic::setHighlight(int highlight)
{
    m_Highlight = highlight;
}

void SensorGraphic::setTiles(bool useTiles[NUM_TILES])
{
    for (int i = 0; i < NUM_TILES; i++)
        m_useTile[i] = useTiles[i];
}

}
