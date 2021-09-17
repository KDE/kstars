/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "legend.h"

#include "colorscheme.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyqpainter.h"
#include "Options.h"

#include <QBrush>

namespace
{
const int symbolSize      = 15;
const int bRectWidth      = 100;
const int bRectHeight     = 45;
const int maxHScalePixels = 200;
const int maxVScalePixels = 100;
const int xSymbolSpacing  = 100;
const int ySymbolSpacing  = 70;
}

Legend::Legend(LEGEND_ORIENTATION orientation, LEGEND_POSITION pos)
    : m_SkyMap(SkyMap::Instance()), m_Orientation(orientation),
      m_Position(pos), m_PositionFloating(QPoint(0, 0)), m_cScheme(KStarsData::Instance()->colorScheme()),
      m_SymbolSize(symbolSize), m_BRectWidth(bRectWidth), m_BRectHeight(bRectHeight),
      m_MaxHScalePixels(maxHScalePixels), m_MaxVScalePixels(maxVScalePixels), m_XSymbolSpacing(xSymbolSpacing),
      m_YSymbolSpacing(ySymbolSpacing)
{
    m_BgColor = m_cScheme->colorNamed("SkyColor");
}

Legend::~Legend()
{
    if (m_Painter != nullptr && m_DeletePainter)
    {
        delete m_Painter;
    }
}

QSize Legend::calculateSize()
{
    int width  = 0;
    int height = 0;

    switch (m_Orientation)
    {
        case LO_HORIZONTAL:
        {
            switch (m_Type)
            {
                case LT_SCALE_ONLY:
                {
                    width  = 40 + m_MaxHScalePixels;
                    height = 60;
                    break;
                }

                case LT_MAGNITUDES_ONLY:
                {
                    width  = 140;
                    height = 70;
                    break;
                }

                case LT_SYMBOLS_ONLY:
                {
                    width  = 7 * m_XSymbolSpacing;
                    height = 20 + m_SymbolSize + m_BRectHeight;
                    break;
                }

                case LT_SCALE_MAGNITUDES:
                {
                    width  = 160 + m_MaxHScalePixels;
                    height = 70;
                    break;
                }

                case LT_FULL:
                {
                    width  = 7 * m_XSymbolSpacing;
                    height = 20 + m_SymbolSize + m_BRectHeight + 70;
                    break;
                }

                default:
                    break; // should never happen
            }
        }

        break;

        case LO_VERTICAL:
        {
            switch (m_Type)
            {
                case LT_SCALE_ONLY:
                {
                    width  = 120;
                    height = 40 + m_MaxVScalePixels;
                    break;
                }

                case LT_MAGNITUDES_ONLY:
                {
                    width  = 140;
                    height = 70;
                    break;
                }

                case LT_SYMBOLS_ONLY:
                {
                    width  = 120;
                    height = 7 * m_YSymbolSpacing;
                    break;
                }

                case LT_SCALE_MAGNITUDES:
                {
                    width  = 120;
                    height = 100 + m_MaxVScalePixels;
                    break;
                }

                case LT_FULL:
                {
                    width  = 120;
                    height = 100 + 7 * m_YSymbolSpacing + m_MaxVScalePixels;
                    break;
                }

                default:
                    break; // should never happen
            }

            break;
        }

        default:
        {
            return QSize();
        }
    }

    return QSize(width, height);
}

void Legend::paintLegend(QPaintDevice *pd)
{
    if (m_Painter)
    {
        delete m_Painter;
    }

    m_Painter       = new SkyQPainter(m_SkyMap, pd);
    m_DeletePainter = true;
    m_Painter->begin();

    paintLegend(m_Painter);

    m_Painter->end();
}

void Legend::paintLegend(SkyQPainter *painter)
{
    if (!m_DeletePainter)
    {
        m_Painter = painter;
    }

    if (m_Position != LP_FLOATING)
    {
        m_PositionFloating = positionToDeviceCoord(painter->device());
    }

    m_Painter->translate(m_PositionFloating.x(), m_PositionFloating.y());

    m_Painter->setFont(m_Font);

    QBrush backgroundBrush(m_BgColor, Qt::SolidPattern);
    QPen backgroundPen(m_cScheme->colorNamed("SNameColor"));
    backgroundPen.setStyle(Qt::SolidLine);

    // set brush & pen
    m_Painter->setBrush(backgroundBrush);
    m_Painter->setPen(backgroundPen);

    QSize size = calculateSize();
    if (m_DrawFrame)
    {
        m_Painter->drawRect(1, 1, size.width(), size.height());
    }

    else
    {
        QPen noLinePen;
        noLinePen.setStyle(Qt::NoPen);

        m_Painter->setPen(noLinePen);
        m_Painter->drawRect(1, 1, size.width(), size.height());

        m_Painter->setPen(backgroundPen);
    }

    switch (m_Orientation)
    {
        case LO_HORIZONTAL:
        {
            switch (m_Type)
            {
                case LT_SCALE_ONLY:
                {
                    paintScale(QPointF(20, 20));
                    break;
                }

                case LT_MAGNITUDES_ONLY:
                {
                    paintMagnitudes(QPointF(20, 20));
                    break;
                }

                case LT_SYMBOLS_ONLY:
                {
                    paintSymbols(QPointF(20, 20));
                    break;
                }

                case LT_SCALE_MAGNITUDES:
                {
                    paintMagnitudes(QPointF(20, 20));
                    paintScale(QPointF(150, 20));
                    break;
                }

                case LT_FULL:
                {
                    paintSymbols(QPointF(20, 20));
                    paintMagnitudes(QPointF(10, 40 + m_SymbolSize + m_BRectHeight));
                    paintScale(QPointF(200, 40 + m_SymbolSize + m_BRectHeight));
                    break;
                }

                default:
                    break; // should never happen
            }

            break;
        }

        case LO_VERTICAL:
        {
            switch (m_Type)
            {
                case LT_SCALE_ONLY:
                {
                    paintScale(QPointF(20, 20));
                    break;
                }

                case LT_MAGNITUDES_ONLY:
                {
                    paintMagnitudes(QPointF(20, 20));
                    break;
                }

                case LT_SYMBOLS_ONLY:
                {
                    paintSymbols(QPointF(20, 20));
                    break;
                }

                case LT_SCALE_MAGNITUDES:
                {
                    paintMagnitudes(QPointF(7, 20));
                    paintScale(QPointF(20, 80));
                    break;
                }

                case LT_FULL:
                {
                    paintSymbols(QPointF(30, 20));
                    paintMagnitudes(QPointF(7, 30 + 7 * m_YSymbolSpacing));
                    paintScale(QPointF(20, 90 + 7 * m_YSymbolSpacing));
                    break;
                }

                default:
                    break; // should never happen
            }

            break;
        }

        default:
            break; // should never happen
    }
}

void Legend::paintLegend(QPaintDevice *pd, LEGEND_TYPE type, LEGEND_POSITION pos)
{
    LEGEND_TYPE prevType    = m_Type;
    LEGEND_POSITION prevPos = m_Position;

    m_Type     = type;
    m_Position = pos;

    paintLegend(pd);

    m_Type     = prevType;
    m_Position = prevPos;
}

void Legend::paintLegend(SkyQPainter *painter, LEGEND_TYPE type, LEGEND_POSITION pos)
{
    LEGEND_TYPE prevType    = m_Type;
    LEGEND_POSITION prevPos = m_Position;

    m_Type     = type;
    m_Position = pos;

    paintLegend(painter);

    m_Type     = prevType;
    m_Position = prevPos;
}

void Legend::paintSymbols(QPointF pos)
{
    qreal x = pos.x();
    qreal y = pos.y();

    x += 30;

    switch (m_Orientation)
    {
        case Legend::LO_HORIZONTAL:
        {
            // paint Open Cluster/Asterism symbol
            QString label1 = i18n("Open Cluster") + '\n' + i18n("Asterism");
            paintSymbol(QPointF(x, y), 3, 1, 0, label1);
            x += m_XSymbolSpacing;

            // paint Globular Cluster symbol
            paintSymbol(QPointF(x, y), 4, 1, 0, i18n("Globular Cluster"));
            x += m_XSymbolSpacing;

            // paint Gaseous Nebula/Dark Nebula symbol
            QString label3 = i18n("Gaseous Nebula") + '\n' + i18n("Dark Nebula");
            paintSymbol(QPointF(x, y), 5, 1, 0, label3);
            x += m_XSymbolSpacing;

            // paint Planetary Nebula symbol
            paintSymbol(QPointF(x, y), 6, 1, 0, i18n("Planetary Nebula"));
            x += m_XSymbolSpacing;

            // paint Supernova Remnant
            paintSymbol(QPointF(x, y), 7, 1, 0, i18n("Supernova Remnant"));
            x += m_XSymbolSpacing;

            // paint Galaxy/Quasar
            QString label6 = i18n("Galaxy") + '\n' + i18n("Quasar");
            paintSymbol(QPointF(x, y), 8, 0.5, 60, label6);
            x += m_XSymbolSpacing;

            // paint Galaxy Cluster
            paintSymbol(QPointF(x, y), 14, 1, 0, i18n("Galactic Cluster"));

            break;
        }

        case Legend::LO_VERTICAL:
        {
            // paint Open Cluster/Asterism symbol
            QString label1 = i18n("Open Cluster") + '\n' + i18n("Asterism");
            paintSymbol(QPointF(x, y), 3, 1, 0, label1);
            y += m_YSymbolSpacing;

            // paint Globular Cluster symbol
            paintSymbol(QPointF(x, y), 4, 1, 0, i18n("Globular Cluster"));
            y += m_YSymbolSpacing;

            // paint Gaseous Nebula/Dark Nebula symbol
            QString label3 = i18n("Gaseous Nebula") + '\n' + i18n("Dark Nebula");
            paintSymbol(QPointF(x, y), 5, 1, 0, label3);
            y += m_YSymbolSpacing;

            // paint Planetary Nebula symbol
            paintSymbol(QPointF(x, y), 6, 1, 0, i18n("Planetary Nebula"));
            y += m_YSymbolSpacing;

            // paint Supernova Remnant
            paintSymbol(QPointF(x, y), 7, 1, 0, i18n("Supernova Remnant"));
            y += m_YSymbolSpacing;

            // paint Galaxy/Quasar
            QString label6 = i18n("Galaxy") + '\n' + i18n("Quasar");
            paintSymbol(QPointF(x, y), 8, 0.5, 60, label6);
            y += m_YSymbolSpacing;

            // paint Galaxy Cluster
            paintSymbol(QPointF(x, y), 14, 1, 0, i18n("Galactic Cluster"));

            break;
        }
        default:
            return; // should never happen
    }
}

void Legend::paintSymbol(QPointF pos, int type, float e, float angle, QString label)
{
    qreal x              = pos.x();
    qreal y              = pos.y();
    qreal bRectHalfWidth = (qreal)m_BRectWidth / 2;

    // paint symbol
    m_Painter->drawDeepSkySymbol(pos, type, m_SymbolSize, e, angle);
    QRectF bRect(QPoint(x - bRectHalfWidth, y + m_SymbolSize),
                 QPoint(x + bRectHalfWidth, y + m_SymbolSize + m_BRectHeight));
    //m_Painter->drawRect(bRect);
    // paint label
    m_Painter->drawText(bRect, label, QTextOption(Qt::AlignHCenter));
}

void Legend::paintMagnitudes(QPointF pos)
{
    qreal x = pos.x();
    qreal y = pos.y();

    m_Painter->drawText(x, y, i18n("Star Magnitudes:"));
    y += 15;

    for (int i = 1; i <= 9; i += 2)
    {
        m_Painter->drawPointSource(QPointF(x + i * 10, y), m_Painter->starWidth(i));
        m_Painter->drawText(x + i * 10 - 4, y + 20, QString::number(i));
    }
}

void Legend::paintScale(QPointF pos)
{
    qreal maxScalePixels;

    switch (m_Orientation)
    {
        case LO_HORIZONTAL:
        {
            maxScalePixels = m_MaxHScalePixels;
            break;
        }

        case LO_VERTICAL:
        {
            maxScalePixels = m_MaxVScalePixels;
            break;
        }

        default:
            return; // should never happen
    }

    qreal maxArcsec = maxScalePixels * 57.3 * 3600 / Options::zoomFactor();

    int deg    = 0;
    int arcmin = 0;
    int arcsec = 0;

    QString lab;
    if (maxArcsec >= 3600)
    {
        deg = maxArcsec / 3600;
        lab = QString::number(deg) + QString::fromWCharArray(L"\u00B0");
    }

    else if (maxArcsec >= 60)
    {
        arcmin = maxArcsec / 60;
        lab    = QString::number(arcmin) + '\'';
    }

    else
    {
        arcsec = maxArcsec;
        lab    = QString::number(arcsec) + "\"";
    }

    int actualArcsec = 3600 * deg + 60 * arcmin + arcsec;

    qreal size = actualArcsec * Options::zoomFactor() / 57.3 / 3600;

    qreal x = pos.x();
    qreal y = pos.y();

    switch (m_Orientation)
    {
        case LO_HORIZONTAL:
        {
            m_Painter->drawText(pos, i18n("Chart Scale:"));
            y += 15;

            m_Painter->drawLine(x, y, x + size, y);
            // paint line endings
            m_Painter->drawLine(x, y - 5, x, y + 5);
            m_Painter->drawLine(x + size, y - 5, x + size, y + 5);

            // paint scale text
            QRectF bRect(QPoint(x, y), QPoint(x + size, y + 20));
            m_Painter->drawText(bRect, lab, QTextOption(Qt::AlignHCenter));

            break;
        }

        case LO_VERTICAL:
        {
            m_Painter->drawText(pos, i18n("Chart Scale:"));
            y += 10;
            x += 40;

            m_Painter->drawLine(x, y, x, y + size);
            // paint line endings
            m_Painter->drawLine(x - 5, y, x + 5, y);
            m_Painter->drawLine(x - 5, y + size, x + 5, y + size);

            // paint scale text
            QRectF bRect(QPoint(x + 5, y), QPoint(x + 20, y + size));
            //m_Painter->drawRect(bRect);
            m_Painter->drawText(bRect, lab, QTextOption(Qt::AlignVCenter));

            break;
        }

        default:
            return; // should never happen
    }
}

QPoint Legend::positionToDeviceCoord(QPaintDevice *pd)
{
    QSize legendSize = calculateSize();

    switch (m_Position)
    {
        case LP_UPPER_LEFT: // position: upper left corner
        {
            return QPoint(0, 0);
        }

        case LP_UPPER_RIGHT: // position: upper right corner
        {
            return QPoint(pd->width() - legendSize.width(), 0);
        }

        case LP_LOWER_LEFT: // position: lower left corner
        {
            return QPoint(0, pd->height() - legendSize.height());
        }

        case LP_LOWER_RIGHT: // position: lower right corner
        {
            return QPoint(pd->width() - legendSize.width(), pd->height() - legendSize.height());
        }

        default: // legend is floating
        {
            return QPoint();
        }
    }
}

Legend::Legend(const Legend &o)
    : m_Painter(nullptr), m_SkyMap(o.m_SkyMap), m_DeletePainter(o.m_DeletePainter), m_Type(o.m_Type),
      m_Orientation(o.m_Orientation), m_Position(o.m_Position), m_PositionFloating(o.m_PositionFloating),
      m_cScheme(o.m_cScheme), m_Font(o.m_Font), m_BgColor(o.m_BgColor), m_DrawFrame(o.m_DrawFrame),
      m_SymbolSize(o.m_SymbolSize), m_BRectWidth(o.m_BRectWidth), m_BRectHeight(o.m_BRectHeight),
      m_MaxHScalePixels(o.m_MaxHScalePixels), m_MaxVScalePixels(o.m_MaxVScalePixels),
      m_XSymbolSpacing(o.m_XSymbolSpacing), m_YSymbolSpacing(o.m_YSymbolSpacing)
{
}

Legend& Legend::operator=(const Legend &o) noexcept
{
    m_SkyMap = o.m_SkyMap;
    m_DeletePainter = o.m_DeletePainter;
    m_Type = o.m_Type;
    m_Orientation = o.m_Orientation;
    m_Position = o.m_Position;
    m_PositionFloating = o.m_PositionFloating;
    m_cScheme = o.m_cScheme;
    m_Font = o.m_Font;
    m_BgColor = o.m_BgColor;
    m_DrawFrame = o.m_DrawFrame;
    m_SymbolSize = o.m_SymbolSize;
    m_BRectWidth = o.m_BRectWidth;
    m_BRectHeight = o.m_BRectHeight;
    m_MaxHScalePixels = o.m_MaxHScalePixels;
    m_MaxVScalePixels = o.m_MaxVScalePixels;
    m_XSymbolSpacing = o.m_XSymbolSpacing;
    m_YSymbolSpacing = o.m_YSymbolSpacing;
    return *this;
}
