/***************************************************************************
                          legend.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Thu May 26 2011
    copyright            : (C) 2001 by Rafał Kułaga
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
const int bRectHeight = 60;
const qreal maxHScalePixels = 200;
const qreal maxVScalePixels = 100;
const int xSymbolSpacing = 100;
const int ySymbolSpacing = 70;

Legend::Legend(KStars *KStars)
    : m_Painter(0), m_KStars(KStars), m_SymbolSize(symbolSize), m_BRectWidth(bRectWidth),
    m_BRectHeight(bRectHeight), m_MaxHScalePixels(maxHScalePixels), m_MaxVScalePixels(maxVScalePixels),
    m_XSymbolSpacing(xSymbolSpacing), m_YSymbolSpacing(ySymbolSpacing)
{}

Legend::~Legend()
{
    if(m_Painter)
    {
        delete m_Painter;
    }
}

int Legend::getSymbolSize()
{
    return m_SymbolSize;
}

int Legend::getBRectWidth()
{
    return m_BRectWidth;
}

int Legend::getBRectHeight()
{
    return m_BRectHeight;
}

qreal Legend::getMaxHScalePixels()
{
    return m_MaxHScalePixels;
}

qreal Legend::getMaxVScalePixels()
{
    return m_MaxVScalePixels;
}

int Legend::getXSymbolSpacing()
{
    return m_XSymbolSpacing;
}

int Legend::getYSymbolSpacing()
{
    return m_YSymbolSpacing;
}

void Legend::setSymbolSize(int size)
{
    m_SymbolSize = size;
}

void Legend::setBRectWidth(int width)
{
    m_BRectWidth = width;
}

void Legend::setBRectHeight(int height)
{
    m_BRectHeight = height;
}

void Legend::setMaxHScalePixels(qreal pixels)
{
    m_MaxHScalePixels = pixels;
}

void Legend::setMaxVScalePixels(qreal pixels)
{
    m_MaxVScalePixels = pixels;
}

void Legend::setXSymbolSpacing(int spacing)
{
    m_XSymbolSpacing = spacing;
}

void Legend::setYSymbolSpacing(int spacing)
{
    m_YSymbolSpacing = spacing;
}

void Legend::paintLegend(QPaintDevice *pd, QPoint pos, LEGEND_ORIENTATION orientation, bool scaleOnly)
{
    if(m_Painter)
    {
        delete m_Painter;
    }

    m_Painter = new SkyQPainter(m_KStars, pd);

    m_Painter->begin();
    m_Painter->drawSkyBackground();

    m_Painter->setFont(QFont("Courier New", 8));

    // draw frame
    m_Painter->setPen(QPen());
    m_Painter->drawRect(0, 0, pd->width(), pd->height());

//    ColorScheme *scheme = m_KStars->data()->colorScheme();
//    m_Painter->setBrush(QBrush(scheme->colorNamed("MessColor")));

    int x = pos.x();
    int y = pos.y();

    switch(orientation)
    {
    case LO_HORIZONTAL:
        {
            if(scaleOnly)
            {
                paintScale(QPointF(x + 20.0, y + 20.0), orientation);
            }

            else
            {
                paintSymbols(QPointF(x + 20.0, y + 20.0), orientation);
                paintMagnitudes(QPointF(x + 10.0, y + 100.0), orientation);
                paintScale(QPointF(x + 200.0, y + 100.0), orientation);
            }

            break;
        }

    case LO_VERTICAL:
        {
            if(scaleOnly)
            {
                // TODO
            }

            else
            {
                // TODO
            }

            break;
        }

    default: break; // should never happen
    }

    m_Painter->end();
}

void Legend::paintSymbols(QPointF pos, LEGEND_ORIENTATION orientation)
{
    qreal x = pos.x();
    qreal y = pos.y();

    x += 30;

    switch(orientation)
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
    QRectF bRect(QPoint(x - bRectHalfWidth, y + m_SymbolSize), QPoint(x + bRectHalfWidth, y + m_BRectHeight));
    //m_Painter->drawRect(bRect);
    // paint label
    m_Painter->drawText(bRect, label, QTextOption(Qt::AlignHCenter));
}

void Legend::paintMagnitudes(QPointF pos, LEGEND_ORIENTATION orientation)
{
    qreal x = pos.x();
    qreal y = pos.y();

    switch(orientation)
    {
    case LO_HORIZONTAL:
        {
            m_Painter->drawText(x, y, i18n("Star Magnitudes:"));
            y += 15;

            for(int i = 1; i <= 9; i += 2)
            {
                m_Painter->drawPointSource(QPointF(x + i * 10, y), m_Painter->starWidth(i));
                m_Painter->drawText(x + i * 10 - 4, y + 20, QString::number(i));
            }

            break;
        }

    case LO_VERTICAL:
        {
            m_Painter->drawText(x, y, i18n("Star Magnitudes:"));
            y += 15;

            for(int i = 1; i <= 9; i += 2)
            {
                m_Painter->drawPointSource(QPointF(x, y + i * 10), m_Painter->starWidth(i));
                m_Painter->drawText(x - 15, y + i * 10 + 3, QString::number(i));
            }

            break;
        }

    default: return; // should never happen
    }
}

void Legend::paintScale(QPointF pos, LEGEND_ORIENTATION orientation)
{
    qreal maxScalePixels;

    switch(orientation)
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

    switch(orientation)
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
            // TODO
            break;
        }

    default: return; // should never happen
    }
}
