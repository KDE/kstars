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

FOV::Shape FOV::intToShape(int s)
{ 
    return (s >= FOV::UNKNOWN || s < 0) ? FOV::UNKNOWN : static_cast<FOV::Shape>(s);
} 

FOV::FOV( const QString &n, float a, float b, Shape sh, const QString &col ) :
    m_name( n ), m_color( col ), m_sizeX( a ), m_shape( sh )
{ 
    m_sizeY = (b < 0.0) ? a : b;
} 

FOV::FOV() : m_name( i18n( "No FOV" ) ), m_color( "#FFFFFF" ), m_sizeX( 0.0 ), m_sizeY( 0.0 ), m_shape( SQUARE )
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
                    m_name = fields[0];
                    m_sizeX = sx;
                    m_sizeY = sy;
                    m_shape = intToShape(sh);
                    m_color = fields[index];
                    return;
                }
            }
        }
    }
    
    //If we get here, then the symbol could not be assigned
    m_name = i18n( "No FOV" );
    m_sizeX = 0.0;
    m_sizeY = 0.0;
    m_shape = UNKNOWN;
    m_color = "#FFFFFF";
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
    case SQUARE: { //Square
        p.drawRect( (w - sx)/2, (h - sy)/2, sx, sy );
        break;
    }
    case CIRCLE: { //Circle
        p.drawEllipse( (w - sx)/2, (h - sy)/2, sx, sy );
        break;
    }
    case CROSSHAIRS: { //Crosshairs
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
    case BULLSEYE: { //Bullseye
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
    case SOLIDCIRCLE: { // Solid Circle
        QColor colorAlpha( color() );
        colorAlpha.setAlpha(127);
        p.setBrush( QBrush ( colorAlpha ) );
        p.drawEllipse( (w - sx)/2, (h - sy)/2, sx, sy );
        p.setBrush(Qt::NoBrush);
        break;
    }
    default: ; 
    }
}

void FOV::setShape( int s)
{
    m_shape = intToShape(s);
}
