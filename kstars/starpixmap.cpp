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

StarPixmap::StarPixmap (bool useNightColors, int colorIntensity)
	: night (useNightColors), intensity (colorIntensity)
{
	loadPixmaps (night, intensity);
}

StarPixmap::~StarPixmap(){
}

QPixmap* StarPixmap::getPixmap (QChar *color, int size, int zoomlevel) {
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
	return &starPixmaps[c][size/*+zoomlevel*/];
}

void StarPixmap::setDefaultColors() {
	if (night)	// only if night colors are loaded
		loadPixmaps (false);
}

void StarPixmap::setNightColors() {
	if (!night)	// only if default colors are loaded
		loadPixmaps (true);
}

void StarPixmap::setIntensity (int colorIntensity) {
	loadPixmaps (night, colorIntensity);
}

void StarPixmap::loadPixmaps (bool useNightColors, int colorIntensity) {
	night = useNightColors;
	intensity = colorIntensity;
	if (intensity < 0) intensity = 0;	// min
	
	QPixmap pix (64, 64);
	QBitmap mask (64, 64);
	QImage image;
	QPainter p;

// background of the star
	if (night)
		pix.fill (Qt::red);	// night color
	else
		pix.fill (Qt::white);	// default color
/*
	*The colors are defined:
		0 = white
		white = QColor (255, 255, 255)
		1..9 = blue..red
		blue		=	QColor (	  0	,	0	, 255	)	color = 1..4
		green	=	QColor (	  0	, 255,	0		)	color = 5
		red		=	QColor (	255	, 	0	,	0		)	color = 6..9
	*/
	for (int color = 0; color < 10; color ++)
	{
		if (color)	// first array field is white
		{
			p.begin (&pix);
			int red(0), green(0), blue(0);
  		if (color==1) { blue = 255; red = 100; green = 100; }
			else if (color==2) { blue = 100; green = 255; }
			else if (color==3) { green = 255; }
			else if (color==4) { red = 100; green = 255; }
			else if (color==5) { red = 255; green = 255; }
			else if (color==6) { red = 255; green = 100; }
			else if (color>=7) { red = 255; }

			QColor c (red, green, blue);
//			p.setPen (QPen (c, 4));	// the intensity of color depends on the width of the pen
			p.setPen (QPen (c, colorIntensity));	// the intensity of color depends on the width of the pen
		
			if (!night)	// if not night is set paint the corona
				p.drawEllipse (32, 32, 23, 23);
			p.end();
		}
		mask.fill (Qt::color0);
	
		p.begin (&mask);
		p.setPen (QPen ( Qt::color1, 1));
		p.setBrush( QBrush( Qt::color1 ) );
//		for (int i = 0; i < 24; i++)
//			p.drawEllipse (32, 32, i, i);	// paint the star
		p.drawEllipse(32,32,23,23);
		p.end();
		pix.setMask (mask);	// set the mask
		image = pix.convertToImage();	// create the image for smoothScale()
	
		for (int i = 0; i < 24; i++)
		{
			QImage tmp;
			if (i < 6)
				tmp = image.smoothScale (1+ i, 1+ i);	// size: 1x1 .. 24x24
			else	if (i < 12)
				tmp = image.smoothScale ((1+ i)*1.25, (1+ i)*1.25);	// size: 1x1 .. 30x30
			else	if (i < 18)
				tmp = image.smoothScale ((1+ i)*1.5, (1+ i)*1.5);	// size: 1x1 .. 36x36
			else
				tmp = image.smoothScale ((1+ i)*2, (1+ i)*2);	// size: 1x1 .. 48x48
//			qDebug ("image: %ix%i", tmp.width(), tmp.height());
			starPixmaps[color][i].convertFromImage (tmp);	// fill the array of pixmaps
		}
	}
}
