/***************************************************************************
                          deepskyobject.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "deepskyobject.h"

#include <kglobal.h>

#include <QFile>
#include <QRegExp>
#include <QPainter>
#include <QImage>

#include <assert.h>

#include "kstarsdata.h"
#include "ksutils.h"
#include "dms.h"
#include "kspopupmenu.h"
#include "Options.h"
#include "skymap.h"

DeepSkyObject::DeepSkyObject( const DeepSkyObject &o ) :
    SkyObject( o ),
    Catalog( o.Catalog ),
    PositionAngle( o.PositionAngle ),
    UGC( o.UGC ),
    PGC( o.PGC ),
    MajorAxis( o.MajorAxis ),
    MinorAxis( o.MinorAxis )
{
    Image = o.Image ? new QImage(*o.Image) : 0;
    customCat = NULL;
    Flux = 0;
    updateID = updateNumID = 0;
}

DeepSkyObject::DeepSkyObject( int t, dms r, dms d, float m,
                              const QString &n, const QString &n2,
                              const QString &lname, const QString &cat,
                              float a, float b, double pa, int pgc, int ugc )
        : SkyObject( t, r, d, m, n, n2, lname ) {
    MajorAxis = a;
    MinorAxis = b;
    PositionAngle = pa;
    PGC = pgc;
    UGC = ugc;
    setCatalog( cat );
    Image = 0;
    updateID = updateNumID = 0;
    customCat = NULL;
    Flux = 0;
}

DeepSkyObject::DeepSkyObject( int t, double r, double d, float m,
                              const QString &n, const QString &n2,
                              const QString &lname, const QString &cat,
                              float a, float b, double pa, int pgc, int ugc )
        : SkyObject( t, r, d, m, n, n2, lname ) {
    MajorAxis = a;
    MinorAxis = b;
    PositionAngle = pa;
    PGC = pgc;
    UGC = ugc;
    setCatalog( cat );
    Image = 0;
    updateID = updateNumID = 0;
    customCat = NULL;
    Flux = 0;
}

DeepSkyObject* DeepSkyObject::clone() const
{
    return new DeepSkyObject(*this);
}
    
void DeepSkyObject::initPopupMenu( KSPopupMenu *pmenu ) {
    pmenu->createDeepSkyObjectMenu( this );
}

float DeepSkyObject::e() const {
    if ( MajorAxis==0.0 || MinorAxis==0.0 )
        return 1.0; //assume circular
    return MinorAxis / MajorAxis;
}

QString DeepSkyObject::catalog() const {
    if ( isCatalogM() ) return QString("M");
    if ( isCatalogNGC() ) return QString("NGC");
    if ( isCatalogIC() ) return QString("IC");
    return QString();
}

void DeepSkyObject::setCatalog( const QString &cat ) {
    if ( cat.toUpper() == "M" ) Catalog = (unsigned char)CAT_MESSIER;
    else if ( cat.toUpper() == "NGC" ) Catalog = (unsigned char)CAT_NGC;
    else if ( cat.toUpper() == "IC"  ) Catalog = (unsigned char)CAT_IC;
    else Catalog = (unsigned char)CAT_UNKNOWN;
}   

QImage* DeepSkyObject::readImage( void ) {
    QFile file;
    if ( Image==0 ) { //Image not currently set; try to load it from disk.
        QString fname = name().toLower().remove( ' ' ) + ".png";

        if ( KSUtils::openDataFile( file, fname ) ) {
            file.close();
            Image = new QImage( file.fileName(), "PNG" );
        }
    }

    return Image;
}

void DeepSkyObject::drawSymbol( QPainter &psky, float x, float y, double PositionAngle, double zoom ) {
    // if size is 0.0 set it to 1.0, this are normally stars (type 0 and 1)
    // if we use size 0.0 the star wouldn't be drawn
    float majorAxis = a();
    if ( majorAxis == 0.0 ) {	majorAxis = 1.0; }

    double scale = SkyMap::Instance()->scale();
    float size = scale * majorAxis * dms::PI * zoom / 10800.0;
    int isize = int(size);

    float dx1 = -0.5*size;
    float dx2 =  0.5*size;
    float dy1 = -1.0*e()*size/2.;
    float dy2 = e()*size/2.;
    float x1 = x + dx1;
    float x2 = x + dx2;
    float y1 = y + dy1;
    float y2 = y + dy2;

    float dxa = -size/4.;
    float dxb =  size/4.;
    float dya = -1.0*e()*size/4.;
    float dyb = e()*size/4.;
    float xa = x + dxa;
    float xb = x + dxb;
    float ya = y + dya;
    float yb = y + dyb;

    float psize;

    QBrush tempBrush;

    switch ( type() ) {
    case 0:
    case 1: //catalog star
        //Some NGC/IC objects are stars...changed their type to 1 (was double star)
        if (size<2.) size = 2.;
        if ( Options::useAntialias() )
            psky.drawEllipse( QRectF(x1, y1, size/2., size/2.) );
        else
            psky.drawEllipse( QRect(int(x1), int(y1), int(size/2), int(size/2)) );
        break;
    case 2: //Planet
        break;
    case 3: //Open cluster; draw circle of points
    case 13: // Asterism
        tempBrush = psky.brush();
        psky.setBrush( psky.pen().color() );
        psize = 2.;
        if ( size > 50. )  psize *= 2.;
        if ( size > 100. ) psize *= 2.;
        if ( Options::useAntialias() ) {
            psky.drawEllipse( QRectF(xa, y1, psize, psize) );
            psky.drawEllipse( QRectF(xb, y1, psize, psize) );
            psky.drawEllipse( QRectF(xa, y2, psize, psize) );
            psky.drawEllipse( QRectF(xb, y2, psize, psize) );
            psky.drawEllipse( QRectF(x1, ya, psize, psize) );
            psky.drawEllipse( QRectF(x1, yb, psize, psize) );
            psky.drawEllipse( QRectF(x2, ya, psize, psize) );
            psky.drawEllipse( QRectF(x2, yb, psize, psize) );
        } else {
            int ix1 = int(x1); int iy1 = int(y1);
            int ix2 = int(x2); int iy2 = int(y2);
            int ixa = int(xa); int iya = int(ya);
            int ixb = int(xb); int iyb = int(yb);
            psky.drawEllipse( QRect(ixa, iy1, int(psize), int(psize)) );
            psky.drawEllipse( QRect(ixb, iy1, int(psize), int(psize)) );
            psky.drawEllipse( QRect(ixa, iy2, int(psize), int(psize)) );
            psky.drawEllipse( QRect(ixb, iy2, int(psize), int(psize)) );
            psky.drawEllipse( QRect(ix1, iya, int(psize), int(psize)) );
            psky.drawEllipse( QRect(ix1, iyb, int(psize), int(psize)) );
            psky.drawEllipse( QRect(ix2, iya, int(psize), int(psize)) );
            psky.drawEllipse( QRect(ix2, iyb, int(psize), int(psize)) );
        }
        psky.setBrush( tempBrush );
        break;        
    case 4: //Globular Cluster
        if (size<2.) size = 2.;
        psky.save();
        psky.translate( x, y );
        psky.rotate( PositionAngle );  //rotate the coordinate system

        if ( Options::useAntialias() ) {
            psky.drawEllipse( QRectF(dx1, dy1, size, e()*size) );
            psky.drawLine( QPointF(0., dy1), QPointF(0., dy2) );
            psky.drawLine( QPointF(dx1, 0.), QPointF(dx2, 0.) );
            psky.restore(); //reset coordinate system
        } else {
            int idx1 = int(dx1); int idy1 = int(dy1);
            int idx2 = int(dx2); int idy2 = int(dy2);
            psky.drawEllipse( QRect(idx1, idy1, isize, int(e()*size)) );
            psky.drawLine( QPoint(0, idy1), QPoint(0, idy2) );
            psky.drawLine( QPoint(idx1, 0), QPoint(idx2, 0) );
            psky.restore(); //reset coordinate system
        }
        break;

    case 5: //Gaseous Nebula
    case 15: // Dark Nebula
        if (size <2.) size = 2.;
        psky.save();
        psky.translate( x, y );
        psky.rotate( PositionAngle );  //rotate the coordinate system

        if ( Options::useAntialias() ) {
            psky.drawLine( QPointF(dx1, dy1), QPointF(dx2, dy1) );
            psky.drawLine( QPointF(dx2, dy1), QPointF(dx2, dy2) );
            psky.drawLine( QPointF(dx2, dy2), QPointF(dx1, dy2) );
            psky.drawLine( QPointF(dx1, dy2), QPointF(dx1, dy1) );
        } else {
            int idx1 = int(dx1); int idy1 = int(dy1);
            int idx2 = int(dx2); int idy2 = int(dy2);
            psky.drawLine( QPoint(idx1, idy1), QPoint(idx2, idy1) );
            psky.drawLine( QPoint(idx2, idy1), QPoint(idx2, idy2) );
            psky.drawLine( QPoint(idx2, idy2), QPoint(idx1, idy2) );
            psky.drawLine( QPoint(idx1, idy2), QPoint(idx1, idy1) );
        }
        psky.restore(); //reset coordinate system
        break;
    case 6: //Planetary Nebula
        if (size<2.) size = 2.;
        psky.save();
        psky.translate( x, y );
        psky.rotate( PositionAngle );  //rotate the coordinate system

        if ( Options::useAntialias() ) {
            psky.drawEllipse( QRectF(dx1, dy1, size, e()*size) );
            psky.drawLine( QPointF(0., dy1), QPointF(0., dy1 - e()*size/2. ) );
            psky.drawLine( QPointF(0., dy2), QPointF(0., dy2 + e()*size/2. ) );
            psky.drawLine( QPointF(dx1, 0.), QPointF(dx1 - size/2., 0.) );
            psky.drawLine( QPointF(dx2, 0.), QPointF(dx2 + size/2., 0.) );
        } else {
            int idx1 = int(dx1); int idy1 = int(dy1);
            int idx2 = int(dx2); int idy2 = int(dy2);
            psky.drawEllipse( QRect( idx1, idy1, isize, int(e()*size) ) );
            psky.drawLine( QPoint(0, idy1), QPoint(0, idy1 - int(e()*size/2) ) );
            psky.drawLine( QPoint(0, idy2), QPoint(0, idy2 + int(e()*size/2) ) );
            psky.drawLine( QPoint(idx1, 0), QPoint(idx1 - int(size/2), 0) );
            psky.drawLine( QPoint(idx2, 0), QPoint(idx2 + int(size/2), 0) );
        }

        psky.restore(); //reset coordinate system
        break;
    case 7: //Supernova remnant
        if (size<2) size = 2;
        psky.save();
        psky.translate( x, y );
        psky.rotate( PositionAngle );  //rotate the coordinate system

        if ( Options::useAntialias() ) {
            psky.drawLine( QPointF(0., dy1), QPointF(dx2, 0.) );
            psky.drawLine( QPointF(dx2, 0.), QPointF(0., dy2) );
            psky.drawLine( QPointF(0., dy2), QPointF(dx1, 0.) );
            psky.drawLine( QPointF(dx1, 0.), QPointF(0., dy1) );
        } else {
            int idx1 = int(dx1); int idy1 = int(dy1);
            int idx2 = int(dx2); int idy2 = int(dy2);
            psky.drawLine( QPoint(0, idy1), QPoint(idx2, 0) );
            psky.drawLine( QPoint(idx2, 0), QPoint(0, idy2) );
            psky.drawLine( QPoint(0, idy2), QPoint(idx1, 0) );
            psky.drawLine( QPoint(idx1, 0), QPoint(0, idy1) );
        }

        psky.restore(); //reset coordinate system
        break;
    case 8: //Galaxy
    case 16: // Quasar
    case 18: // Radio Source
        if ( size <1. && zoom > 20*MINZOOM ) size = 3.; //force ellipse above zoomFactor 20
        if ( size <1. && zoom > 5*MINZOOM ) size = 1.; //force points above zoomFactor 5
        if ( size>2. ) {
            psky.save();
            psky.translate( x, y );
            psky.rotate( PositionAngle );  //rotate the coordinate system

            if ( Options::useAntialias() ) {
                psky.drawEllipse( QRectF(dx1, dy1, size, e()*size) );
            } else {
                int idx1 = int(dx1); int idy1 = int(dy1);
                psky.drawEllipse( QRect(idx1, idy1, isize, int(e()*size)) );
            }

            psky.restore(); //reset coordinate system

        } else if ( size>0. ) {
            psky.drawPoint( QPointF(x, y) );
        }
        break;
    case 14: // Galaxy cluster - draw a circle of + marks
        tempBrush = psky.brush();
        psky.setBrush( psky.pen().color() );
        psize = 1.;
        if ( size > 50. )  psize *= 2.;

        if ( Options::useAntialias() ) {
            psky.drawLine( QLineF( xa-psize, y1, xa+psize, y1 ) );
            psky.drawLine( QLineF( xa, y1-psize, xa, y1+psize ) );
            psky.drawLine( QLineF( xb-psize, y1, xa+psize, y1 ) );
            psky.drawLine( QLineF( xb, y1-psize, xa, y1+psize ) );
            psky.drawLine( QLineF( xa-psize, y2, xa+psize, y1 ) );
            psky.drawLine( QLineF( xa, y2-psize, xa, y1+psize ) );
            psky.drawLine( QLineF( xb-psize, y2, xa+psize, y1 ) );
            psky.drawLine( QLineF( xb, y2-psize, xa, y1+psize ) );
            psky.drawLine( QLineF( x1-psize, ya, xa+psize, y1 ) );
            psky.drawLine( QLineF( x1, ya-psize, xa, y1+psize ) );
            psky.drawLine( QLineF( x1-psize, yb, xa+psize, y1 ) );
            psky.drawLine( QLineF( x1, yb-psize, xa, y1+psize ) );
            psky.drawLine( QLineF( x2-psize, ya, xa+psize, y1 ) );
            psky.drawLine( QLineF( x2, ya-psize, xa, y1+psize ) );
            psky.drawLine( QLineF( x2-psize, yb, xa+psize, y1 ) );
            psky.drawLine( QLineF( x2, yb-psize, xa, y1+psize ) );
        } else {
            int ix1 = int(x1); int iy1 = int(y1);
            int ix2 = int(x2); int iy2 = int(y2);
            int ixa = int(xa); int iya = int(ya);
            int ixb = int(xb); int iyb = int(yb);
            psky.drawLine( QLine( ixa - int(psize), iy1, ixa + int(psize), iy1 ) );
            psky.drawLine( QLine( ixa, iy1 - int(psize), ixa, iy1 + int(psize) ) );
            psky.drawLine( QLine( ixb - int(psize), iy1, ixa + int(psize), iy1 ) );
            psky.drawLine( QLine( ixb, iy1 - int(psize), ixa, iy1 + int(psize) ) );
            psky.drawLine( QLine( ixa - int(psize), iy2, ixa + int(psize), iy1 ) );
            psky.drawLine( QLine( ixa, iy2 - int(psize), ixa, iy1 + int(psize) ) );
            psky.drawLine( QLine( ixb - int(psize), iy2, ixa + int(psize), iy1 ) );
            psky.drawLine( QLine( ixb, iy2 - int(psize), ixa, iy1 + int(psize) ) );
            psky.drawLine( QLine( ix1 - int(psize), iya, ixa + int(psize), iy1 ) );
            psky.drawLine( QLine( ix1, iya - int(psize), ixa, iy1 + int(psize) ) );
            psky.drawLine( QLine( ix1 - int(psize), iyb, ixa + int(psize), iy1 ) );
            psky.drawLine( QLine( ix1, iyb - int(psize), ixa, iy1 + int(psize) ) );
            psky.drawLine( QLine( ix2 - int(psize), iya, ixa + int(psize), iy1 ) );
            psky.drawLine( QLine( ix2, iya - int(psize), ixa, iy1 + int(psize) ) );
            psky.drawLine( QLine( ix2 - int(psize), iyb, ixa + int(psize), iy1 ) );
            psky.drawLine( QLine( ix2, iyb - int(psize), ixa, iy1 + int(psize) ) );
        }
        psky.setBrush( tempBrush );
        break;
    }
}

bool DeepSkyObject::drawImage( QPainter &psky, float x, float y, double PositionAngle, double zoom ) {
    QImage *image = readImage();
    QImage ScaledImage;

    if ( image ) {
        double scale = SkyMap::Instance()->scale();
        float w = a() * scale * dms::PI * zoom/10800.0;

        if ( w < 0.75*psky.window().width() ) {
            float h = w*image->height()/image->width(); //preserve image's aspect ratio
            float dx = 0.5*w;
            float dy = 0.5*h;
            ScaledImage = image->scaled( int(w), int(h) );
            psky.save();
            psky.translate( x, y );
            psky.rotate( PositionAngle );  //rotate the coordinate system

            if ( Options::useAntialias() )
                psky.drawImage( QPointF( -dx, -dy ), ScaledImage );
            else
                psky.drawImage( QPoint( -1*int(dx), -1*int(dy) ), ScaledImage );

            psky.restore();
        }
        return true;
    }
    else 
        return false;
}

double DeepSkyObject::labelOffset() const {
    //Calculate object size in pixels
    double majorAxis = a();
    double minorAxis = b();
    if ( majorAxis == 0.0 && type() == 1 ) { //catalog stars
      majorAxis = 1.0;
      minorAxis = 1.0;
    }
    double scale = SkyMap::Instance()->scale();
    double size = ((majorAxis + minorAxis) / 2.0 ) * scale * dms::PI * Options::zoomFactor()/10800.0;
    return 0.5*size + 4.;
}

QString DeepSkyObject::labelString() const {
    QString oName;
    if( Options::showDeepSkyNames() ) {
        if( Options::deepSkyLongLabels() && translatedLongName() != translatedName() )
            oName = translatedLongName() + " (" + translatedName() + ')';
        else
            oName = translatedName();
    }

    if( Options::showDeepSkyMagnitudes() ) {
        if( Options::showDeepSkyNames() )
            oName += "; ";
        oName += KGlobal::locale()->formatNumber( mag(), 1 );
    }

    return oName;
}


SkyObject::UID DeepSkyObject::getUID() const
{
    // mag takes 10 bit
    SkyObject::UID m = mag()*10; 
    if( m < 0 ) m = 0;

    // Both RA & dec fits in 24-bits
    SkyObject::UID ra  = ra0().Degrees() * 36000;
    SkyObject::UID dec = (ra0().Degrees()+91) * 36000;

    assert("Magnitude is expected to fit into 10bits" && m>=0 && m<(1<<10));
    assert("RA should fit into 24bits"  && ra>=0  && ra <(1<<24));
    assert("Dec should fit into 24bits" && dec>=0 && dec<(1<<24));

    // Choose kind of 
    SkyObject::UID kind = type() == SkyObject::GALAXY ? SkyObject::UID_GALAXY : SkyObject::UID_DEEPSKY;
    return (kind << 60) | (m << 48) | (ra << 24) | dec;
}
