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
const int bRectHeight = 45;
const qreal maxHScalePixels = 200;
const qreal maxVScalePixels = 100;
const int xSymbolSpacing = 100;
const int ySymbolSpacing = 70;

const int frameWidth = 2;

Legend::Legend(KStars *KStars, LEGEND_ORIENTATION orientation)
    : m_Painter(0), m_KStars(KStars), m_DeletePainter(false), m_Orientation(orientation), m_cScheme(KStars->data()->colorScheme()),
    m_SymbolSize(symbolSize), m_BRectWidth(bRectWidth), m_BRectHeight(bRectHeight), m_MaxHScalePixels(maxHScalePixels),
    m_MaxVScalePixels(maxVScalePixels), m_XSymbolSpacing(xSymbolSpacing), m_YSymbolSpacing(ySymbolSpacing)
{}

Legend::~Legend()
{
    if(m_Painter && m_DeletePainter)
    {
        delete m_Painter;
    }
}

Legend::LEGEND_ORIENTATION Legend::getOrientation()
{
    return m_Orientation;
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

QFont Legend::getFont()
{
    return m_Font;
}

void Legend::setOrientation(LEGEND_ORIENTATION orientation)
{
    m_Orientation = orientation;
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

void Legend::setFont(const QFont &font)
{
    m_Font = font;
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
                width = 40 + m_MaxHScalePixels + 2 * frameWidth;
                height = 60 + 2 * frameWidth;
            }

            else
            {
                width = 7 * m_XSymbolSpacing + 2 * frameWidth;
                height = 20 + m_SymbolSize + m_BRectHeight + 70 + 2 * frameWidth;
            }

            break;
        }

    case LO_VERTICAL:
        {
            if(scaleOnly)
            {
                width = 50;
                height = 40 + m_MaxVScalePixels;
            }

            else
            {
                // TODO
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
    backgroundPen.setWidth(frameWidth);
    backgroundPen.setStyle(Qt::SolidLine);

    // set brush & pen
    m_Painter->setBrush(backgroundBrush);
    m_Painter->setPen(backgroundPen);

    // draw frame
    QSize size = calculateSize(scaleOnly);
    m_Painter->drawRect(0, 0, size.width(), size.height());

    // revert to old line width for symbols
    backgroundPen.setWidth(1);
    m_Painter->setPen(backgroundPen);

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
                paintSymbols(QPointF(20.0, 20.0));
                paintMagnitudes(QPointF(20.0, 500));
                paintScale(QPointF(20.0, 550.0));
            }

            break;
        }

    default: break; // should never happen
    }
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
            y += 15;

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
