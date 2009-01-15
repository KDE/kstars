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

#include "fov.h"

#include <qpainter.h>
#include <qfile.h>
//Added by qt3to4:
#include <QTextStream>
#include <kdebug.h>
#include <klocale.h>
#include <kstandarddirs.h>

FOV::FOV( const QString &n, float a, float b, int sh, const QString &col ) : Name( n ), Color( col ), SizeX( a ), Shape( sh )
{
    SizeY = (b < 0.0) ? a : b;
}

FOV::FOV() : Name( i18n( "No FOV" ) ), Color( "#FFFFFF" ), SizeX( 0.0 ), SizeY( 0.0 ), Shape( 0 )
{}

FOV::FOV( const QString &sname ) {
    QFile f;
    f.setFileName( KStandardDirs::locate( "appdata", "fov.dat" ) );

    int sh;
    float sx, sy;

    /* NOTE:
       If there are five fields, interpret them as Name:SizeX:SizeY:Shape:Color
       If there are four fields, interpret them as Name:Size = SizeX = SizeY:Shape:Color
       [This is required for backward compatibility]
    */

    if ( f.exists() && f.open( QIODevice::ReadOnly ) ) {
        QTextStream stream( &f );
        while ( !stream.atEnd() ) {
            QStringList fields = stream.readLine().split( ':' );
            bool ok( false );
            if ( fields.count() == 4 || fields.count() == 5 ) {
                int index = 0;
                if ( fields[index] == sname ) {
                    ++index;
                    sx = (float)(fields[index].toDouble( &ok ));
                    if( !ok )
                        break;  // Conversion failed, so no point continuing
                    ++ index;

                    if( fields.count() == 5 ) {
                        sy = (float)(fields[index].toDouble( &ok ));
                        if( !ok )
                            break;
                        ++index;
                    }
                    else
                        sy = sx;

                    sh = fields[index].toInt( &ok );
                    if( !ok )
                        break;
                    ++index;
                    Name = fields[0];
                    SizeX = sx;
                    SizeY = sy;
                    Shape = sh;
                    Color = fields[index];
                    return;
                }
            }
        }
    }
    
    //If we get here, then the symbol could not be assigned
    Name = i18n( "No FOV" );
    SizeX = 0.0;
    SizeY = 0.0;
    Shape = 0;
    Color = "#FFFFFF";
}

void FOV::draw( QPainter &p, float pixelSizeX, float pixelSizeY ) {
    p.setPen( QColor( color() ) );
    p.setBrush( Qt::NoBrush );

    int w = p.viewport().width();
    int h = p.viewport().height();

    int sx = int( pixelSizeX );
    if( pixelSizeY < 0.0 )
        pixelSizeY = pixelSizeX;
    int sy = int( pixelSizeY );

    switch ( shape() ) {
    case 0: { //Square
        p.drawRect( (w - sx)/2, (h - sy)/2, sx, sy );
        break;
    }
    case 1: { //Circle
        p.drawEllipse( (w - sx)/2, (h - sy)/2, sx, sy );
        break;
    }
    case 2: { //Crosshairs
        int sx1 = sx;
        int sy1 = sy;
        int sx2 = 2 * sx;
        int sy2 = 2 * sy;
        int sx3 = 3 * sx;
        int sy3 = 3 * sy;
        
        int x0 = w/2;  int y0 = h/2;
        int x1 = x0 - sx1/2;  int y1 = y0 - sy1/2;
        int x2 = x0 - sx2/2;  int y2 = y0 - sy2/2;
        int x3 = x0 - sx3/2;  int y3 = y0 - sy3/2;
        
        //Draw radial lines
        p.drawLine( x1, y0, x3, y0 );
        p.drawLine( x0+sx3/2, y0, x0+sx1/2, y0 );
        p.drawLine( x0, y1, x0, y3 );
        p.drawLine( x0, y0+sy1/2, x0, y0+sy3/2 );
        
        //Draw circles at 0.5 & 1 degrees
        p.drawEllipse( x1, y1, sx1, sy1 );
        p.drawEllipse( x2, y2, sx2, sy2 );
        
        break;
    }
    case 3: { //Bullseye
        int sx1 = sx;
        int sy1 = sy;
        int sx2 = 4 * sx;
        int sy2 = 4 * sy;
        int sx3 = 8 * sx;
        int sy3 = 8 * sy;

        int x0 = w/2;  int y0 = h/2;
        int x1 = x0 - sx1/2;  int y1 = y0 - sy1/2;
        int x2 = x0 - sx2/2;  int y2 = y0 - sy2/2;
        int x3 = x0 - sx3/2;  int y3 = y0 - sy3/2;

        p.drawEllipse( x1, y1, sx1, sy1 );
        p.drawEllipse( x2, y2, sx2, sy2 );
        p.drawEllipse( x3, y3, sx3, sy3 );

        break;
    }
    case 4: { // Solid Circle
        QColor colorAlpha( color() );
        colorAlpha.setAlpha(127);
        p.setBrush( QBrush ( colorAlpha ) );
        p.drawEllipse( (w - sx)/2, (h - sy)/2, sx, sy );
        p.setBrush(Qt::NoBrush);
        break;
    }
    }
}

