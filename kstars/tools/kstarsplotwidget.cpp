/***************************************************************************
                          kstarsplotwidget.cpp - A widget for data plotting in KStars
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
#include <kglobal.h>
#include <klocale.h>
#include <qcolor.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qstring.h>

#include "kstarsplotwidget.h"



KStarsPlotWidget::KStarsPlotWidget( double x1, double x2, double y1, double y2, QWidget *parent, const char* name )
 : KPlotWidget( x1, x2, y1, y2, parent, name ),
   dXtick2(0.0), dYtick2(0.0),
   nmajX2(0), nminX2(0), nmajY2(0), nminY2(0),
   XAxisType(DOUBLE), YAxisType(DOUBLE), XAxisType_0(DOUBLE), YAxisType_0(DOUBLE),
   XScaleFactor(1.0), YScaleFactor(1.0)  
    {

	setLimits( x1, x2, y1, y2 );
	setSecondaryLimits( 0.0, 0.0, 0.0, 0.0 );
}

void KStarsPlotWidget::setLimits( double x1, double x2, double y1, double y2 ) {
	double X1=0, X2=0, Y1=0, Y2=0;
	if (x2<x1) { X1=x2; X2=x1; }
	else { X1=x1; X2=x2; }
	if ( y2<y1) { Y1=y2; Y2=y1; }
	else { Y1=y1; Y2=y2; }

	DataRect = DRect( X1, Y1, X2 - X1, Y2 - Y1 );
	checkLimits();

	updateTickmarks();
}

void KStarsPlotWidget::setSecondaryLimits( double x1, double x2, double y1, double y2 ) {
	double XB1=0, XB2=0, YB1=0, YB2=0;
	if (x2<x1) { XB1=x2; XB2=x1; }
	else { XB1=x1; XB2=x2; }
	if ( y2<y1) { YB1=y2; YB2=y1; }
	else { YB1=y1; YB2=y2; }

	DataRect2 = DRect( XB1, YB1, XB2 - XB1, YB2 - YB1 );
	updateTickmarks();
}

void KStarsPlotWidget::checkLimits() {
	AXIS_TYPE type(DOUBLE);
	double Range(0.0), sc(1.0);

	for ( unsigned int i=0; i<2; ++i ) {
		if ( i==0 ) {
			type = xAxisType0();
			Range = DataRect.x2() - DataRect.x();
		} else {
			type = yAxisType0();
			Range = DataRect.y2() - DataRect.y();
		}

		//we switch from TIME type to DOUBLE type if :
		// Range >36 (we measure in days) or
		// Range <1 (we measure in minutes)
		if ( type==TIME ) {
			if ( Range > 36.0 ) {
				type = DOUBLE;
				sc = 1.0/24.0;
			} else if ( Range < 1.0 ) {
				type = DOUBLE;
				sc = 60.0;
			}
		}

		//we switch from ANGLE type to DOUBLE type if :
		// Range >450 (== 1.25 revolutions) (we still measure in degrees, but use DOUBLE rules) or
		// Range <1 (we measure in arcminutes)
		if ( type==ANGLE ) {
			if ( Range > 450.0 ) {
				type = DOUBLE;
			} else if ( Range < 1.0 ) {
				type = DOUBLE;
				sc = 60.0;
			}
		}

		//set the effective DataRect with a bootstrap method; first the x-values
		//(temporarily using the intrinsic DataRect0 for the y-values), then the
		//y-values (using the new x-values already in DataRect)
		if ( i==0 ) {
			setXAxisType( type );
			setXScale( sc );
		} else {
			setYAxisType( type );
			setYScale( sc );
		}
	}
}

void KStarsPlotWidget::updateTickmarks() {
	//This function differs from KPlotWidget::updateTickmarks() in two ways:
	//the placement of tickmarks is dependent on the Data type of the axis,
	//and we add the possibility of secondary limits for the top/right axes.
	if ( dataWidth() == 0.0 ) {
		kdWarning() << "X range invalid! " << x() << " to " << x2() << endl;
		DataRect.setWidth( 1.0 );
		checkLimits();
		return;
	}
	if ( dataHeight() == 0.0 ) {
		kdWarning() << "Y range invalid! " << y() << " to " << y2() << endl;
		DataRect.setHeight( 1.0 );
		checkLimits();
		return;
	}

	AXIS_TYPE type(DOUBLE);
	int nmajor(0), nminor(0);
	double z1(0.0), z2(0.0), zb1(0.0), zb2(0.0), scale(1.0);
	double Range(0.0), s(0.0), t(0.0), pwr(0.0), dTick(0.0);

	//loop over X and Y axes...the z variables substitute for either X or Y
	for ( unsigned int iaxis=0; iaxis<2; ++iaxis ) {
		if ( iaxis == 1 ) {
			z1 = x(); z2 = x2(); zb1 = xb(); zb2 = xb2();
			type = xAxisType();
			scale = xScale();
		} else {
			z1 = y(); z2 = y2(); zb1 = yb(); zb2 = yb2();
			type = yAxisType();
			scale = yScale();
		}

		unsigned int nLimits(1);
		if ( zb2 - zb1 > 0.0 ) nLimits=2; //secondary limits are defined

		for ( unsigned int itry=1; itry<=nLimits; ++itry ) {
			//determine size of region to be drawn, in draw units
			if ( itry==1 ) Range = scale*(z2 - z1);
			else           Range = scale*(zb2 - zb1);

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
						dTick = s/scale;
						nmajor = int(t);
						nminor = 5;
					} else if ( t < 10.0 ) { //factor of 2
						dTick = s*2.0/scale;
						nmajor = int(t/2.0);
						nminor = 4;
					} else if ( t < 20.0 ) { //factor of 4
						dTick = s*4.0/scale;
						nmajor = int(t/4.0);
						nminor = 4;
					} else { //factor of 5
						dTick = s*5.0/scale;
						nmajor = int(t/5.0);
						nminor = 5;
					}

					break;
				} // end case DOUBLE

				case TIME:
				{
					if ( Range < 3.0 ) {
						dTick = 0.5/scale;
						nmajor = int(Range/dTick);
						nminor = 3;
					} else if ( Range < 6.0 ) {
						dTick = 1.0/scale;
						nmajor = int(Range/dTick);
						nminor = 4;
					} else if ( Range < 12.0 ) {
						dTick = 2.0/scale;
						nmajor = int(Range/dTick);
						nminor = 4;
					} else {
						dTick = 4.0/scale;
						nmajor = int(Range/dTick);
						nminor = 4;
					}

					break;
				} //end case TIME

				case ANGLE:
				{
					if ( Range < 3.0 ) {
						dTick = 0.5/scale;
						nmajor = int(Range/dTick);
						nminor = 3;
					} else if ( Range < 6.0 ) {
						dTick = 1.0/scale;
						nmajor = int(Range/dTick);
						nminor = 4;
					} else if ( Range < 12.0 ) {
						dTick = 2.0/scale;
						nmajor = int(Range/dTick);
						nminor = 4;
					} else if ( Range < 20.0 ) {
						dTick = 4.0/scale;
						nmajor = int(Range/dTick);
						nminor = 5;
					} else if ( Range < 30.0 ) {
						dTick = 5.0/scale;
						nmajor = int(Range/dTick);
						nminor = 5;
					} else if ( Range < 60.0 ) {
						dTick = 10.0/scale;
						nmajor = int(Range/dTick);
						nminor = 5;
					} else if ( Range < 190.0 ) {
						dTick = 30.0/scale;
						nmajor = int(Range/dTick);
						nminor = 3;
					} else {
						dTick = 45.0/scale;
						nmajor = int(Range/dTick);
						nminor = 3;
					}

					break;
				} //end case TIME

				case UNKNOWN_TYPE: break;

			} //end type switch

			if ( iaxis==1 ) { //X axis
				if ( itry==1 ) {
					nmajX = nmajor;
					nminX = nminor;
					dXtick = dTick;
				} else {
					nmajX2 = nmajor;
					nminX2 = nminor;
					dXtick2 = dTick;
				}
			} else { //Y axis
				if ( itry==1 ) {
					nmajY = nmajor;
					nminY = nminor;
					dYtick = dTick;
				} else {
					nmajY2 = nmajor;
					nminY2 = nminor;
					dYtick2 = dTick;
				}
			} //end if iaxis
		} //end for itry
	} //end for iaxis
}

void KStarsPlotWidget::drawBox( QPainter *p ) {
	int pW = PixRect.width(), pH = PixRect.height();

	//First, fill in padding region with bgColor() to mask out-of-bounds plot data
	p->setPen( bgColor() );
	p->setBrush( bgColor() );

	//left padding ( don't forget: we have translated by XPADDING, YPADDING )
	p->drawRect( -leftPadding(), -topPadding(), leftPadding(), height() );

	//right padding
	p->drawRect( pW, -topPadding(), rightPadding(), height() );

	//top padding
	p->drawRect( 0, -topPadding(), pW, topPadding() );

	//bottom padding
	p->drawRect( 0, pH, pW, bottomPadding() );

	if ( ShowGrid ) {
		//Grid lines are placed at locations of primary axes' major tickmarks
		p->setPen( gridColor() );

		//vertical grid lines
		double x0 = x() - dmod( x(), dXtick ); //zeropoint; x(i) is this plus i*dXtick1
		for ( int ix = 0; ix <= nmajX+1; ix++ ) {
			int px = int( pW * ( (x0 + ix*dXtick - x())/dataWidth() ) );
			p->drawLine( px, 0, px, pH );
		}

		//horizontal grid lines
		double y0 = y() - dmod( y(), dYtick ); //zeropoint; y(i) is this plus i*mX
		for ( int iy = 0; iy <= nmajY+1; iy++ ) {
			int py = int( pH * ( (y0 + iy*dYtick - y())/dataHeight() ) );
			p->drawLine( 0, py, pW, py );
		}
	}

	p->setPen( fgColor() );
	p->setBrush( Qt::NoBrush );

	if ( LeftAxis.isVisible() || BottomAxis.isVisible() ) p->drawRect( PixRect ); //box outline

	if ( ShowTickMarks ) {
		//spacing between minor tickmarks (in data units)
		double dminX = dXtick/nminX;
		double dminY = dYtick/nminY;

		bool secondaryXLimits( false );
		bool secondaryYLimits( false );
		if ( dataWidth2()  > 0.0 && ( xb() != x() || xb2() != x2() ) ) secondaryXLimits = true;
		if ( dataHeight2() > 0.0 && ( yb() != y() || yb2() != y2() ) ) secondaryYLimits = true;

		//set small font for tick labels
		QFont f = p->font();
		int s = f.pointSize();
		f.setPointSize( s - 2 );
		p->setFont( f );

		//--- Draw primary X tickmarks on bottom axis---//
		double x0 = x() - dmod( x(), dXtick ); //zeropoint; tickmark i is this plus i*dXtick1 (in data units)
		if ( x() < 0 ) x0 -= dXtick;

		for ( int ix = 0; ix <= nmajX+1; ix++ ) {
			int px = int( pW * ( (x0 + ix*dXtick - x())/dataWidth() ) ); //position of tickmark i (in screen units)
			if ( px > 0 && px < pW ) {
				p->drawLine( px, pH - 2, px, pH - BIGTICKSIZE - 2 ); //move tickmarks 2 pixels (avoids sticking out other side)
				if ( !secondaryXLimits ) p->drawLine( px, 0, px, BIGTICKSIZE );
			}

			//tick label
			if ( ShowTickLabels ) {
				double lab = xScale()*(x0 + ix*dXtick);
				if ( fabs(lab)/dXtick < 0.00001 ) lab = 0.0; //fix occassional roundoff error with "0.0" label

				switch ( xAxisType() ) {
					case DOUBLE :
					{
						QString str = QString( "%1" ).arg( lab, 0, 'g', 2 );
						int idot = str.find( '.' );
						if ( idot >= 0 ) 
							str = str.replace( idot, 1, KGlobal::locale()->decimalSymbol() );
						
						if ( px > 0 && px < pW ) {
							QRect r( px - BIGTICKSIZE, pH+BIGTICKSIZE, 2*BIGTICKSIZE, BIGTICKSIZE );
							p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
						}
						break;
					}
					case TIME :
					{
						int h = int(lab);
						int m = int(60.*(lab - h));
						while ( h > 24 ) { h -= 24; }
						while ( h <  0 ) { h += 24; }

						QString str = QString().sprintf( "%02d:%02d", h, m );
						if ( px > 0 && px < pW ) {
							QRect r( px - BIGTICKSIZE, pH+BIGTICKSIZE, 2*BIGTICKSIZE, BIGTICKSIZE );
							p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
						}
						break;
					}
					case ANGLE :
					{
						QString str = QString().sprintf( "%d%c", int(lab), 176 );
						if ( px > 0 && px < pW ) {
							QRect r( px - BIGTICKSIZE, pH+BIGTICKSIZE, 2*BIGTICKSIZE, BIGTICKSIZE );
							p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
						}
						break;
					}

					case UNKNOWN_TYPE : break;
				}
			}

			//draw minor ticks
			for ( int j=0; j < nminX; j++ ) {
				int pmin = int( px + pW*j*dminX/dataWidth() ); //position of minor tickmark j (in screen units)
				if ( pmin > 0 && pmin < pW ) {
					p->drawLine( pmin, pH-2, pmin, pH-SMALLTICKSIZE-2 );
					if ( !secondaryXLimits ) p->drawLine( pmin, 0, pmin, SMALLTICKSIZE );
				}
			}
		}

		//--- Draw primary Y tickmarks on left axis---//
		double y0 = y() - dmod( y(), dYtick ); //zeropoint; tickmark i is this plus i*dYtick1 (in data units)
		if ( y() < 0 ) y0 -= dYtick;

		for ( int iy = 0; iy <= nmajY+1; iy++ ) {
			int py = pH - int( pH * ( (y0 + iy*dYtick - y())/dataHeight() ) ); //position of tickmark i (in screen units)
			if ( py > 0 && py < pH ) {
				p->drawLine( 0, py, BIGTICKSIZE, py );
				if ( !secondaryXLimits ) p->drawLine( pW - BIGTICKSIZE - 2, py, pW - 2, py );
			}

			//tick label
			if ( ShowTickLabels ) {
				double lab = yScale()*(y0 + iy*dYtick);
				if ( fabs(lab)/dYtick < 0.00001 ) lab = 0.0; //fix occassional roundoff error with "0.0" label

				switch ( yAxisType() ) {
					case DOUBLE :
					{
						QString str = QString( "%1" ).arg( lab, 0, 'g', 2 );
						int idot = str.find( '.' );
						if ( idot >= 0 ) 
							str = str.replace( idot, 1, KGlobal::locale()->decimalSymbol() );
						
						if ( py > 0 && py < pH ) {
							QRect r( -2*BIGTICKSIZE, py-SMALLTICKSIZE, 2*BIGTICKSIZE, 2*SMALLTICKSIZE );
							p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
						}
						break;
					}
					case TIME :
					{
						int h = int(lab);
						int m = int(60.*(lab - h));
						while ( h > 24 ) { h -= 24; }
						while ( h <  0 ) { h += 24; }

						QString str = QString().sprintf( "%02d:%02d", h, m );
						if ( py > 0 && py < pH ) {
							QRect r( -3*BIGTICKSIZE, py-SMALLTICKSIZE, 2*BIGTICKSIZE, 2*SMALLTICKSIZE );
							p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
						}
						break;
					}
					case ANGLE :
					{
						QString str = QString().sprintf( "%d%c", int(lab), 176 );
						if ( py > 0 && py < pH ) {
							QRect r( -3*BIGTICKSIZE, py-SMALLTICKSIZE, 2*BIGTICKSIZE, 2*SMALLTICKSIZE );
							p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
						}
						break;
					}

					case UNKNOWN_TYPE : break;
				}
			}

			//draw minor ticks
			for ( int j=0; j < nminY; j++ ) {
				int pmin = int( py - pH*j*dminY/dataHeight() ); //position of minor tickmark j (in screen units)
				if ( pmin > 0 && pmin < pH ) {
					p->drawLine( 0, pmin, SMALLTICKSIZE, pmin );
					if ( !secondaryYLimits ) p->drawLine( pW - 2, pmin, pW-SMALLTICKSIZE-2, pmin );
				}
			}
		}

		//--- Draw secondary X tickmarks on top axis---//
		if ( secondaryXLimits ) {
			double dminX2 = dXtick2/nminX2;
			double x0 = xb() - dmod( xb(), dXtick2 ); //zeropoint; tickmark i is this plus i*dXtick2 (in data units)

			for ( int ix = 0; ix <= nmajX2; ix++ ) {
				int px = int( pW * ( (x0 + ix*dXtick2 - xb())/dataWidth2() ) ); //position of tickmark i (in screen units)
				if ( px > 0 && px < pW ) p->drawLine( px, 0, px, BIGTICKSIZE );

				//tick label
				if ( ShowTickLabels ) {
					double lab = xScale()*(x0 + ix*dXtick2);
					if ( fabs(lab)/dXtick2 < 0.00001 ) lab = 0.0; //fix occassional roundoff error with "0.0" label

					switch ( xAxisType() ) {
						case DOUBLE :
						{
							QString str = QString( "%1" ).arg( lab, 0, 'g', 2 );
							int idot = str.find( '.' );
							if ( idot >= 0 ) 
								str = str.replace( idot, 1, KGlobal::locale()->decimalSymbol() );
							
							if ( px > 0 && px < pW ) {
								QRect r( px - BIGTICKSIZE, -2*BIGTICKSIZE, 2*BIGTICKSIZE, BIGTICKSIZE );
								p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
							}
							break;
						}
						case TIME :
						{
							int h = int(lab);
							int m = int(60.*(lab - h));
							while ( h > 24 ) { h -= 24; }
							while ( h <  0 ) { h += 24; }

							QString str = QString().sprintf( "%02d:%02d", h, m );
							if ( px > 0 && px < pW ) {
								QRect r( px - BIGTICKSIZE, -2*BIGTICKSIZE, 2*BIGTICKSIZE, BIGTICKSIZE );
								p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
							}
							break;
						}
						case ANGLE :
						{
							QString str = QString().sprintf( "%d%c", int(lab), 176 );
							if ( px > 0 && px < pW ) {
								QRect r( px - BIGTICKSIZE, -2*BIGTICKSIZE, 2*BIGTICKSIZE, BIGTICKSIZE );
								p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
							}
							break;
						}

						case UNKNOWN_TYPE : break;
					}
				}

				//draw minor ticks
				for ( int j=0; j < nminX2; j++ ) {
					int pmin = int( px + pW*j*dminX2/dataWidth2() ); //position of minor tickmark j (in screen units)
					if ( pmin > 0 && pmin < pW ) p->drawLine( pmin, 0, pmin, SMALLTICKSIZE );
				}
			}
		} //end if ( secondaryXLimits )

		//--- Draw secondary Y tickmarks on right axis ---//
		if ( secondaryYLimits ) {
			double dminY2 = dYtick2/nminY2;
			double y0 = yScale()*(yb() - dmod( yb(), dYtick2 )); //zeropoint; tickmark i is this plus i*mX (in data units)

			for ( int iy = 0; iy <= nmajY2; iy++ ) {
				int py = pH - int( pH * ( (y0 + iy*dYtick2 - yb())/dataWidth2() ) ); //position of tickmark i (in screen units)
				if ( py > 0 && py < pH ) p->drawLine( pH, py, pW-BIGTICKSIZE, py );

				//tick label
				if ( ShowTickLabels ) {
					double lab = y0 + iy*dYtick2;
					if ( fabs(lab)/dYtick2 < 0.00001 ) lab = 0.0; //fix occassional roundoff error with "0.0" label

					switch ( yAxisType() ) {
						case DOUBLE :
						{
							QString str = QString( "%1" ).arg( lab, 0, 'g', 2 );
							int idot = str.find( '.' );
							if ( idot >= 0 ) 
								str = str.replace( idot, 1, KGlobal::locale()->decimalSymbol() );
							
							if ( py > 0 && py < pH ) {
								QRect r( pW + 2*BIGTICKSIZE, py-SMALLTICKSIZE, 2*BIGTICKSIZE, 2*SMALLTICKSIZE );
								p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
							}
							break;
						}
						case TIME :
						{
							int h = int(lab);
							int m = int(60.*(lab - h));
							while ( h > 24 ) { h -= 24; }
							while ( h <  0 ) { h += 24; }

							QString str = QString().sprintf( "%02d:%02d", h, m );
							if ( py > 0 && py < pH ) {
								QRect r( pW + 2*BIGTICKSIZE, py-SMALLTICKSIZE, 2*BIGTICKSIZE, 2*SMALLTICKSIZE );
								p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
							}
							break;
						}
						case ANGLE :
						{
							QString str = QString().sprintf( "%d%c", int(lab), 176 );
							if ( py > 0 && py < pH ) {
								QRect r( pW + 3*BIGTICKSIZE, py-SMALLTICKSIZE, 2*BIGTICKSIZE, 2*SMALLTICKSIZE );
								p->drawText( r, Qt::AlignCenter | Qt::DontClip, str );
							}
							break;
						}

						case UNKNOWN_TYPE : break;
					}
				}

				//minor ticks
				for ( int j=0; j < nminY2; j++ ) {
					int pmin = py - int( pH*j*dminY2/dataHeight2() ); //position of minor tickmark j (in screen units)
					if ( pmin > 0 && pmin < pH ) p->drawLine( pW, pmin, pW-SMALLTICKSIZE, pmin );
				}
			}
		} //end if ( secondaryYLimits )

		f.setPointSize( s );
		p->setFont( f );

	} //end if ( showTickmarks )

	//Draw X Axis Label(s)
	if ( ! BottomAxis.label().isEmpty() ) {
		QRect r( 0, PixRect.height() + 2*YPADDING, PixRect.width(), YPADDING );
		p->drawText( r, Qt::AlignCenter | Qt::DontClip, BottomAxis.label() );
	}
	if ( ! XAxisLabel2.isEmpty() ) {
		QRect r( 0, -3*YPADDING, PixRect.width(), YPADDING );
		p->drawText( r, Qt::AlignCenter | Qt::DontClip, XAxisLabel2 );
	}

	//Draw Y Axis Label(s).  We need to draw the text sideways.
	if ( ! LeftAxis.label().isEmpty() ) {
		//store current painter translation/rotation state
		p->save();

		//translate coord sys to left corner of axis label rectangle, then rotate 90 degrees.
		p->translate( -3*XPADDING, PixRect.height() );
		p->rotate( -90.0 );

		QRect r( 0, 0, PixRect.height(), XPADDING );
		p->drawText( r, Qt::AlignCenter | Qt::DontClip, LeftAxis.label() ); //draw the label, now that we are sideways

		p->restore();  //restore translation/rotation state
	}
	if ( ! YAxisLabel2.isEmpty() ) {
		//store current painter translation/rotation state
		p->save();

		//translate coord sys to left corner of axis label rectangle, then rotate 90 degrees.
		p->translate( PixRect.width() + 2*XPADDING, PixRect.height() );
		p->rotate( -90.0 );

		QRect r( 0, 0, PixRect.height(), XPADDING );
		p->drawText( r, Qt::AlignCenter | Qt::DontClip, YAxisLabel2 ); //draw the label, now that we are sideways

		p->restore();  //restore translation/rotation state
	}
}

int KStarsPlotWidget::rightPadding() const {
	if ( RightPadding >= 0 ) return RightPadding;

	bool secondaryYLimits( false );
	if ( dataHeight2() > 0.0 && ( yb() != y() || yb2() != y2() ) ) secondaryYLimits = true;
	if ( secondaryYLimits && ( ShowTickLabels && ! XAxisLabel2 ) ) return 3*XPADDING;
	if ( secondaryYLimits && ( ShowTickLabels || ! XAxisLabel2 ) ) return 2*XPADDING;
	return XPADDING;
}

int KStarsPlotWidget::topPadding() const {
	if ( TopPadding >= 0 ) return TopPadding;

	bool secondaryXLimits( false );
	if ( dataWidth2()  > 0.0 && ( xb() != x() || xb2() != x2() ) ) secondaryXLimits = true;
	if ( secondaryXLimits && ( ShowTickLabels && ! YAxisLabel2 ) ) return 3*YPADDING;
	if ( secondaryXLimits && ( ShowTickLabels || ! YAxisLabel2 ) ) return 2*YPADDING;
	return YPADDING;
}

#include "kstarsplotwidget.moc"
