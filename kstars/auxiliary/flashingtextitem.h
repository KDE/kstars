/*  Text item that flashes up in a big bold font
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>, 2025

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "qcustomplot.h"
#include <QTimer>
#include <QVariantAnimation>

/**
 * @brief A QCPItemText that briefly shows in a big font and then
 *        switches back to a normal font.
 */
class FlashingTextItem : public QCPItemText
{
    public:
        explicit FlashingTextItem(QCustomPlot *parentPlot);

        // big overlay appears, then shrinks into the small base text
        void setText(const QString &text, int holdMs = 3000, int shrinkMs = 600);

        // Font sizing: either fixed pixels or autoscaled from plot’s min(width,height)
        void setBigPixelSize(int px)
        {
            m_bigPx = px;
            m_useAutoScale = false;
        }
        void setNormalPixelSize(int px)
        {
            m_normalPx = px;
            m_useAutoScale = false;
        }

        // Call this from your plot’s resizeEvent if autoscaling is enabled
        void updateFontSizesFromPlot(QCustomPlot *plot);

        // Optional styling knobs
        void setBigBold(bool on)
        {
            m_bigBold = on;
        }
        void setNormalBold(bool on)
        {
            m_normalBold = on;
        }

    private:
        void applyBigFont();
        void applyNormalFont();
        void ensureOverlayCreated();
        void startShrinkAnimation(int shrinkMs);

        QTimer            m_timer;      // Timer to switch back
        QTimer            m_holdTimer;  // wait before shrinking
        QVariantAnimation m_flashAnim;  // fade 1.0 -> 0.0

        QCPItemText *m_overlay {nullptr}; // transient big text

        // Sizing
        bool   m_useAutoScale {true};
        int    m_bigPx        {28};     // used when !m_useAutoScale
        int    m_normalPx     {12};     // used when !m_useAutoScale
        double m_bigRatio     {0.18};   // used when m_useAutoScale
        double m_normalRatio  {0.06};   // used when m_useAutoScale

        // Style
        bool   m_bigBold      {true};
        bool   m_normalBold   {false};
};
