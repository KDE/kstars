/***************************************************************************
                          plotwidget.cpp - A widget for plotting in KStars
                             -------------------
    begin                : Sun 18 May 2003
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

#include <math.h> //for log10(), pow(), modf()
#include <kdebug.h>
#include <qcolor.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qstring.h>

#include "plotwidget.h"

PlotWidget::PlotWidget( double x1, double x2, double y1, double y2, QWidget *parent, const char* name )
 : QWidget( parent, name ), XS1( XPADDING ), YS1( YPADDING ),
   dXtick1(0.0), dYtick1(0.0), dXtick2(0.0), dYtick2(0.0),
   nmajX1(0), nmajX2(0), nminX1(0), nminX2(0), nmajY1(0), nmajY2(0), nminY1(0), nminY2(0),
   XAxisType(DOUBLE), YAxisType(DOUBLE) {

	setLimits( x1, x2, y1, y2 );
	setSecondaryLimits( 0.0, 0.0, 0.0, 0.0 );

	//Set dXS, XS2, dYS, YS2
	XS2 = width() - XS1;
	YS2 = height() - YS2;
	dXS = XS2 - XS1;
	dYS = YS2 - YS1;

	buffer = new QPixmap();

	//default colors:
	setBGColor( QColor( "black" ) );
	setFGColor( QColor( "white" ) );
	setGridColor( QColor( "grey" ) );

	ObjectList.setAutoDelete( TRUE );
}

void PlotWidget::setLimits( double x1, double x2, double y1, double y2 ) {
	if (x2<x1) { XA1=x2; XA2=x1; }
	else { XA1=x1; XA2=x2; }
	if ( y2<y1) { YA1=y2; YA2=y1; }
	else { YA1=y1; YA2=y2; }

	dXA=XA2-XA1; dYA=YA2-YA1;
	DataRect = DRect( XA1, YA1, dXA, dYA );

	updateTickmarks();
}

void PlotWidget::setSecondaryLimits( double x1, double x2, double y1, double y2 ) {
	if (x2<x1) { XB1=x2; XB2=x1; }
	else { XB1=x1; XB2=x2; }
	if ( y2<y1) { YB1=y2; YB2=y1; }
	else { YB1=y1; YB2=y2; }

	dXB=XB2-XB1; dYB=YB2-YB1;

	updateTickmarks();
}

void PlotWidget::updateTickmarks() {
	// Determine the number and spacing of tickmarks for the current plot limits.

	if ( x1()==x2() ) {
		kdWarning() << "X range invalid! " << x1() << " to " << x2() << endl;
		XA1=0.0; XA2=1.0;
		return;
	}
	if ( y1()==y2() ) {
		kdWarning() << "Y range invalid! " << y1() << " to " << y2() << endl;
		YA1=0.0; YA2=1.0;
		return;
	}

	AXIS_TYPE type(DOUBLE);
	int nmajor(0), nminor(0);
	double z1(0.0), z2(0.0), zb1(0.0), zb2(0.0), dzb(0.0);
	double Range(0.0), s(0.0), t(0.0), pwr(0.0), dTick(0.0);

	//loop over X and Y axes...the z variables substitute for either X or Y
	for ( unsigned int iaxis=0; iaxis<2; ++iaxis ) {
		if ( iaxis == 1 ) {
			z1 = x1(); z2 = x2(); zb1 = xb1(); zb2 = xb2(); dzb = dXB;
			type = xAxisType();
		} else {
			z1 = y1(); z2 = y2(); zb1 = yb1(); zb2 = yb2(); dzb = dYB;
			type = yAxisType();
		}

		int ntry(1);
		if ( dzb > 0.0 ) ntry=2; //secondary limits are defined

		for ( unsigned int itry=1; itry<=ntry; ++itry ) {
			//determine size of region to be drawn, in draw units
			if ( itry==1 ) Range = z2 - z1;
			else           Range = zb2 - zb1;

			//we switch from TIME type to DOUBLE type if :
			// Range <1 (we measure in minutes) or
			// Range >36 (we measure in days)
			if ( type==TIME ) {
				if ( Range > 36.0 ) {
					type = DOUBLE;
					Range /= 24.0;
				} else if ( Range < 1.0 ) {
					type = DOUBLE;
					Range *= 60.0;
				}
			}

			//we switch from ANGLE type to DOUBLE type if :
			// Range <1 (we measure in arcminutes) or
			// Range >450 (== 1.25 revolutions) (we still measure in degrees, but use DOUBLE rules)
			if ( type==ANGLE ) {
				if ( Range > 450.0 ) {
					type = DOUBLE;
				} else if ( Range < 1.0 ) {
					type = DOUBLE;
					Range *= 60.0;
				}
			}

			switch ( type ) {
				case DOUBLE :
				{
					//s is the power-of-ten factor of Range:
					//Range = t * s; s = 10^(pwr).  e.g., Range=350.0 then t=3.5, s = 100.0; pwr = 2.0
					modf( log10(Range), &pwr );
					s = pow( 10.0, pwr );
					t = Range/s;

					//adjust s and t such that t is between 3 and 5:
					if ( t < 3.0 ) { t *= 10.0; s /= 10.0; } //t now btwn 3 and 30
					if ( t < 6.0 ) { //accept current values
						dTick = s;
						nmajor = int(t);
						nminor = 5;
					} else if ( t < 10.0 ) { //factor of 2
						dTick = s*2.0;
						nmajor = int(t/2.0);
						nminor = 4;
					} else if ( t < 20.0 ) { //factor of 4
						dTick = s*4.0;
						nmajor = int(t/4.0);
						nminor = 4;
					} else { //factor of 5
						dTick = s*5.0;
						nmajor = int(t/5.0);
						nminor = 5;
					}

					break;
				} // end case DOUBLE

				case TIME:
				{
					if ( Range < 3.0 ) {
						dTick = 0.5;
						nmajor = int(Range/dTick);
						nminor = 3;
					} else if ( Range < 6.0 ) {
						dTick = 1.0;
						nmajor = int(Range/dTick);
						nminor = 4;
					} else if ( Range < 12.0 ) {
						dTick = 2.0;
						nmajor = int(Range/dTick);
						nminor = 4;
					} else {
						dTick = 4.0;
						nmajor = int(Range/dTick);
						nminor = 4;
					}

					break;
				} //end case TIME

				case ANGLE:
				{
					if ( Range < 3.0 ) {
						dTick = 0.5;
						nmajor = int(Range/dTick);
						nminor = 3;
					} else if ( Range < 6.0 ) {
						dTick = 1.0;
						nmajor = int(Range/dTick);
						nminor = 4;
					} else if ( Range < 12.0 ) {
						dTick = 2.0;
						nmajor = int(Range/dTick);
						nminor = 4;
					} else if ( Range < 20.0 ) {
						dTick = 4.0;
						nmajor = int(Range/dTick);
						nminor = 5;
					} else if ( Range < 30.0 ) {
						dTick = 5.0;
						nmajor = int(Range/dTick);
						nminor = 5;
					} else if ( Range < 60.0 ) {
						dTick = 10.0;
						nmajor = int(Range/dTick);
						nminor = 5;
					} else if ( Range < 190.0 ) {
						dTick = 30.0;
						nmajor = int(Range/dTick);
						nminor = 3;
					} else {
						dTick = 45.0;
						nmajor = int(Range/dTick);
						nminor = 3;
					}

					break;
				} //end case TIME
			} //end type switch

			if ( iaxis==1 ) { //X axis
				if ( itry==1 ) {
					nmajX1 = nmajor;
					nminX1 = nminor;
					dXtick1 = dTick;
				} else {
					nmajX2 = nmajor;
					nminX2 = nminor;
					dXtick2 = dTick;
				}
			} else { //Y axis
				if ( itry==1 ) {
					nmajY1 = nmajor;
					nminY1 = nminor;
					dYtick1 = dTick;
				} else {
					nmajY2 = nmajor;
					nminY2 = nminor;
					dYtick2 = dTick;
				}
			} //end if iaxis
		} //end for itry
	} //end for iaxis
}

void PlotWidget::resizeEvent( QResizeEvent *e ) {
	XS2 = width()  - XPADDING;
	YS2 = height() - YPADDING;
	dXS = XS2 - XS1;
	dYS = YS2 - YS1;
	PixRect = QRect( 0, 0, dXS, dYS );

	buffer->resize( width(), height() );
}

void PlotWidget::paintEvent( QPaintEvent *e ) {
	QPainter p;

	p.begin( buffer );
	p.fillRect( 0, 0, width(), height(), bgColor() );

	p.translate( XS1, YS1 );

	drawBox( &p, true, true, true, true );
	drawObjects( &p );
	p.end();

	bitBlt( this, 0, 0, buffer );
}

void PlotWidget::drawObjects( QPainter *p ) {
	for ( PlotObject *po = ObjectList.first(); po; po = ObjectList.next() ) {
		//draw the plot object
		p->setPen( QColor( po->color() ) );

		switch ( po->type() ) {
			case PlotObject::POINTS :
			{
				p->setBrush( QColor( po->color() ) );

				for ( DPoint *dp = po->points()->first(); dp; dp = po->points()->next() ) {
					QPoint q = dp->qpoint( PixRect, DataRect );
					int x1 = q.x() - po->size()/2;
					int y1 = q.y() - po->size()/2;

					switch( po->param() ) {
						case PlotObject::CIRCLE : p->drawEllipse( x1, y1, po->size(), po->size() ); break;
						case PlotObject::SQUARE : p->drawRect( x1, y1, po->size(), po->size() ); break;
						case PlotObject::LETTER : p->drawText( q, po->name().left(1) ); break;
						default: p->drawPoint( q );
					}
				}

				p->setBrush( Qt::NoBrush );
				break;
			}

			case PlotObject::CURVE :
			{
				p->setPen( QPen( QColor( po->color() ), po->size(), (QPen::PenStyle)po->param() ) );
				DPoint *dp = po->points()->first();
				p->moveTo( dp->qpoint( PixRect, DataRect ) );
				for ( dp = po->points()->next(); dp; dp = po->points()->next() )
					p->lineTo( dp->qpoint( PixRect, DataRect ) );
				break;
			}

			case PlotObject::LABEL :
			{
				QPoint q = po->points()->first()->qpoint( PixRect, DataRect );
				p->drawText( q, po->name() );
				break;
			}
		}
	}
}

double PlotWidget::dmod( double a, double b ) { return ( b * ( ( a / b ) - int( a / b ) ) ); }

void PlotWidget::drawBox( QPainter *p, bool showAxes, bool showTickMarks, bool showTickLabels, bool showGrid ) {
	//First, fill in padding region with bgColor() to mask out-of-bounds plot data
	p->setPen( bgColor() );
	p->setBrush( bgColor() );
	//left padding
	p->drawRect( -XS1, -YS1, XS1, height() );
	//right padding
	p->drawRect( dXS, -YS1, XS1, height() );
	//top padding
	p->drawRect( 0, -YS1, dXS, YS1 );
	//bottom padding
	p->drawRect( 0, dYS, dXS, YS1 );

	if ( showGrid ) {
		//Grid lines are placed at locations of primary axes' major tickmarks

		p->setPen( gridColor() );

		//vertical grid lines
		double x0 = XA1 - dmod( XA1, dXtick1 ); //zeropoint; x(i) is this plus i*dXtick1
		for ( int ix = 0; ix <= nmajX1+1; ix++ ) {
			int px = int( dXS * ( (x0 + ix*dXtick1 - XA1)/dXA ) );
			p->drawLine( px, 0, px, dYS );
		}

		//horizontal grid lines
		double y0 = YA1 - dmod( YA1, dYtick1 ); //zeropoint; y(i) is this plus i*mX
		for ( int iy = 0; iy <= nmajY1+1; iy++ ) {
			int py = int( dYS * ( (y0 + iy*dYtick1 - YA1)/dYA ) );
			p->drawLine( 0, py, dXS, py );
		}
	}

	p->setPen( fgColor() );
	p->setBrush( Qt::NoBrush );

	if ( showAxes ) p->drawRect( PixRect ); //box outline

	if ( showTickMarks ) {
		//spacing between minor tickmarks (in data units)
		double dminX = dXtick1/nminX1;
		double dminY = dYtick1/nminY1;

		bool secondaryLimits( false );
		if ( dXB > 0.0 ) secondaryLimits = true;

		//--- Draw primary X tickmarks on bottom axis---//
		double x0 = XA1 - dmod( XA1, dXtick1 ); //zeropoint; tickmark i is this plus i*dXtick1 (in data units)
		if ( XA1 < 0 ) x0 -= dXtick1;

		for ( int ix = 0; ix <= nmajX1+1; ix++ ) {
			int px = int( dXS * ( (x0 + ix*dXtick1 - XA1)/dXA ) ); //position of tickmark i (in screen units)
			if ( px > 0 && px < dXS ) {
				p->drawLine( px, dYS - 2, px, dYS - BIGTICKSIZE - 2 ); //move tickmarks 2 pixels (avoids sticking out other side)
				if ( !secondaryLimits ) p->drawLine( px, 0, px, BIGTICKSIZE );
			}

			//tick label
			if ( showTickLabels ) {
				double lab = x0 + ix*dXtick1;
				if ( fabs(lab)/dXtick1 < 0.00001 ) lab = 0.0; //fix occassional roundoff error with "0.0" label

				switch ( xAxisType() ) {
					case DOUBLE :
					{
						QString str = QString( "%1" ).arg( lab, 0, 'g', 2 );
						if ( px > 0 && px < dXS ) p->drawText( px - BIGTICKSIZE, dYS + 2*BIGTICKSIZE, str );
						break;
					}
					case TIME :
					{
						int h = int(lab);
						int m = int(60.*(lab - h));
						QString str = QString().sprintf( "%02d:%02d", h, m );
						if ( px > 0 && px < dXS ) p->drawText( px - 2*BIGTICKSIZE, dYS + 2*BIGTICKSIZE, str );
						break;
					}
					case ANGLE :
					{
						QString str = QString().sprintf( "%d%c", int(lab), 176 );
						if ( px > 0 && px < dXS ) p->drawText( px - BIGTICKSIZE, dYS + 2*BIGTICKSIZE, str );
						break;
					}
				}
			}

			//draw minor ticks
			for ( int j=0; j < nminX1; j++ ) {
				int pmin = int( px + dXS*j*dminX/dXA ); //position of minor tickmark j (in screen units)

				if ( pmin > 0 && pmin < dXS ) {
					p->drawLine( pmin, dYS - 2, pmin, dYS - SMALLTICKSIZE - 2 );
					if ( !secondaryLimits ) p->drawLine( pmin, 0, pmin, SMALLTICKSIZE );
				}
			}
		}

		//--- Draw primary Y tickmarks on left axis ---//
		double y0 = YA1 - dmod( YA1, dYtick1 ); //zeropoint; tickmark i is this plus i*dYtick1 (in data units)
		if ( YA1 < 0 ) y0 -= dYtick1;

		for ( int iy = 0; iy <= nmajY1+1; iy++ ) {
			int py = dYS - int( dYS * ( (y0 + iy*dYtick1 - YA1)/dYA ) ); //position of tickmark i (in screen units)
			if ( py > 0 && py < dYS ) {
				p->drawLine( 0, py, BIGTICKSIZE, py );
				if ( !secondaryLimits ) p->drawLine( dXS, py, dXS-BIGTICKSIZE, py );
			}

			//tick label
			if ( showTickLabels ) {
				double lab = y0 + iy*dYtick1;
				if ( fabs(lab)/dYtick1 < 0.00001 ) lab = 0.0; //fix occassional roundoff error with "0.0" label

				switch ( yAxisType() ) {
					case DOUBLE :
					{
						QString str = QString( "%1" ).arg( lab, 0, 'g', 2 );
						if ( py > 0 && py < dYS ) p->drawText( -2*BIGTICKSIZE, py + SMALLTICKSIZE, str );
						break;
					}
					case TIME :
					{
						int h = int(lab);
						int m = int(60.*(lab - h));
						QString str = QString().sprintf( "%02d:%02d", h, m );
						if ( py > 0 && py < dYS ) p->drawText( -2*BIGTICKSIZE, py + SMALLTICKSIZE, str );
						break;
					}
					case ANGLE :
					{
						QString str = QString().sprintf( "%d%c", int(lab), 176 );
						if ( py > 0 && py < dYS ) p->drawText(-3*BIGTICKSIZE, py + SMALLTICKSIZE, str );
						break;
					}
				}
			}

			//minor ticks
			for ( int j=0; j < nminY1; j++ ) {
				int pmin = int( py - dYS*j*dminY/dYA ); //position of minor tickmark j (in screen units)
				if ( pmin > 0 && pmin < dYS ) {
					p->drawLine( 0, pmin, SMALLTICKSIZE, pmin );
					if ( !secondaryLimits ) p->drawLine( dXS, pmin, dXS-SMALLTICKSIZE, pmin );
				}
			}
		}

		if ( secondaryLimits ) {
			double dminX2 = dXtick2/nminX2;
			double dminY2 = dYtick2/nminY2;

			//--- Draw secondary X tickmarks on top axis---//
			double x0 = XB1 - dmod( XB1, dXtick2 ); //zeropoint; tickmark i is this plus i*dXtick2 (in data units)

			for ( int ix = 0; ix <= nmajX2; ix++ ) {
				int px = int( dXS * ( (x0 + ix*dXtick2 - XB1)/dXB ) ); //position of tickmark i (in screen units)
				if ( px > 0 && px < dXS ) p->drawLine( px, 0, px, BIGTICKSIZE );

				//tick label
				if ( showTickLabels ) {
					double lab = x0 + ix*dXtick2;
					if ( fabs(lab)/dXtick2 < 0.00001 ) lab = 0.0; //fix occassional roundoff error with "0.0" label

					switch ( xAxisType() ) {
						case DOUBLE :
						{
							QString str = QString( "%1" ).arg( lab, 0, 'g', 2 );
							if ( px > 0 && px < dXS ) p->drawText( px - BIGTICKSIZE, -2*BIGTICKSIZE, str );
							break;
						}
						case TIME :
						{
							int h = int(lab);
							int m = int(60.*(lab - h));
							QString str = QString().sprintf( "%02d:%02d", h, m );
							if ( px > 0 && px < dXS ) p->drawText( px - 2*BIGTICKSIZE, -2*BIGTICKSIZE, str );
							break;
						}
						case ANGLE :
						{
							QString str = QString().sprintf( "%d%c", int(lab), 176 );
							if ( px > 0 && px < dXS ) p->drawText( px - BIGTICKSIZE, -2*BIGTICKSIZE, str );
							break;
						}
					}
				}

				//draw minor ticks
				for ( int j=0; j < nminX2; j++ ) {
					int pmin = int( px + dXS*j*dminX2/dXB ); //position of minor tickmark j (in screen units)
					if ( pmin > 0 && pmin < dXS ) p->drawLine( pmin, 0, pmin, SMALLTICKSIZE );
				}
			}

			//--- Draw secondary Y tickmarks on right axis ---//
			double y0 = YB1 - dmod( YB1, dYtick2 ); //zeropoint; tickmark i is this plus i*mX (in data units)

			for ( int iy = 0; iy <= nmajY2; iy++ ) {
				int py = dYS - int( dYS * ( (y0 + iy*dYtick2 - YB1)/dYB ) ); //position of tickmark i (in screen units)
				if ( py > 0 && py < dYS ) p->drawLine( dYS, py, dXS-BIGTICKSIZE, py );

				//tick label
				if ( showTickLabels ) {
					double lab = y0 + iy*dYtick2;
					if ( fabs(lab)/dYtick2 < 0.00001 ) lab = 0.0; //fix occassional roundoff error with "0.0" label

					switch ( yAxisType() ) {
						case DOUBLE :
						{
							QString str = QString( "%1" ).arg( lab, 0, 'g', 2 );
							if ( py > 0 && py < dYS ) p->drawText( dXS + 2*BIGTICKSIZE, py + SMALLTICKSIZE, str );
							break;
						}
						case TIME :
						{
							int h = int(lab);
							int m = int(60.*(lab - h));
							QString str = QString().sprintf( "%02d:%02d", h, m );
							if ( py > 0 && py < dYS ) p->drawText( dXS + 2*BIGTICKSIZE, py + SMALLTICKSIZE, str );
							break;
						}
						case ANGLE :
						{
							QString str = QString().sprintf( "%d%c", int(lab), 176 );
							if ( py > 0 && py < dYS ) p->drawText( dXS + 3*BIGTICKSIZE, py + SMALLTICKSIZE, str );
							break;
						}
					}
				}

				//minor ticks
				for ( int j=0; j < nminY2; j++ ) {
					int pmin = py - int( dYS*j*dminY2/dYB ); //position of minor tickmark j (in screen units)
					if ( pmin > 0 && pmin < dYS ) p->drawLine( dXS, pmin, dXS-SMALLTICKSIZE, pmin );
				}
			}

		} //end if ( secondaryLimits )

	} //end if ( showTickmarks )
}

#include "plotwidget.moc"
