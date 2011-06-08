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

class SkyQPainter;
class QPaintDevice;
class QPointF;
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

    void paintLegend(QPaintDevice *pd, LEGEND_ORIENTATION orientation, bool scaleOnly);

private:
    void paintSymbols(QPointF pos, LEGEND_ORIENTATION orientation, SkyQPainter *painter);
    void paintMagnitudes(QPointF pos, LEGEND_ORIENTATION orientation, SkyQPainter *painter);
    void paintScale(QPointF pos, LEGEND_ORIENTATION orientation, SkyQPainter *painter);

    //SkyQPainter m_Painter;
    KStars *m_KStars;
};

#endif // LEGEND_H
