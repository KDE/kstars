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


#include "starpixmap.h"

#include <qbitmap.h>
#include <qcolor.h>
#include <qimage.h>
#include <qpainter.h>

StarPixmap::StarPixmap (int starColorMode, int starColorIntensity)
	: colorMode (starColorMode), colorIntensity (starColorIntensity)
{
	loadPixmaps (starColorMode, starColorIntensity);
}

QPixmap* StarPixmap::getPixmap (QChar *color, int size) {
	int c (0);
//	the colors from blue to red	+, O, B, A, F, G, K, M, N, P
// if *color is '+' use white star
	if (*color == 'O')
		c = 1;
	else if (*color == 'B')
		c = 2;
	else if (*color == 'A')
		c = 3;
	else if (*color == 'F')
		c = 4;
	else if (*color == 'G')
		c = 5;
	else if (*color == 'K')
		c = 6;
	else if (*color == 'M')
		c = 7;
	else if (*color == 'N')
		c = 8;
	else if (*color == 'P')
		c = 9;
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
	
	QPixmap pix (64, 64);
	QBitmap mask (64, 64);
	QImage image;
	QPainter p;
	QArray<QColor> starColor;
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
			p.drawEllipse (32, 32, 23, 23);
			p.end();
		}

		mask.fill (Qt::color0);

		p.begin (&mask);
		p.setPen (QPen ( Qt::color1, 1));
		p.setBrush( QBrush( Qt::color1 ) );
		p.drawEllipse(32,32,23,23);
		p.end();
		pix.setMask (mask);	// set the mask
		image = pix.convertToImage();	// create the image for smoothScale()

		for (int i = 0; i < 26; i++)
		{
			QImage tmp;
			if (i < 6)
				tmp = image.smoothScale (1+ i, 1+ i);	// size: 1x1 .. 24x24
			else	if (i < 12)
				tmp = image.smoothScale (int((1+ i)*1.25), int((1+ i)*1.25));	// size: 1x1 .. 30x30
			else	if (i < 18)
				tmp = image.smoothScale (int((1+ i)*1.5), int((1+ i)*1.5));	// size: 1x1 .. 36x36
			else
				tmp = image.smoothScale ((1+ i)*2, (1+ i)*2);	// size: 1x1 .. 48x48

			starPixmaps[color][i].convertFromImage (tmp);	// fill the array of pixmaps
		}
	}
}
