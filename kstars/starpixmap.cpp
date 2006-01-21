/***************************************************************************
                          starpixmap.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Sep 19 2001
    copyright            : (C) 2001 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QBitmap>
#include <QPainter>
#include <QPixmap>
#include <QVector>

#include <kdebug.h>

#include "starpixmap.h"

#define STARSIZE 24

StarPixmap::StarPixmap (int starColorMode, int starColorIntensity)
	: colorMode (starColorMode), colorIntensity (starColorIntensity)
{
	loadPixmaps (starColorMode, starColorIntensity);
}

QPixmap* StarPixmap::getPixmap (QChar *color, int size) {
	int c;
//	the colors from blue to red	+, O, B, A, F, G, K, M, N, P
// if *color is '+' use white star
	switch ( color->toAscii() ) {
		case 'O' : c = 1; break;
		case 'B' : c = 2; break;
		case 'A' : c = 3; break;
		case 'F' : c = 4; break;
		case 'G' : c = 5; break;
		case 'K' : c = 6; break;
		case 'M' : c = 7; break;
		case 'N' : c = 8; break;
		case 'P' : c = 9; break;
		default : c = 0;
	}
	// don't overflow the array
	if (size > 25) size = 25;
	return &starPixmaps[c][size];
}

void StarPixmap::setColorMode( int newMode ) {
	colorMode = newMode;
	loadPixmaps ( colorMode, colorIntensity );
}

void StarPixmap::setIntensity ( int newIntensity ) {
	colorIntensity = newIntensity;
	loadPixmaps ( colorMode, colorIntensity );
}

void StarPixmap::loadPixmaps (int newColorMode, int newColorIntensity) {
	colorMode = newColorMode;
	colorIntensity = newColorIntensity;

	if (colorIntensity < 0) colorIntensity = 0;	// min

	QPixmap pix(STARSIZE, STARSIZE), tmp;
	QBitmap mask (STARSIZE, STARSIZE);
//	QImage image;
//	image.setAlphaBuffer(true);
	QPainter p;
	QVector<QColor> starColor;
	starColor.resize( 8 );

	starColor[0] = QColor( 255, 255, 255 );   //default to white
	starColor[1] = QColor(   0,   0, 255 );   //type O
	starColor[2] = QColor(   0, 200, 255 );   //type B
	starColor[3] = QColor(   0, 255, 255 );   //type A
	starColor[4] = QColor( 200, 255, 100 );   //type F
	starColor[5] = QColor( 255, 255,   0 );   //type G
	starColor[6] = QColor( 255, 100,   0 );   //type K
	starColor[7] = QColor( 255,   0,   0 );   //type M

// background of the star
	if ( colorMode==1 ) // night colors (fill red, no temperature colors)
		pix.fill (Qt::red);
	else if ( colorMode==2 ) //star chart colors (fill black, no temperature colors)
		pix.fill (Qt::black);
	else
		pix.fill (Qt::white);	// default (white)

	for (int color = 0; color < 10; color ++) {
		int ic = color;
		if ( color > 7 ) ic = 7;

		if (colorMode==0)	{
			p.begin (&pix);
			p.setPen (QPen (starColor[ic], colorIntensity));	// the intensity of color determines the width of the pen
			p.drawEllipse (0, 0, STARSIZE, STARSIZE);
			p.end();
		}

		mask.fill (Qt::color0);
		p.begin (&mask);
		p.setPen (QPen ( Qt::color1, 1));
		p.setBrush( QBrush( Qt::color1 ) );
		p.drawEllipse(0, 0, STARSIZE, STARSIZE);
		p.end();

		pix.setMask (mask);	// set the mask

		for (int i = 0; i < 26; i++)
		{
			int size = STARSIZE*(i+1)/26;
			starPixmaps[color][i] = pix.scaled( size, size );
		}
	}
}
