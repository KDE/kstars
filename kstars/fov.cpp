/***************************************************************************
                          fov.cpp  -  description
                             -------------------
    begin                : Fri 05 Sept 2003
    copyright            : (C) 2003 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <tqpainter.h>
#include <tqfile.h>
#include <kdebug.h>
#include <klocale.h>
#include <kstandarddirs.h>

#include "fov.h"

//------------FOV-----------------//
FOV::FOV( TQString n, float sz, int sh, TQString col ) : Name( n ), Color( col ), Size( sz ), Shape( sh )
{}

FOV::FOV() : Name( i18n( "No FOV" ) ), Color( "#FFFFFF" ), Size( 0.0 ), Shape( 0 )
{}

FOV::FOV( TQString sname ) {
	TQFile f;
	f.setName( locate( "appdata", "fov.dat" ) );
	
	int sh;
	float sz;

	if ( f.exists() && f.open( IO_ReadOnly ) ) {
		TQTextStream stream( &f );
		while ( !stream.eof() ) {
			TQStringList fields = TQStringList::split( ":", stream.readLine() );
			bool ok( false );

			if ( fields.count() == 4 ) {
				if ( fields[0] == sname ) {
					sz = (float)(fields[1].toDouble( &ok ));
					if ( ok ) {
						sh = fields[2].toInt( &ok );
						if ( ok ) {
							Name = fields[0];
							Size = sz;
							Shape = sh;
							Color = fields[3]; 
							
							return;
						}
					}
					
					break;
				}
			}
		}
	}
	
	//If we get here, then the symbol could not be assigned
	Name = i18n( "No FOV" );
	Size = 0.0;
	Shape = 0;
	Color = "#FFFFFF";
}

void FOV::draw( TQPainter &p, float pixelsize ) {
	p.setPen( TQColor( color() ) );
	p.setBrush( Qt::NoBrush );
	int w = p.viewport().width();
	int h = p.viewport().height();

	switch ( shape() ) {
		case 0: { //Square
			int s = int( pixelsize );
			p.drawRect( (w - s)/2, (h - s)/2, s, s );
			break;
		}
		case 1: { //Circle
			int s = int( pixelsize );
			p.drawEllipse( (w - s)/2, (h - s)/2, s, s );
			break;
		}
		case 2: { //Crosshairs
			int s1 = int( pixelsize );
			int s2 = 2*int( pixelsize );
			int s3 = 3*int( pixelsize );

			int x0 = w/2;  int y0 = h/2;
			int x1 = x0 - s1/2;  int y1 = y0 - s1/2;
			int x2 = x0 - s2/2;  int y2 = y0 - s2/2;
			int x3 = x0 - s3/2;  int y3 = y0 - s3/2;

			//Draw radial lines
			p.drawLine( x1, y0, x3, y0 );
			p.drawLine( x0+s3/2, y0, x0+s1/2, y0 );
			p.drawLine( x0, y1, x0, y3 );
			p.drawLine( x0, y0+s1/2, x0, y0+s3/2 );

			//Draw circles at 0.5 & 1 degrees
			p.drawEllipse( x1, y1, s1, s1 );
			p.drawEllipse( x2, y2, s2, s2 );

			break;
		}
		case 3: { //Bullseye
			int s1 = int( pixelsize );
			int s2 = 2*int( pixelsize );
			int s3 = 3*int( pixelsize );

			int x0 = w/2;  int y0 = h/2;
			int x1 = x0 - s1/2;  int y1 = y0 - s1/2;
			int x2 = x0 - s2/2;  int y2 = y0 - s2/2;
			int x3 = x0 - s3/2;  int y3 = y0 - s3/2;

			p.drawEllipse( x1, y1, s1, s1 );
			p.drawEllipse( x2, y2, s2, s2 );
			p.drawEllipse( x3, y3, s3, s3 );

			break;
		}
		case 4: { // Solid Circle
			int s = int( pixelsize );
			p.setBrush( TQBrush ( TQColor( color() ), Qt::Dense4Pattern) );
			p.drawEllipse( (w - s)/2, (h - s)/2, s, s );
			p.setBrush(Qt::NoBrush);
			break;
		}
	}
}

