/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "aberrationinspectorutils.h"
#include <QtWidgets/QWidget>

// The SensorGraphic class renders a graphic of a camera sensor with the mosaic tiles drawn on it.
// The tiles occupy the correct proportion and position of the sensor.
// Each tile is labelled appropriately, e.g. T = top, TL = top left.
// There is a highlight method where a single tile is highlighted in its appropriate colour.
//
// SensorGraphic behaves like a tooltip. Tooltips seem to only allow text. This class creates a popup window.
// It is a subclass of QWidget where the paintEvent method has been reimplemented.
//
namespace Ekos
{

class SensorGraphic : public QWidget
{
    public:
        /**
         * @brief create a SensorGraphic with the associated dimensions
         * @param parent widget
         * @param sensor width (pixels)
         * @param sensor height (pixels)
         * @param tile size
         */
        SensorGraphic(const QWidget *parent, const int SW, const int SH, const int tileSize);
        ~SensorGraphic();

        /**
         * @brief set the tile to highlight
         * @param highlight
         */
        void setHighlight(int highlight);

        /**
         * @brief set the tiles to use
         * @param useTile array
         */
        void setTiles(bool useTile[NUM_TILES]);

    protected:
        /**
         * @brief implemented paintEvent
         * @param paint event
         */
        void paintEvent(QPaintEvent *) override;

    private:
        const QWidget * m_Parent;
        int m_Highlight { 0 };

        // Picture variables
        int m_PW { 0 };
        int m_PH { 0 };

        // Sensor variables
        int m_SW { 0 };
        int m_SH { 0 };
        int m_tileSize { 0 };

        // Path to picture resource
        QString m_picturePath { ":/icons/sensorgraphic.png" };

        bool m_useTile[NUM_TILES] = { true, true, true, true, true, true, true, true, true };
};

}
