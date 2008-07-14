/***************************************************************************
                          avtplotwidget.cpp  -  description
                             -------------------
    begin                : Sat Nov 10 2007
    copyright            : (C) 2007 by Jason Harris
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

#include "avtplotwidget.h"

#include <QWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QTime>
#include <QLinearGradient>

#include <KGlobal>
#include <KLocale>
#include <KPlotObject>
#include <kdebug.h>

AVTPlotWidget::AVTPlotWidget( QWidget *parent )
        : KPlotWidget( parent )
{
    setAntialiasing(true);

    //Default SunRise/SunSet values
    SunRise = 0.25;
    SunSet = 0.75;

    MousePoint = QPoint( -1, -1 );
}

void AVTPlotWidget::mousePressEvent( QMouseEvent *e ) {
    mouseMoveEvent( e );
}

void AVTPlotWidget::mouseDoubleClickEvent( QMouseEvent * ) {
    MousePoint = QPoint(-1, -1);
    update();
}

void AVTPlotWidget::mouseMoveEvent( QMouseEvent *e ) {
    QRect checkRect( leftPadding(), topPadding(), pixRect().width(), pixRect().height() );
    int Xcursor = e->x();
    int Ycursor = e->y();

    if ( ! checkRect.contains( e->x(), e->y() ) ) {
        if ( e->x() < checkRect.left() )   Xcursor = checkRect.left();
        if ( e->x() > checkRect.right() )  Xcursor = checkRect.right();
        if ( e->y() < checkRect.top() )    Ycursor = checkRect.top();
        if ( e->y() > checkRect.bottom() ) Ycursor = checkRect.bottom();
    }

    Xcursor -= leftPadding();
    Ycursor -= topPadding();

    MousePoint = QPoint( Xcursor, Ycursor );
    update();
}

void AVTPlotWidget::paintEvent( QPaintEvent *e ) {
    Q_UNUSED(e)

    QPainter p;

    p.begin( this );
    p.setRenderHint( QPainter::Antialiasing, antialiasing() );
    p.fillRect( rect(), backgroundColor() );
    p.translate( leftPadding(), topPadding() );

    setPixRect();
    p.setClipRect( pixRect() );
    p.setClipping( true );

    int pW = pixRect().width();
    int pH = pixRect().height();

    QColor SkyColor( 0, 100, 200 );

    //draw daytime sky if the Sun rises for the current date/location
    //(when Sun does not rise, SunSet = -1.0)
    if ( SunSet != -1.0 ) {
        //If Sun does not set, then just fill the daytime sky color
        if ( SunSet == 1.0 ) {
            p.fillRect( 0, 0, pW, int(0.5*pH), SkyColor );
        } else {
            //Display centered on midnight, so need to modulate dawn/dusk by 0.5
            int dawn = int(pW*(0.5 + SunRise));
            int dusk = int(pW*(SunSet - 0.5));
            p.fillRect( 0, 0, dusk, int(0.5*pH), SkyColor );
            p.fillRect( dawn, 0, pW - dawn, int(0.5*pH), SkyColor );

            //draw twilight gradients
	    QLinearGradient grad = QLinearGradient( QPointF(dusk-20.0, 0.0), QPointF(dusk+20.0, 0.0) );
	    grad.setColorAt(0, SkyColor );
	    grad.setColorAt(1, Qt::black );
	    p.fillRect( QRectF( dusk-20.0, 0.0, 40.0, pH ), grad );
	    
	    grad.setStart( QPointF(dawn+20.0, 0.0) );
	    grad.setFinalStop( QPointF(dawn-20.0, 0.0) );
	    p.fillRect( QRectF( dawn-20.0, 0.0, 40.0, pH ), grad );
	}
    }
	    
    //draw ground
    p.fillRect( 0, int(0.5*pH), pW, int(0.5*pH), QColor( "#002200" ) );

    foreach( KPlotObject *po, plotObjects() )
    po->draw( &p, this );

    p.setClipping( false );
    drawAxes( &p );

    //Add vertical line indicating "now"
    QTime t = QTime::currentTime();
    double x = 12.0 + t.hour() + t.minute()/60.0 + t.second()/3600.0;
    while ( x > 24.0 ) x -= 24.0;
    int ix = int(x*pW/24.0); //convert to screen pixel coords
    p.setPen( QPen( QBrush("white"), 2.0, Qt::DotLine ) );
    p.drawLine( ix, 0, ix, pH );

    //Label this vertical line with the current time
    p.save();
    QFont smallFont = p.font();
    smallFont.setPointSize( smallFont.pointSize() - 2 );
    p.setFont( smallFont );
    p.translate( ix + 10, pH - 20 );
    p.rotate(-90);
    p.drawText(0, 0, KGlobal::locale()->formatTime( t ) );
    p.restore();

    //Draw crosshairs at clicked position
    if ( MousePoint.x() > 0 ) {
        p.setPen( QPen( QBrush("gold"), 1.0, Qt::SolidLine ) );
        p.drawLine( QLineF( MousePoint.x()+0.5, 0.5, MousePoint.x()+0.5, pixRect().height()-0.5 ) );
        p.drawLine( QLineF( 0.5, MousePoint.y()+0.5, pixRect().width()-0.5, MousePoint.y()+0.5 ) );

        //Label each crosshair line (time and altitude)
        p.setFont( smallFont );
        double a = (pH - MousePoint.y())*180.0/pH - 90.0;
        p.drawText( 20, MousePoint.y() + 10, QString::number(a,'f',2) + QChar(176) );

        double h = MousePoint.x()*24.0/pW - 12.0;
        if ( h < 0.0 ) h += 24.0;
        t = QTime( int(h), int(60.*(h - int(h))) );
        p.save();
        p.translate( MousePoint.x() + 10, pH - 20 );
        p.rotate(-90);
        p.drawText( 0, 0, KGlobal::locale()->formatTime( t ) );
        p.restore();
    }

    p.end();
}

#include "avtplotwidget.moc"
