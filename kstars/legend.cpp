/***************************************************************************
                          legend.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Thu May 26 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "legend.h"
#include "skyqpainter.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "colorscheme.h"

#include <QBrush>

const int symbolSize = 15;
const int bRectWidth = 100;
const int bRectHeight = 45;
const qreal maxHScalePixels = 200;
const qreal maxVScalePixels = 100;
const int xSymbolSpacing = 100;
const int ySymbolSpacing = 70;

Legend::Legend(LEGEND_ORIENTATION orientation)
    : m_Painter(0), m_KStars(KStars::Instance()), m_DeletePainter(false), m_Orientation(orientation),
    m_cScheme(KStars::Instance()->data()->colorScheme()), m_SymbolSize(symbolSize), m_BRectWidth(bRectWidth),
    m_BRectHeight(bRectHeight), m_MaxHScalePixels(maxHScalePixels), m_MaxVScalePixels(maxVScalePixels),
    m_XSymbolSpacing(xSymbolSpacing), m_YSymbolSpacing(ySymbolSpacing)
{}

Legend::~Legend()
{
    if(m_Painter && m_DeletePainter)
    {
        delete m_Painter;
    }
}

QSize Legend::calculateSize(bool scaleOnly)
{
    int width = 0;
    int height = 0;

    switch(m_Orientation)
    {
    case LO_HORIZONTAL:
        {
            if(scaleOnly)
            {
                width = 40 + m_MaxHScalePixels;
                height = 60;
            }

            else
            {
                width = 7 * m_XSymbolSpacing;
                height = 20 + m_SymbolSize + m_BRectHeight + 70;
            }

            break;
        }

    case LO_VERTICAL:
        {
            if(scaleOnly)
            {
                width = 120;
                height = 40 + m_MaxVScalePixels;
            }

            else
            {
                width = 120;
                height = 100 + 7 * m_YSymbolSpacing + m_MaxVScalePixels;
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

void Legend::paintLegend(QPaintDevice *pd, QPoint pos, bool scaleOnly)
{
    if(m_Painter)
    {
        delete m_Painter;
    }

    m_Painter = new SkyQPainter(m_KStars, pd);
    m_DeletePainter = true;
    m_Painter->begin();

    paintLegend(m_Painter, pos, scaleOnly);

    m_Painter->end();
}

void Legend::paintLegend(SkyQPainter *painter, QPoint pos, bool scaleOnly)
{
    if(!m_DeletePainter)
    {
        m_Painter = painter;
    }

    m_Painter->translate(pos.x(), pos.y());

    m_Painter->setFont(m_Font);

    QBrush backgroundBrush(m_cScheme->colorNamed("SkyColor"), Qt::SolidPattern);
    QPen backgroundPen(m_cScheme->colorNamed("SNameColor"));
    backgroundPen.setStyle(Qt::SolidLine);

    // set brush & pen
    m_Painter->setBrush(backgroundBrush);
    m_Painter->setPen(backgroundPen);

    // draw frame
    QSize size = calculateSize(scaleOnly);
    m_Painter->drawRect(0, 0, size.width(), size.height());

    switch(m_Orientation)
    {
    case LO_HORIZONTAL:
        {
            if(scaleOnly)
            {
                paintScale(QPointF(20.0, 20.0));
            }

            else
            {
                paintSymbols(QPointF(20.0, 20.0));
                paintMagnitudes(QPointF(10.0, 40.0 + m_SymbolSize + m_BRectHeight));
                paintScale(QPointF(200.0, 40.0 + m_SymbolSize + m_BRectHeight));
            }

            break;
        }

    case LO_VERTICAL:
        {
            if(scaleOnly)
            {
                paintScale(QPointF(20.0, 20.0));
            }

            else
            {
                paintSymbols(QPointF(30.0, 20.0));
                paintMagnitudes(QPointF(7.0, 30 + 7 * m_YSymbolSpacing));
                paintScale(QPointF(20.0, 80 + 7 * m_YSymbolSpacing));
            }

            break;
        }

    default: break; // should never happen
    }
}

void Legend::paintLegend(QPaintDevice *pd, LEGEND_POSITION pos, bool scaleOnly)
{
    paintLegend(pd, positionToDeviceCoord(pos, pd, calculateSize(scaleOnly)), scaleOnly);
}

void Legend::paintLegend(SkyQPainter *painter, LEGEND_POSITION pos, bool scaleOnly)
{
    paintLegend(painter, positionToDeviceCoord(pos, painter->device(), calculateSize(scaleOnly)), scaleOnly);
}

void Legend::paintSymbols(QPointF pos)
{
    qreal x = pos.x();
    qreal y = pos.y();

    x += 30;

    switch(m_Orientation)
    {
    case Legend::LO_HORIZONTAL :
        {
            // paint Open Cluster/Asterism symbol
            QString label1 = i18n("Open Cluster") + "\n" + i18n("Asterism");
            paintSymbol(QPointF(x, y), 3, 1, 0, label1);
            x += m_XSymbolSpacing;

            // paint Globular Cluster symbol
            paintSymbol(QPointF(x, y), 4, 1, 0, i18n("Globular Cluster"));
            x += m_XSymbolSpacing;

            // paint Gaseous Nebula/Dark Nebula symbol
            QString label3 = i18n("Gaseous Nebula") + "\n" + i18n("Dark Nebula");
            paintSymbol(QPointF(x, y), 5, 1, 0, label3);
            x += m_XSymbolSpacing;

            // paint Planetary Nebula symbol
            paintSymbol(QPointF(x, y), 6, 1, 0, i18n("Planetary Nebula"));
            x += m_XSymbolSpacing;

            // paint Supernova Remnant
            paintSymbol(QPointF(x, y), 7, 1, 0, i18n("Supernova Remnant"));
            x += m_XSymbolSpacing;

            // paint Galaxy/Quasar
            QString label6 = i18n("Galaxy") + "\n" + i18n("Quasar");
            paintSymbol(QPointF(x, y), 8, 0.5, 60, label6);
            x += m_XSymbolSpacing;

            // paint Galaxy Cluster
            paintSymbol(QPointF(x, y), 14, 1, 0, i18n("Galactic Cluster"));

            break;
        }

    case Legend::LO_VERTICAL :
        {
            // paint Open Cluster/Asterism symbol
            QString label1 = i18n("Open Cluster") + "\n" + i18n("Asterism");
            paintSymbol(QPointF(x, y), 3, 1, 0, label1);
            y += m_YSymbolSpacing;

            // paint Globular Cluster symbol
            paintSymbol(QPointF(x, y), 4, 1, 0, i18n("Globular Cluster"));
            y += m_YSymbolSpacing;

            // paint Gaseous Nebula/Dark Nebula symbol
            QString label3 = i18n("Gaseous Nebula") + "\n" + i18n("Dark Nebula");
            paintSymbol(QPointF(x, y), 5, 1, 0, label3);
            y += m_YSymbolSpacing;

            // paint Planetary Nebula symbol
            paintSymbol(QPointF(x, y), 6, 1, 0, i18n("Planetary Nebula"));
            y += m_YSymbolSpacing;

            // paint Supernova Remnant
            paintSymbol(QPointF(x, y), 7, 1, 0, i18n("Supernova Remnant"));
            y += m_YSymbolSpacing;

            // paint Galaxy/Quasar
            QString label6 = i18n("Galaxy") + "\n" + i18n("Quasar");
            paintSymbol(QPointF(x, y), 8, 0.5, 60, label6);
            y += m_YSymbolSpacing;

            // paint Galaxy Cluster
            paintSymbol(QPointF(x, y), 14, 1, 0, i18n("Galactic Cluster"));

            break;
        }
    default : return; // shoud never happen
    }
}

void Legend::paintSymbol(QPointF pos, int type, float e, float angle, QString label)
{
    qreal x = pos.x();
    qreal y = pos.y();
    qreal bRectHalfWidth = m_BRectWidth / 2;

    // paint symbol
    m_Painter->drawDeepSkySymbol(pos, type, m_SymbolSize, e, angle);
    QRectF bRect(QPoint(x - bRectHalfWidth, y + m_SymbolSize), QPoint(x + bRectHalfWidth, y + m_SymbolSize + m_BRectHeight));
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

    for(int i = 1; i <= 9; i += 2)
    {
        m_Painter->drawPointSource(QPointF(x + i * 10, y), m_Painter->starWidth(i));
        m_Painter->drawText(x + i * 10 - 4, y + 20, QString::number(i));
    }
}

void Legend::paintScale(QPointF pos)
{
    qreal maxScalePixels;

    switch(m_Orientation)
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

    default: return; // should never happen
    }

    qreal maxArcsec = maxScalePixels * 57.3 * 3600 / Options::zoomFactor();

    int deg = 0;
    int arcmin = 0;
    int arcsec = 0;

    QString lab;
    if(maxArcsec >= 3600)
    {
        deg = maxArcsec / 3600;
        lab = QString::number(deg) + QString::fromWCharArray(L"\u00B0");
    }

    else if(maxArcsec >= 60)
    {
        arcmin = maxArcsec / 60;
        lab = QString::number(arcmin) + "'";
    }

    else
    {
         arcsec = maxArcsec;
         lab = QString::number(arcsec) + "\"";
    }

    int actualArcsec = 3600 * deg + 60 * arcmin + arcsec;

    qreal size = actualArcsec * Options::zoomFactor() / 57.3 / 3600;

    qreal x = pos.x();
    qreal y = pos.y();

    switch(m_Orientation)
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

    default: return; // should never happen
    }
}

QPoint Legend::positionToDeviceCoord(LEGEND_POSITION pos, QPaintDevice *pd, QSize legendSize)
{
    switch(pos)
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

    default: // should never happen
        {
            return QPoint();
        }
    }
}
