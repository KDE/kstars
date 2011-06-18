/***************************************************************************
                          legend.h  -  K Desktop Planetarium
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

#ifndef LEGEND_H
#define LEGEND_H

#include <QObject>

class SkyQPainter;
class QPaintDevice;
class QPoint;
class QPointF;
class QString;
class KStars;

class Legend
{
public:

    enum LEGEND_ORIENTATION
    {
        LO_HORIZONTAL,
        LO_VERTICAL
    };

    Legend(KStars *KStars);
    ~Legend();

    int getSymbolSize();
    int getBRectWidth();
    int getBRectHeight();
    qreal getMaxHScalePixels();
    qreal getMaxVScalePixels();
    int getXSymbolSpacing();
    int getYSymbolSpacing();

    void setSymbolSize(int size);
    void setBRectWidth(int width);
    void setBRectHeight(int height);
    void setMaxHScalePixels(qreal pixels);
    void setMaxVScalePixels(qreal pixels);
    void setXSymbolSpacing(int spacing);
    void setYSymbolSpacing(int spacing);

    void paintLegend(QPaintDevice *pd, QPoint pos, LEGEND_ORIENTATION orientation, bool scaleOnly);

private:
    void paintSymbols(QPointF pos, LEGEND_ORIENTATION orientation);
    void paintSymbol(QPointF pos, int type, float e, float angle, QString label);
    void paintMagnitudes(QPointF pos, LEGEND_ORIENTATION orientation);
    void paintScale(QPointF pos, LEGEND_ORIENTATION orientation);

    SkyQPainter *m_Painter;
    KStars *m_KStars;

    int m_SymbolSize;
    int m_BRectWidth;
    int m_BRectHeight;
    qreal m_MaxHScalePixels;
    qreal m_MaxVScalePixels;
    int m_XSymbolSpacing;
    int m_YSymbolSpacing;
};

#endif // LEGEND_H
