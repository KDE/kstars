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
const int bRectHalfWidth = 50;
const int bRectHeight = 60;
const qreal maxHScalePixels = 200;
const qreal maxVScalePixels = 100;

Legend::Legend(KStars *KStars) : m_KStars(KStars)
{}

Legend::~Legend()
{}

void Legend::paintLegend(QPaintDevice *pd, LEGEND_ORIENTATION orientation, bool scaleOnly)
{
    SkyQPainter painter(m_KStars, pd);
    painter.begin();
    painter.drawSkyBackground();

    painter.setFont(QFont("Courier New", 8));

//    ColorScheme *scheme = m_KStars->data()->colorScheme();
//    painter.setBrush(QBrush(scheme->colorNamed("MessColor")));

    switch(orientation)
    {
    case LO_HORIZONTAL:
        {
            if(scaleOnly)
            {
                paintScale(QPointF(20.0, 20.0), orientation, &painter);
            }

            else
            {
                paintSymbols(QPointF(20.0, 20.0), orientation, &painter);
                paintMagnitudes(QPointF(10.0, 100.0), orientation, &painter);
                paintScale(QPointF(200.0, 100.0), orientation, &painter);
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

    painter.end();
}

void Legend::paintSymbols(QPointF pos, LEGEND_ORIENTATION orientation, SkyQPainter *painter)
{
    qreal x = pos.x();
    qreal y = pos.y();

    x += 30;

    switch(orientation)
    {
    case Legend::LO_HORIZONTAL :
        {
            // paint Open Cluster/Asterism symbol
            painter->drawDeepSkySymbol(QPointF(x, y), 3, symbolSize, 1, 0);
            QRectF bRect1(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            //painter->drawRect(bRect1);
            QString label1 = i18n("Open Cluster") + "\n" + i18n("Asterism");
            painter->drawText(bRect1, label1, QTextOption(Qt::AlignHCenter));
            x += 100;

            // paint Globular Cluster symbol
            painter->drawDeepSkySymbol(QPointF(x, y), 4, symbolSize, 1, 0);
            QRectF bRect2(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            //painter->drawRect(bRect2);
            painter->drawText(bRect2, i18n("Globular Cluster"), QTextOption(Qt::AlignHCenter));
            x += 100;

            // paint Gaseous Nebula/Dark Nebula symbol
            painter->drawDeepSkySymbol(QPointF(x, y), 5, symbolSize, 1, 0);
            QRectF bRect3(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            //painter->drawRect(bRect3);
            QString label3 = i18n("Gaseous Nebula") + "\n" + i18n("Dark Nebula");
            painter->drawText(bRect3, label3, QTextOption(Qt::AlignHCenter));
            x += 100;

            // paint Planetary Nebula symbol
            painter->drawDeepSkySymbol(QPointF(x, y), 6, symbolSize, 1, 0);
            QRectF bRect4(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            //painter->drawRect(bRect4);
            painter->drawText(bRect4, i18n("Planetary Nebula"), QTextOption(Qt::AlignHCenter));
            x += 100;

            // paint Supernova Remnant
            painter->drawDeepSkySymbol(QPointF(x, y), 7, symbolSize, 1, 0);
            QRectF bRect5(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            //painter->drawRect(bRect5);
            painter->drawText(bRect5, i18n("Supernova Remnant"), QTextOption(Qt::AlignHCenter));
            x += 100;

            // paint Galaxy/Quasar
            painter->drawDeepSkySymbol(QPointF(x, y), 8, symbolSize, 0.5, 60);
            QRectF bRect6(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            //painter->drawRect(bRect6);
            QString label6 = i18n("Galaxy") + "\n" + i18n("Quasar");
            painter->drawText(bRect6, label6, QTextOption(Qt::AlignHCenter));
            x += 100;

            // paint Galaxy Cluster
            painter->drawDeepSkySymbol(QPointF(x, y), 14, symbolSize, 1, 0);
            QRectF bRect7(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            //painter->drawRect(bRect7);
            painter->drawText(bRect7, i18n("Galactic Cluster"), QTextOption(Qt::AlignHCenter));

            break;
        }

    case Legend::LO_VERTICAL :
        {
            // paint Open Cluster/Asterism symbol
            painter->drawDeepSkySymbol(QPointF(x, y), 3, symbolSize, 1, 0);
            QRectF bRect1(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            QString label1 = i18n("Open Cluster") + "\n" + i18n("Asterism");
            painter->drawText(bRect1, label1, QTextOption(Qt::AlignHCenter));
            y += 70;

            // paint Globular Cluster symbol
            painter->drawDeepSkySymbol(QPointF(x, y), 4, symbolSize, 1, 0);
            QRectF bRect2(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            painter->drawRect(bRect2);
            painter->drawText(bRect2, i18n("Globular Cluster"), QTextOption(Qt::AlignHCenter));
            y += 70;

            // paint Gaseous Nebula/Dark Nebula symbol
            painter->drawDeepSkySymbol(QPointF(x, y), 5, symbolSize, 1, 0);
            QRectF bRect3(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            QString label3 = i18n("Gaseous Nebula") + "\n" + i18n("Dark Nebula");
            painter->drawText(bRect3, label3, QTextOption(Qt::AlignHCenter));
            y += 70;

            // paint Planetary Nebula symbol
            painter->drawDeepSkySymbol(QPointF(x, y), 6, symbolSize, 1, 0);
            QRectF bRect4(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            painter->drawRect(bRect4);
            painter->drawText(bRect4, i18n("Planetary Nebula"), QTextOption(Qt::AlignHCenter));
            y += 70;

            // paint Supernova Remnant
            painter->drawDeepSkySymbol(QPointF(x, y), 7, symbolSize, 1, 0);
            QRectF bRect5(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            painter->drawRect(bRect5);
            painter->drawText(bRect5, i18n("Supernova Remnant"), QTextOption(Qt::AlignHCenter));
            y += 70;

            // paint Galaxy/Quasar
            painter->drawDeepSkySymbol(QPointF(x, y), 8, symbolSize, 0.5, 60);
            QRectF bRect6(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            QString label6 = i18n("Galaxy") + "\n" + i18n("Quasar");
            painter->drawText(bRect6, label6, QTextOption(Qt::AlignHCenter));
            y += 70;

            // paint Galaxy Cluster
            painter->drawDeepSkySymbol(QPointF(x, y), 14, symbolSize, 1, 0);
            QRectF bRect7(QPoint(x - bRectHalfWidth, y + symbolSize), QPoint(x + bRectHalfWidth, y + bRectHeight));
            painter->drawRect(bRect7);
            painter->drawText(bRect7, i18n("Galactic Cluster"), QTextOption(Qt::AlignHCenter));

            break;
        }
    default : return; // shoud never happen
    }
}

void Legend::paintMagnitudes(QPointF pos, LEGEND_ORIENTATION orientation,  SkyQPainter *painter)
{
    qreal x = pos.x();
    qreal y = pos.y();

    switch(orientation)
    {
    case LO_HORIZONTAL:
        {
            painter->drawText(x, y, i18n("Star Magnitudes:"));
            y += 15;

            for(int i = 1; i <= 9; i += 2)
            {
                painter->drawPointSource(QPointF(x + i * 10, y), painter->starWidth(i));
                painter->drawText(x + i * 10 - 4, y + 20, QString::number(i));
            }

            break;
        }

    case LO_VERTICAL:
        {
            painter->drawText(x, y, i18n("Star Magnitudes:"));
            y += 15;

            for(int i = 1; i <= 9; i += 2)
            {
                painter->drawPointSource(QPointF(x, y + i * 10), painter->starWidth(i));
                painter->drawText(x - 15, y + i * 10 + 3, QString::number(i));
            }

            break;
        }

    default: return; // should never happen
    }
}

void Legend::paintScale(QPointF pos, LEGEND_ORIENTATION orientation, SkyQPainter *painter)
{
    qreal maxScalePixels;

    switch(orientation)
    {
    case LO_HORIZONTAL:
        {
            maxScalePixels = maxHScalePixels;
            break;
        }

    case LO_VERTICAL:
        {
            maxScalePixels = maxVScalePixels;
            break;
        }

    default: return; // should never happen
    }

    qreal maxArcsect = maxScalePixels * 57.3 * 3600 / Options::zoomFactor();

    int deg = 0;
    int arcmin = 0;
    int arcsec = 0;

    QString lab;
    if(maxArcsect >= 3600)
    {
        deg = maxArcsect / 3600;
        lab = QString::number(deg) + QString::fromWCharArray(L"\u00B0");
    }

    else if(maxArcsect >= 60)
    {
        arcmin = maxArcsect / 60;
        lab = QString::number(arcmin) + "'";
    }

    else
    {
         arcsec = maxArcsect;
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
            painter->drawText(pos, i18n("Chart Scale:"));
            y += 15;

            painter->drawLine(x, y, x + size, y);
            // paint line endings
            painter->drawLine(x, y - 5, x, y + 5);
            painter->drawLine(x + size, y - 5, x + size, y + 5);
            // paint scale text
            QRectF bRect(QPoint(x, y), QPoint(x + size, y + 20));
            painter->drawText(bRect, lab, QTextOption(Qt::AlignHCenter));

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
