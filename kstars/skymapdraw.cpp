/***************************************************************************
                          skymapdraw.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Mar 2 2003
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

//This file contains drawing functions SkyMap class.

#include <stdlib.h> // abs
#include <math.h> //log10()
#include <iostream>

#include <qpaintdevicemetrics.h>
#include <qpainter.h>

#include "skymap.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skyobject.h"
#include "deepskyobject.h"
#include "starobject.h"
#include "ksplanetbase.h"
#include "ksasteroid.h"
#include "kscomet.h"
#include "ksmoon.h"
#include "jupitermoons.h"
#include "infoboxes.h"
#include "simclock.h"
#include "csegment.h"
#include "customcatalog.h"
#include "devicemanager.h"
#include "indimenu.h"
#include "indiproperty.h"
#include "indielement.h"
#include "indidevice.h"

void SkyMap::drawOverlays( QPixmap *pm ) {
	if ( ksw ) { //only if we are drawing in the GUI window
		QPainter p;
		p.begin( pm );

		showFocusCoords( true );
		drawBoxes( p );

		//draw FOV symbol
		ksw->data()->fovSymbol.draw( p, (float)(Options::fOVSize() * Options::zoomFactor()/57.3/60.0) );
		drawTelescopeSymbols( p );
		drawObservingList( p );
		drawZoomBox( p );
		if ( transientObject() ) drawTransientLabel( p );
		if (isAngleMode()) {
			updateAngleRuler();
			drawAngleRuler( p );
		}
	}
}

void SkyMap::drawAngleRuler( QPainter &p ) {
	p.setPen( QPen( data->colorScheme()->colorNamed( "AngularRuler" ), 1, DotLine ) );
	p.drawLine( beginRulerPoint, endRulerPoint );
}

void SkyMap::drawZoomBox( QPainter &p ) {
	//draw the manual zoom-box, if it exists
	if ( ZoomRect.isValid() ) {
		p.setPen( QPen( "white", 1, DotLine ) );
		p.drawRect( ZoomRect.x(), ZoomRect.y(), ZoomRect.width(), ZoomRect.height() );
	}
}

void SkyMap::drawTransientLabel( QPainter &p ) {
	if ( transientObject() ) {
		p.setPen( TransientColor );

		if ( checkVisibility( transientObject(), fov(), XRange ) ) {
			QPoint o = getXY( transientObject(), Options::useAltAz(), Options::useRefraction(), 1.0 );
			if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
				drawNameLabel( p, transientObject(), o.x(), o.y(), 1.0 );
			}
		}
	}
}

void SkyMap::drawBoxes( QPainter &p ) {
	if ( ksw ) { //only if we are drawing in the GUI window
		ksw->infoBoxes()->drawBoxes( p,
				data->colorScheme()->colorNamed( "BoxTextColor" ),
				data->colorScheme()->colorNamed( "BoxGrabColor" ),
				data->colorScheme()->colorNamed( "BoxBGColor" ), Options::boxBGMode() );
	}
}

void SkyMap::drawObservingList( QPainter &psky, double scale ) {
	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "ObsListColor" ) ), 1 ) );

	if ( ksw && ksw->observingList()->count() ) {
		for ( SkyObject* obj = ksw->observingList()->first(); obj; obj = ksw->observingList()->next() ) {
			
			if ( checkVisibility( obj, fov(), XRange ) ) {
				QPoint o = getXY( obj, Options::useAltAz(), Options::useRefraction() );

				// label object if it is currently on screen
				if (o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <= height() ) {
					if ( Options::obsListSymbol() ) {
						int size = int(20*scale);
						int x1 = o.x() - size/2;
						int y1 = o.y() - size/2;
						psky.drawArc( x1, y1, size, size, -60*16, 120*16 );
						psky.drawArc( x1, y1, size, size, 120*16, 120*16 );
					}
					if ( Options::obsListText() ) {
						drawNameLabel( psky, obj, o.x(), o.y(), scale );
					}
				}
			}
		}
	}
}

void SkyMap::drawTelescopeSymbols(QPainter &psky) {

	if ( ksw ) { //ksw doesn't exist in non-GUI mode!
		INDI_P *eqNum;
		INDI_P *portConnect;
		INDI_E *lp;
		INDIMenu *devMenu = ksw->getINDIMenu();
		bool useJ2000 (false), useAltAz(false);
		SkyPoint indi_sp;

		if (!Options::indiCrosshairs() || devMenu == NULL)
			return;

		psky.setPen( QPen( QColor( data->colorScheme()->colorNamed("TargetColor" ) ) ) );
		psky.setBrush( NoBrush );
		int pxperdegree = int(Options::zoomFactor()/57.3);

		//fprintf(stderr, "in draw telescope function with mgrsize of %d\n", devMenu->mgr.size());
		for (uint i=0; i < devMenu->mgr.count(); i++)
		{
			for (uint j=0; j < devMenu->mgr.at(i)->indi_dev.count(); j++)
			{
				useAltAz = false;
				useJ2000 = false;

				// make sure the dev is on first
				if (devMenu->mgr.at(i)->indi_dev.at(j)->isOn())
				{
				        portConnect = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("CONNECTION");

					if (!portConnect)
					 return;

					 if (portConnect->state == PS_BUSY)
					  return;

					eqNum = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("EQUATORIAL_EOD_COORD");
					
					if (eqNum == NULL)
					{
						eqNum = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("EQUATORIAL_COORD");
						if (eqNum == NULL)
                                                {
						  eqNum = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("HORIZONTAL_COORD");
                                                  if (eqNum == NULL) continue;
                                                  else
							useAltAz = true;
						}
                                                else
						 useJ2000 = true;
					}

					// make sure it has RA and DEC properties
					if ( eqNum)
					{
						//fprintf(stderr, "Looking for RA label\n");
                                                if (useAltAz)
						{
						lp = eqNum->findElement("AZ");
						if (!lp)
							continue;

						dms azDMS(lp->value);
						
						lp = eqNum->findElement("ALT");
						if (!lp)
							continue;

						dms altDMS(lp->value);

						indi_sp.setAz(azDMS);
						indi_sp.setAlt(altDMS);

						}
						else
						{

						lp = eqNum->findElement("RA");
						if (!lp)
							continue;

						// express hours in degrees on the celestial sphere
						dms raDMS(lp->value);
						raDMS.setD ( raDMS.Degrees() * 15.0);

						lp = eqNum->findElement("DEC");
						if (!lp)
							continue;

						dms decDMS(lp->value);


						//kdDebug() << "the KStars RA is " << raDMS.toHMSString() << endl;
						//kdDebug() << "the KStars DEC is " << decDMS.toDMSString() << "\n****************" << endl;

						indi_sp.setRA(raDMS);
						indi_sp.setDec(decDMS);

						if (useJ2000)
						{
							indi_sp.setRA0(raDMS);
							indi_sp.setDec0(decDMS);
							indi_sp.apparentCoord( (double) J2000, ksw->data()->ut().djd());
						}
							
						if ( Options::useAltAz() ) indi_sp.EquatorialToHorizontal( ksw->LST(), ksw->geo()->lat() );

						}

						QPoint P = getXY( &indi_sp, Options::useAltAz(), Options::useRefraction() );

						int s1 = pxperdegree/2;
						int s2 = pxperdegree;
						int s3 = 2*pxperdegree;

						int x0 = P.x();  int y0 = P.y();
						int x1 = x0 - s1/2;  int y1 = y0 - s1/2;
						int x2 = x0 - s2/2;  int y2 = y0 - s2/2;
						int x3 = x0 - s3/2;  int y3 = y0 - s3/2;

						//Draw radial lines
						psky.drawLine( x1, y0, x3, y0 );
						psky.drawLine( x0+s2, y0, x0+s1/2, y0 );
						psky.drawLine( x0, y1, x0, y3 );
						psky.drawLine( x0, y0+s1/2, x0, y0+s2 );
						//Draw circles at 0.5 & 1 degrees
						psky.drawEllipse( x1, y1, s1, s1 );
						psky.drawEllipse( x2, y2, s2, s2 );

						psky.drawText( x0+s2 + 2 , y0, QString(devMenu->mgr.at(i)->indi_dev.at(j)->label) );

					}
				}
			}
		}
	}

}

void SkyMap::drawMilkyWay( QPainter& psky, double scale )
{
	int ptsCount = 0;
	int mwmax = int( scale * Options::zoomFactor()/100.);
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	int thick(1);
	if ( ! Options::fillMilkyWay() ) thick=3;

	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "MWColor" ) ), thick, SolidLine ) );
	psky.setBrush( QBrush( QColor( data->colorScheme()->colorNamed( "MWColor" ) ) ) );
	bool offscreen, lastoffscreen=false;

	for ( register unsigned int j=0; j<11; ++j ) {
		if ( Options::fillMilkyWay() ) {
			ptsCount = 0;
			bool partVisible = false;

			QPoint o = getXY( data->MilkyWay[j].at(0), Options::useAltAz(), Options::useRefraction(), scale );
			if ( o.x() != -10000000 && o.y() != -10000000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
			if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) partVisible = true;

			for ( SkyPoint *p = data->MilkyWay[j].first(); p; p = data->MilkyWay[j].next() ) {
				o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
				if ( o.x() != -10000000 && o.y() != -10000000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
				if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) partVisible = true;
			}

			if ( ptsCount && partVisible ) {
				psky.drawPolygon( (  const QPointArray ) *pts, false, 0, ptsCount );
			}
		} else {
			QPoint o = getXY( data->MilkyWay[j].at(0), Options::useAltAz(), Options::useRefraction(), scale );
			if (o.x()==-10000000 && o.y()==-10000000) offscreen = true;
			else offscreen = false;

			psky.moveTo( o.x(), o.y() );

			for ( register unsigned int i=1; i<data->MilkyWay[j].count(); ++i ) {
				o = getXY( data->MilkyWay[j].at(i), Options::useAltAz(), Options::useRefraction(), scale );
				if (o.x()==-10000000 && o.y()==-10000000) offscreen = true;
				else offscreen = false;

				//don't draw a line if the last point's getXY was (-10000000, -10000000)
				int dx = abs(o.x()-psky.pos().x());
				int dy = abs(o.y()-psky.pos().y());
				if ( (!lastoffscreen && !offscreen) && (dx<mwmax && dy<mwmax) ) {
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
				}
				lastoffscreen = offscreen;
			}
		}
	}
}

void SkyMap::drawCoordinateGrid( QPainter& psky, double scale )
{
	QPoint cur;

	//Draw coordinate grid
	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "GridColor" ) ), 1, DotLine ) ); //change to GridColor

	//First, the parallels
	for ( register double Dec=-80.; Dec<=80.; Dec += 20. ) {
		bool newlyVisible = false;
		sp->set( 0.0, Dec );
		if ( Options::useAltAz() ) sp->EquatorialToHorizontal( data->LST, data->geo()->lat() );
		QPoint o = getXY( sp, Options::useAltAz(), Options::useRefraction(), scale );
		QPoint o1 = o;
		cur = o;
		psky.moveTo( o.x(), o.y() );

		double dRA = 1./5.; //120 points along full circle of RA
		for ( register double RA=dRA; RA<24.; RA+=dRA ) {
			sp->set( RA, Dec );
			if ( Options::useAltAz() ) sp->EquatorialToHorizontal( data->LST, data->geo()->lat() );

			if ( checkVisibility( sp, guideFOV, guideXRange ) ) {
				o = getXY( sp, Options::useAltAz(), Options::useRefraction(), scale );

				//When drawing on the printer, the psky.pos() point does NOT get updated
				//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
				int dx = cur.x() - o.x();
				int dy = cur.y() - o.y();
				cur = o;

				if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						psky.moveTo( o.x(), o.y() );
					} else {
						psky.lineTo( o.x(), o.y() );
					}
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			}
	}

		//connect the final segment
		int dx = psky.pos().x() - o1.x();
		int dy = psky.pos().y() - o1.y();
		if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
			psky.lineTo( o1.x(), o1.y() );
		} else {
			psky.moveTo( o1.x(), o1.y() );
		}
	}

	//next, the meridians
	for ( register double RA=0.; RA<24.; RA += 2. ) {
		bool newlyVisible = false;
		SkyPoint *sp1 = new SkyPoint( RA, -90. );
		if ( Options::useAltAz() ) sp1->EquatorialToHorizontal( data->LST, data->geo()->lat() );
		QPoint o = getXY( sp1, Options::useAltAz(), Options::useRefraction(), scale );
		cur = o;
		psky.moveTo( o.x(), o.y() );

		double dDec = 1.;
		for ( register double Dec=-89.; Dec<=90.; Dec+=dDec ) {
			sp1->set( RA, Dec );
			if ( Options::useAltAz() ) sp1->EquatorialToHorizontal( data->LST, data->geo()->lat() );

			if ( checkVisibility( sp1, guideFOV, guideXRange ) ) {
				o = getXY( sp1, Options::useAltAz(), Options::useRefraction(), scale );

				//When drawing on the printer, the psky.pos() point does NOT get updated
				//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
				int dx = cur.x() - o.x();
				int dy = cur.y() - o.y();
				cur = o;

				if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						psky.moveTo( o.x(), o.y() );
					} else {
						psky.lineTo( o.x(), o.y() );
					}
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			}
		}
		delete sp1;  // avoid memory leak
	}
}

void SkyMap::drawEquator( QPainter& psky, double scale )
{
	//Draw Equator (currently can't be hidden on slew)
	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "EqColor" ) ), 1, SolidLine ) );

	SkyPoint *p = data->Equator.first();
	QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
	QPoint o1 = o;
	QPoint last = o;
	QPoint cur = o;
	QPoint o2;
	psky.moveTo( o.x(), o.y() );
	bool newlyVisible = false;

	//index of point near the left or top/bottom edge
	uint index1(0), index2(0);
	int xSmall(width() + 100); //ridiculous initial value

	//start loop at second item
	for ( p = data->Equator.next(); p; p = data->Equator.next() ) {
		if ( checkVisibility( p, guideFOV, guideXRange ) ) {
			o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

			//first iteration for positioning the "Equator" label:
			//flag the onscreen equator point with the smallest positive x value
			//we don't draw the label while slewing
			if ( ! slewing && o.x() > 0 && o.x() < width() && o.y() > 0 && o.y() < height() ) {
				if ( o.x() < xSmall ) {
					xSmall = o.x();
					index1 = data->Equator.at();
				}
			}

			//When drawing on the printer, the psky.pos() point does NOT get updated
			//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
			int dx = cur.x() - o.x();
			int dy = cur.y() - o.y();
			cur = o;

			if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						psky.moveTo( last.x(), last.y() );
					}
					psky.lineTo( o.x(), o.y() );
			} else {
				psky.moveTo( o.x(), o.y() );
			}
		} else {
			newlyVisible = true;
		}
		last = o;
	}

	//connect the final segment
	int dx = psky.pos().x() - o1.x();
	int dy = psky.pos().y() - o1.y();
	if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
		psky.lineTo( o1.x(), o1.y() );
	} else {
		psky.moveTo( o1.x(), o1.y() );
	}

	if ( ! slewing && xSmall < width() ) {
		//Draw the "Equator" label.  We have flagged the leftmost onscreen Equator point.
		//If the zoom level is below 1000, simply adopt this point as the anchor for the
		//label.  If the zoom level is 1000 or higher, we interpolate to find the exact
		//point at which the Equator goes offscreen, and anchor from that point.
		p = data->Equator.at(index1);
		double ra0(0.0);  //the RA of our anchor point (the Dec is known to be 0.0
											//since it's the Equator)

		if ( Options::zoomFactor() < 1000. ) {
			ra0 = p->ra()->Hours();

		} else {
			//Somewhere between Equator point p and its immediate neighbor, the Equator goes
			//offscreen.  Determine the exact point at which this happens.
			index2 = index1 + 1;
			if ( index2 >= data->Equator.count() ) index2 -= data->Equator.count();
			SkyPoint *p2 = data->Equator.at(index2);

			o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
			o2 = getXY( p2, Options::useAltAz(), Options::useRefraction(), scale );

			double x1, x2;
			//there are 3 possibilities:  (o2.x() < 0); (o2.y() < 0); (o2.y() > height())
			if ( o2.x() < 0 ) {
				x1 = double(o.x())/double(o.x()-o2.x());
				x2 = -1.0*double(o2.x())/double(o.x()-o2.x());
			} else if ( o2.y() < 0 ) {
				x1 = double(o.y())/double(o.y()-o2.y());
				x2 = -1.0*double(o2.y())/double(o.y()-o2.y());
			} else if ( o2.y() > height() ) {
				x1 = double(height() - o.y())/double(o2.y()-o.y());
				x2 = double(o2.y() - height())/double(o2.y()-o.y());
			} else {  //should never get here
				x1 = 0.0;
				x2 = 1.0;
			}

			//ra0 is the exact RA at which the Equator intersects a screen edge
			ra0 = x1*p2->ra()->Hours() + x2*p->ra()->Hours();
		}

		//LabelPoint is the top left corner of the text label.  It is
		//offset from the anchor point by -1.5 degree (0.1 hour) in RA
		//and -0.4 degree in Dec, scaled by 2000./zoomFactor so that they are
		//independent of zoom.
		SkyPoint LabelPoint( ra0 - 200./Options::zoomFactor(), -800./Options::zoomFactor() );
		if ( Options::useAltAz() )
			LabelPoint.EquatorialToHorizontal( data->LST, data->geo()->lat() );

		//p2 is a SkyPoint offset from LabelPoint in RA by -0.1 hour/zoomFactor.
		//We use this point to determine the rotation angle for the text (which
		//we want to be parallel to the line joining LabelPoint and p2)
		SkyPoint p2 = LabelPoint;
		p2.setRA( p2.ra()->Hours() - 200./Options::zoomFactor() );
		if ( Options::useAltAz() )
			p2.EquatorialToHorizontal( data->LST, data->geo()->lat() );

		//o and o2 are the screen coordinates of LabelPoint and p2.
		o = getXY( &LabelPoint, Options::useAltAz(), Options::useRefraction(), scale );
		o2 = getXY( &p2, Options::useAltAz(), Options::useRefraction() );

		double sx = double( o.x() - o2.x() );
		double sy = double( o.y() - o2.y() );
		double angle;
		if ( sx ) {
			angle = atan( sy/sx )*180.0/dms::PI;
		} else {
			angle = 90.0;
			if ( sy < 0 ) angle = -90.0;
		}

		//Finally, draw the "Equator" label at the determined location and angle
		psky.save();
		psky.translate( o.x(), o.y() );
		psky.rotate( double( angle ) );  //rotate the coordinate system
		psky.drawText( 0, 0, i18n( "Equator" ) );
		psky.restore(); //reset coordinate system
	}
}

void SkyMap::drawEcliptic( QPainter& psky, double scale )
{
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	//Draw Ecliptic (currently can't be hidden on slew)
	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "EclColor" ) ), 1, SolidLine ) );

	SkyPoint *p = data->Ecliptic.first();
	QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
	QPoint o2 = o;
	QPoint o1 = o;
	QPoint last = o;
	QPoint cur = o;
	psky.moveTo( o.x(), o.y() );

	//index of point near the right or top/bottom edge
	uint index1(0), index2(0);
	int xBig(-100); //ridiculous initial value

	bool newlyVisible = false;
	//Start loop at second item
	for ( p = data->Ecliptic.next(); p; p = data->Ecliptic.next() ) {
		if ( checkVisibility( p, guideFOV, guideXRange ) ) {
			o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

			//first iteration for positioning the "Ecliptic" label:
			//flag the onscreen equator point with the largest x value
			//we don't draw the label while slewing
			if ( ! slewing && o.x() > 0 && o.x() < width() && o.y() > 0 && o.y() < height() ) {
				if ( o.x() > xBig ) {
					xBig = o.x();
					index1 = data->Ecliptic.at();
				}
			}

			//When drawing on the printer, the psky.pos() point does NOT get updated
			//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
			int dx = cur.x() - o.x();
			int dy = cur.y() - o.y();
			cur = o;

			if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
				if ( newlyVisible ) {
					newlyVisible = false;
					psky.moveTo( last.x(), last.y() );
				}
				psky.lineTo( o.x(), o.y() );
			} else {
				psky.moveTo( o.x(), o.y() );
			}
		} else {
			newlyVisible = true;
		}
		last = o;
	}

	//connect the final segment
	int dx = psky.pos().x() - o1.x();
	int dy = psky.pos().y() - o1.y();
	if ( abs(dx) < Width && abs(dy) < Height ) {
		psky.lineTo( o1.x(), o1.y() );
	} else {
		psky.moveTo( o1.x(), o1.y() );
	}

	if ( ! slewing && xBig > 0 ) {
		//Draw the "Ecliptic" label.  We have flagged the rightmost onscreen Ecliptic point.
		//If the zoom level is below 1000, simply adopt this point as the anchor for the
		//label.  If the zoom level is 1000 or higher, we interpolate to find the exact
		//point at which the Ecliptic goes offscreen, and anchor from that point.
		p = data->Ecliptic.at(index1);
		double ra0(0.0);  //the ra of our anchor point
		double dec0(0.0); //the dec of our anchor point

		if ( Options::zoomFactor() < 1000. ) {
			ra0 = p->ra()->Hours();
			dec0 = p->dec()->Degrees();
		} else {
			//Somewhere between Ecliptic point p and its immediate neighbor, the Ecliptic goes
			//offscreen.  Determine the exact point at which this happens.
			if ( index1 == 0 ) index2 = 0;
			else index2 = index1 - 1;
			SkyPoint *p2 = data->Ecliptic.at(index2);

			o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
			o2 = getXY( p2, Options::useAltAz(), Options::useRefraction(), scale );

			double x1, x2;
			//there are 3 possibilities:  (o2.x() > width()); (o2.y() < 0); (o2.y() > height())
			if ( o2.x() > width() ) {
				x1 = double(width()-o.x())/double(o2.x()-o.x());
				x2 = double(o2.x()-width())/double(o2.x()-o.x());
			} else if ( o2.y() < 0 ) {
				x1 = double(o.y())/double(o.y()-o2.y());
				x2 = -1.0*double(o2.y())/double(o.y()-o2.y());
			} else if ( o2.y() > height() ) {
				x1 = double(height() - o.y())/double(o2.y()-o.y());
				x2 = double(o2.y() - height())/double(o2.y()-o.y());
			} else {  //should never get here
				x1 = 0.0;
				x2 = 1.0;
			}

			//ra0 is the exact RA at which the Ecliptic intersects a screen edge
			ra0 = x1*p2->ra()->Hours() + x2*p->ra()->Hours();
			//dec0 is the exact Dec at which the Ecliptic intersects a screen edge
			dec0 = x1*p2->dec()->Degrees() + x2*p->dec()->Degrees();
		}

		KSNumbers num( data->ut().djd() );
		dms ecLong, ecLat;

		//LabelPoint is offset from the anchor point by +2.0 degree ecl. Long
		//and -0.4 degree in ecl. Lat, scaled by 2000./zoomFactor so that they are
		//independent of zoom.
		SkyPoint LabelPoint(ra0, dec0);
		LabelPoint.findEcliptic( num.obliquity(), ecLong, ecLat );
		ecLong.setD( ecLong.Degrees() + 4000./Options::zoomFactor() );
		ecLat.setD( ecLat.Degrees() - 800./Options::zoomFactor() );
		LabelPoint.setFromEcliptic( num.obliquity(), &ecLong, &ecLat );
		if ( Options::useAltAz() )
			LabelPoint.EquatorialToHorizontal( data->LST, data->geo()->lat() );

		//p2 is a SkyPoint offset from LabelPoint by -1.0 degrees of ecliptic longitude.
		//we use p2 to determine the onscreen rotation angle for the ecliptic label,
		//which we want to be parallel to the line between LabelPoint and p2.
		SkyPoint p2(ra0, dec0);
		p2.findEcliptic( num.obliquity(), ecLong, ecLat );
		ecLong.setD( ecLong.Degrees() + 2000./Options::zoomFactor() );
		ecLat.setD( ecLat.Degrees() - 800./Options::zoomFactor() );
		p2.setFromEcliptic( num.obliquity(), &ecLong, &ecLat );
		if ( Options::useAltAz() )
			p2.EquatorialToHorizontal( data->LST, data->geo()->lat() );

		//o and o2 are the screen positions of LabelPoint and p2.
		o = getXY( &LabelPoint, Options::useAltAz(), Options::useRefraction(), scale );
		o2 = getXY( &p2, Options::useAltAz(), Options::useRefraction() );

		double sx = double( o.x() - o2.x() );
		double sy = double( o.y() - o2.y() );
		double angle;
		if ( sx ) {
			angle = atan( sy/sx )*180.0/dms::PI;
		} else {
			angle = 90.0;
			if ( sy < 0 ) angle = -90.0;
		}

		//Finally, draw the "Ecliptic" label at the determined location and angle
		psky.save();
		psky.translate( o.x(), o.y() );
		psky.rotate( double( angle ) );  //rotate the coordinate system
		psky.drawText( 0, 0, i18n( "Ecliptic" ) );
		psky.restore(); //reset coordinate system
	}
}

void SkyMap::drawHorizon( QPainter& psky, double scale )
{
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	QPtrList<QPoint> points;
	points.setAutoDelete(true);
	QPoint o, o2;

	//Draw Horizon
	//The horizon should not be corrected for atmospheric refraction, so getXY has doRefract=false...
	if (Options::showHorizon() || Options::showGround() ) {
		QPoint OutLeft(0,0), OutRight(0,0);

		psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "HorzColor" ) ), 1, SolidLine ) );
		psky.setBrush( QColor ( data->colorScheme()->colorNamed( "HorzColor" ) ) );
		int ptsCount = 0;
		int maxdist = int(Options::zoomFactor()/4);

		//index of point near the right or top/bottom edge
		uint index1(0), index2(0);
		int xBig(-100); //ridiculous initial value

		for ( SkyPoint *p = data->Horizon.first(); p; p = data->Horizon.next() ) {
			o = getXY( p, Options::useAltAz(), false, scale );  //false: do not refract the horizon
			bool found = false;

			//first iteration for positioning the "Horizon" label:
			//flag the onscreen equator point with the largest x value
			//we don't draw the label while slewing, or if the opaque ground is drawn
			if ( ! slewing && ( ! Options::showGround() || ! Options::useAltAz() )
						&& o.x() > 0 && o.x() < width() && o.y() > 0 && o.y() < height() ) {
				if ( o.x() > xBig ) {
					xBig = o.x();
					index1 = data->Horizon.at();
				}
			}

			//Use the QPtrList of points to pre-sort visible horizon points
			if ( o.x() > -100 && o.x() < Width + 100 && o.y() > -100 && o.y() < Height + 100 ) {
				if ( Options::useAltAz() ) {
					register unsigned int j;
					for ( j=0; j<points.count(); ++j ) {
						if ( o.x() < points.at(j)->x() ) {
							found = true;
							break;
						}
					}
					if ( found ) {
						points.insert( j, new QPoint(o) );
					} else {
						points.append( new QPoint(o) );
					}
				} else {
					points.append( new QPoint(o) );
				}
			} else {  //find the out-of-bounds points closest to the left and right borders
				if ( ( OutLeft.x() == 0 || o.x() > OutLeft.x() ) && o.x() < -100 ) {
					OutLeft.setX( o.x() );
					OutLeft.setY( o.y() );
				}
				if ( ( OutRight.x() == 0 || o.x() < OutRight.x() ) && o.x() >  + 100 ) {
					OutRight.setX( o.x() );
					OutRight.setY( o.y() );
				}
			}
		}

		//Add left-edge and right-edge points based on interpolating the first/last onscreen points
		//to the nearest offscreen points.

		if ( Options::useAltAz() && points.count() > 0 ) {
			//If the edge of the visible sky circle is onscreen, then we should 
			//interpolate the first and last horizon points to the edge of the circle.
			//Otherwise, interpolate to the screen edge.
			double dx = 0.5*double(Width)/Options::zoomFactor(); //center-to-edge ang in radians
			double r0 = 2.0*sin(0.25*dms::PI);
			
			if ( dx < r0 ) { //edge of visible sky circle is not visible
				//Interpolate from first sorted onscreen point to x=-100,
				//using OutLeft to determine the slope
				int xtotal = ( points.at( 0 )->x() - OutLeft.x() );
				int xx = ( points.at( 0 )->x() + 100 ) / xtotal;
				int yp = xx*OutRight.y() + (1-xx)*points.at( 0 )->y();  //interpolated left-edge y value
				QPoint *LeftEdge = new QPoint( -100, yp );
				points.insert( 0, LeftEdge ); //Prepend LeftEdge to the beginning of points
	
				//Interpolate from the last sorted onscreen point to ()+100,
				//using OutRight to determine the slope.
				xtotal = ( OutRight.x() - points.at( points.count() - 1 )->x() );
				xx = ( Width + 100 - points.at( points.count() - 1 )->x() ) / xtotal;
				yp = xx*OutRight.y() + (1-xx)*points.at( points.count() - 1 )->y(); //interpolated right-edge y value
				QPoint *RightEdge = new QPoint( Width+100, yp );
				points.append( RightEdge );
			}
		//If there are no horizon points, then either the horizon doesn't pass through the screen
		//or we're at high zoom, and horizon points lie on either side of the screen.
		} else if ( Options::useAltAz() && OutLeft.y() !=0 && OutRight.y() !=0 &&
						!( OutLeft.y() > Height + 100 && OutRight.y() > Height + 100 ) &&
						!( OutLeft.y() < -100 && OutRight.y() < -100 ) ) {

			//It's possible at high zoom that /no/ horizon points are onscreen.  In this case,
			//interpolate between OutLeft and OutRight directly to construct the horizon polygon.
			int xtotal = ( OutRight.x() - OutLeft.x() );
			int xx = ( OutRight.x() + 100 ) / xtotal;
			int yp = xx*OutLeft.y() + (1-xx)*OutRight.y();  //interpolated left-edge y value
//			QPoint *LeftEdge = new QPoint( -100, yp );
			points.append( new QPoint( -100, yp ) );

			xx = ( Width + 100 - OutLeft.x() ) / xtotal;
			yp = xx*OutRight.y() + (1-xx)*OutLeft.y(); //interpolated right-edge y value
//			QPoint *RightEdge = new QPoint( Width+100, yp );
			points.append( new QPoint( Width+100, yp ) );
 		}

		if ( points.count() ) {
//		Fill the pts array with sorted horizon points, Draw Horizon Line
			pts->setPoint( 0, points.at(0)->x(), points.at(0)->y() );
			if ( Options::showHorizon() ) psky.moveTo( points.at(0)->x(), points.at(0)->y() );

			for ( register unsigned int i=1; i<points.count(); ++i ) {
				pts->setPoint( i, points.at(i)->x(), points.at(i)->y() );

				if ( Options::showHorizon() ) {
					if ( !Options::useAltAz() && ( abs( points.at(i)->x() - psky.pos().x() ) > maxdist ||
								abs( points.at(i)->y() - psky.pos().y() ) > maxdist ) ) {
						psky.moveTo( points.at(i)->x(), points.at(i)->y() );
					} else {
						psky.lineTo( points.at(i)->x(), points.at(i)->y() );
					}

				}
			}

			//connect the last segment back to the beginning
			if ( abs( points.at(0)->x() - psky.pos().x() ) < maxdist && abs( points.at(0)->y() - psky.pos().y() ) < maxdist )
				psky.lineTo( points.at(0)->x(), points.at(0)->y() );

			//Finish the Ground polygon.  If sky edge is visible, the 
			//bottom edge follows the sky circle.  Otherwise, we just 
			//add a square bottom edge, offscreen
			if ( Options::useAltAz() ) {
				if ( Options::showGround() ) {
					ptsCount = points.count();
					
					//center-to-edge ang in radians
					double dx = 0.5*double(Width)/Options::zoomFactor(); 
					double r0 = 2.0*sin(0.25*dms::PI);
					
//					//Second QPointsArray for blocking the region outside the sky circle
//					QPointArray bpts( 100 ); //need 90 points along sky circle, plus 4 to complete polygon
//					uint bpCount(0);
					
					if ( dx > r0 ) { //sky edge is visible
						for ( double t=360.; t >= 180.; t-=2. ) {  //add points along edge of circle
							dms a( t );
							double sa(0.), ca(0.);
							a.SinCos( sa, ca );
							int xx = Width/2 + int(r0*Options::zoomFactor()*ca);
							int yy = Height/2 - int(r0*Options::zoomFactor()*sa);
							
							pts->setPoint( ptsCount++, xx, yy );
//							bpts.setPoint( bpCount++, xx, yy );
						}
						
//						//complete the background polygon, then draw it with SkyColor
//						bpts.setPoint( bpCount++,        -100, Height/2     );
//						bpts.setPoint( bpCount++,        -100, Height + 100 );
//						bpts.setPoint( bpCount++, Width + 100, Height + 100 );
//						bpts.setPoint( bpCount++, Width + 100, Height/2     );
//						psky.setPen( QColor ( data->colorScheme()->colorNamed( "SkyColor" ) ) );
//						psky.setBrush( QColor ( data->colorScheme()->colorNamed( "SkyColor" ) ) );
//						psky.drawPolygon( bpts, false, 0, bpCount );
//						//Reset colors for Horizon polygon
//						psky.setPen( QColor ( data->colorScheme()->colorNamed( "HorzColor" ) ) );
//						psky.setBrush( QColor ( data->colorScheme()->colorNamed( "HorzColor" ) ) );
					
					} else {
						pts->setPoint( ptsCount++, Width+100, Height+100 );   //bottom right corner
						pts->setPoint( ptsCount++, -100, Height+100 );         //bottom left corner
					}
						
					//Draw the Horizon polygon
					psky.drawPolygon( ( const QPointArray ) *pts, false, 0, ptsCount );

					//remove all items in points list
					for ( register unsigned int i=0; i<points.count(); ++i ) {
						points.remove(i);
					}
				}

//	Draw compass heading labels along horizon
				SkyPoint *c = new SkyPoint;
				QPoint cpoint;

				if ( Options::showGround() )
					psky.setPen( QColor ( data->colorScheme()->colorNamed( "CompassColor" ) ) );
				else
					psky.setPen( QColor ( data->colorScheme()->colorNamed( "HorzColor" ) ) );

		//North
				c->setAz( 359.99 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "North", "N" ) );
				}

		//NorthEast
				c->setAz( 45.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northeast", "NE" ) );
				}

		//East
				c->setAz( 90.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "East", "E" ) );
				}

		//SouthEast
				c->setAz( 135.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southeast", "SE" ) );
				}

		//South
				c->setAz( 179.99 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "South", "S" ) );
				}

		//SouthWest
				c->setAz( 225.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southwest", "SW" ) );
				}

		//West
				c->setAz( 270.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "West", "W" ) );
				}

		//NorthWest
				c->setAz( 315.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				cpoint = getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northwest", "NW" ) );
				}

				delete c;
			}
		}

		if ( ! slewing && (! Options::showGround() || ! Options::useAltAz() ) && xBig > 0 ) {
			//Draw the "Horizon" label.  We have flagged the rightmost onscreen Horizon point.
			//If the zoom level is below 1000, simply adopt this point as the anchor for the
			//label.  If the zoom level is 1000 or higher, we interpolate to find the exact
			//point at which the Horizon goes offscreen, and anchor from that point.
			SkyPoint *p = data->Horizon.at(index1);
			double ra0(0.0);  //the ra of our anchor point
			double dec0(0.0); //the dec of our anchor point

			if ( Options::zoomFactor() < 1000. ) {
				ra0 = p->ra()->Hours();
				dec0 = p->dec()->Degrees();
			} else {
				//Somewhere between Horizon point p and its immediate neighbor, the Horizon goes
				//offscreen.  Determine the exact point at which this happens.
				index2 = index1 + 1;
				if ( data->Horizon.count() &&  index2 > data->Horizon.count() - 1 ) index2 -= data->Horizon.count();
				SkyPoint *p2 = data->Horizon.at(index2);

				QPoint o1 = getXY( p, Options::useAltAz(), false, scale );
				QPoint o2 = getXY( p2, Options::useAltAz(), false, scale );

				double x1, x2;
				//there are 3 possibilities:  (o2.x() > width()); (o2.y() < 0); (o2.y() > height())
				if ( o2.x() > width() ) {
					x1 = double(width()-o1.x())/double(o2.x()-o1.x());
					x2 = double(o2.x()-width())/double(o2.x()-o1.x());
				} else if ( o2.y() < 0 ) {
					x1 = double(o1.y())/double(o1.y()-o2.y());
					x2 = -1.0*double(o2.y())/double(o1.y()-o2.y());
				} else if ( o2.y() > height() ) {
					x1 = double(height() - o1.y())/double(o2.y()-o1.y());
					x2 = double(o2.y() - height())/double(o2.y()-o1.y());
				} else {  //should never get here
					x1 = 0.0;
					x2 = 1.0;
				}

				//ra0 is the exact RA at which the Horizon intersects a screen edge
				ra0 = x1*p2->ra()->Hours() + x2*p->ra()->Hours();
				//dec0 is the exact Dec at which the Horizon intersects a screen edge
				dec0 = x1*p2->dec()->Degrees() + x2*p->dec()->Degrees();
			}

			//LabelPoint is offset from the anchor point by -2.0 degrees in azimuth
			//and -0.4 degree altitude, scaled by 2000./zoomFactor so that they are
			//independent of zoom.
			SkyPoint LabelPoint(ra0, dec0);
			LabelPoint.EquatorialToHorizontal( data->LST, data->geo()->lat() );
			LabelPoint.setAlt( LabelPoint.alt()->Degrees() - 800./Options::zoomFactor() );
			LabelPoint.setAz( LabelPoint.az()->Degrees() - 4000./Options::zoomFactor() );
			LabelPoint.HorizontalToEquatorial( data->LST, data->geo()->lat() );
			o = getXY( &LabelPoint, Options::useAltAz(), false, scale );
			if ( o.x() > width() || o.x() < 0 ) {
				//the LabelPoint is offscreen.  Either we are in the Southern hemisphere,
				//or the sky is rotated upside-down.  Use an azimuth offset of +2.0 degrees
			LabelPoint.setAlt( LabelPoint.alt()->Degrees() + 1600./Options::zoomFactor() );
				LabelPoint.setAz( LabelPoint.az()->Degrees() + 8000./Options::zoomFactor() );
				LabelPoint.HorizontalToEquatorial( data->LST, data->geo()->lat() );
				o = getXY( &LabelPoint, Options::useAltAz(), false, scale );
			}

			//p2 is a skypoint offset from LabelPoint by +/-1 degree azimuth (scaled by
			//2000./zoomFactor).  We use p2 to determine the rotation angle for the
			//Horizon label, which we want to be parallel to the line between LabelPoint and p2.
			SkyPoint p2( LabelPoint.ra(), LabelPoint.dec() );
			p2.EquatorialToHorizontal( data->LST, data->geo()->lat() );
			p2.setAz( p2.az()->Degrees() + 2000./Options::zoomFactor() );
			p2.HorizontalToEquatorial( data->LST, data->geo()->lat() );

			//o and o2 are the screen positions of LabelPoint and p2
			o = getXY( &LabelPoint, Options::useAltAz(), false, scale );
			o2 = getXY( &p2, Options::useAltAz(), false, scale );

			double sx = double( o.x() - o2.x() );
			double sy = double( o.y() - o2.y() );
			double angle;
			if ( sx ) {
				angle = atan( sy/sx )*180.0/dms::PI;
			} else {
				angle = 90.0;
				if ( sy < 0 ) angle = -90.0;
			}

			//Finally, draw the "Equator" label at the determined location and angle
			psky.save();
			psky.translate( o.x(), o.y() );
			psky.rotate( double( angle ) );  //rotate the coordinate system
			psky.drawText( 0, 0, i18n( "Horizon" ) );
			psky.restore(); //reset coordinate system
		}
	}  //endif drawing horizon
}

void SkyMap::drawConstellationLines( QPainter& psky, double scale )
{
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	//Draw Constellation Lines
	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "CLineColor" ) ), 1, SolidLine ) ); //change to colorGrid
	int iLast = -1;

	for ( SkyPoint *p = data->clineList.first(); p; p = data->clineList.next() ) {
		QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

		if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
			if ( data->clineModeList.at(data->clineList.at())->latin1()=='M' ) {
				psky.moveTo( o.x(), o.y() );
			} else if ( data->clineModeList.at(data->clineList.at())->latin1()=='D' ) {
				if ( data->clineList.at()== (int)(iLast+1) ) {
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			}
			iLast = data->clineList.at();
		}
  }
}

void SkyMap::drawConstellationBoundaries( QPainter &psky, double scale ) {
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "CBoundColor" ) ), 1, SolidLine ) );

	for ( CSegment *seg = data->csegmentList.first(); seg; seg = data->csegmentList.next() ) {
		bool started( false );
		SkyPoint *p = seg->firstNode();
		QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
		if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
			psky.moveTo( o.x(), o.y() );
			started = true;
		}

		for ( p = seg->nextNode(); p; p = seg->nextNode() ) {
			QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

			if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
				if ( started ) {
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
					started = true;
				}
			} else {
				started = false;
			}
		}
	}
}

void SkyMap::drawConstellationNames( QPainter& psky, double scale ) {
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	//Draw Constellation Names
	psky.setPen( QColor( data->colorScheme()->colorNamed( "CNameColor" ) ) );
	for ( SkyObject *p = data->cnameList.first(); p; p = data->cnameList.next() ) {
		if ( checkVisibility( p, fov(), XRange ) ) {
			QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
			if (o.x() >= 0 && o.x() <= Width && o.y() >=0 && o.y() <= Height ) {
				if ( Options::useLatinConstellNames() ) {
					int dx = 5*p->name().length();
					psky.drawText( o.x()-dx, o.y(), p->name() );  // latin constellation names
				} else if ( Options::useLocalConstellNames() ) {
					// can't use translatedName() because we need the context string in i18n()
					int dx = 5*( i18n( "Constellation name (optional)", p->name().local8Bit().data() ).length() );
					psky.drawText( o.x()-dx, o.y(), i18n( "Constellation name (optional)", p->name().local8Bit().data() ) ); // localized constellation names
				} else {
					int dx = 5*p->name2().length();
					psky.drawText( o.x()-dx, o.y(), p->name2() ); //name2 is the IAU abbreviation
				}
			}
		}
  }
}

void SkyMap::drawStars( QPainter& psky, double scale ) {
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	bool checkSlewing = ( ( slewing || ( clockSlewing && data->clock()->isActive() ) )
				&& Options::hideOnSlew() );

//shortcuts to inform wheter to draw different objects
	bool hideFaintStars( checkSlewing && Options::hideStars() );

	if ( Options::showStars() ) {
		//adjust maglimit for ZoomLevel
		double lgmin = log10(MINZOOM);
		double lgmax = log10(MAXZOOM);
		double lgz = log10(Options::zoomFactor());

		double maglim = Options::magLimitDrawStar();
		if ( lgz <= 0.75*lgmax ) maglim -= (Options::magLimitDrawStar() - Options::magLimitDrawStarZoomOut() )*(0.75*lgmax - lgz)/(0.75*lgmax - lgmin);
		float sizeFactor = 6.0 + (lgz - lgmin);

		for ( StarObject *curStar = data->starList.first(); curStar; curStar = data->starList.next() ) {
			// break loop if maglim is reached
			if ( curStar->mag() > maglim || ( hideFaintStars && curStar->mag() > Options::magLimitHideStar() ) ) break;

			if ( checkVisibility( curStar, fov(), XRange ) ) {
				QPoint o = getXY( curStar, Options::useAltAz(), Options::useRefraction(), scale );

				// draw star if currently on screen
				if (o.x() >= 0 && o.x() <= Width && o.y() >=0 && o.y() <= Height ) {
					int size = int( scale * ( sizeFactor*( maglim - curStar->mag())/maglim ) + 1 );

					if ( size > 0 ) {
						QChar c = curStar->color();
						QPixmap *spixmap = starpix->getPixmap( &c, size );
						curStar->draw( psky, sky, spixmap, o.x(), o.y(), true, scale );

						// now that we have drawn the star, we can display some extra info
						//don't label unnamed stars with the generic "star" name
						bool drawName = ( Options::showStarNames() && (curStar->name() != i18n("star") ) );
						if ( !checkSlewing && (curStar->mag() <= Options::magLimitDrawStarInfo() )
								&& ( drawName || Options::showStarMagnitudes() ) ) {

							psky.setPen( QColor( data->colorScheme()->colorNamed( "SNameColor" ) ) );
							QFont stdFont( psky.font() );
							QFont smallFont( stdFont );
							smallFont.setPointSize( stdFont.pointSize() - 2 );
							if ( Options::zoomFactor() < 10.*MINZOOM ) {
								psky.setFont( smallFont );
							} else {
								psky.setFont( stdFont );
							}

							curStar->drawLabel( psky, o.x(), o.y(), Options::zoomFactor(),
									drawName, Options::showStarMagnitudes(), scale );

							//reset font
							psky.setFont( stdFont );
						}
					}
				}
			}
		}
	}
}

void SkyMap::drawDeepSkyCatalog( QPainter& psky, QPtrList<DeepSkyObject>& catalog, QColor& color,
			bool drawObject, bool drawImage, double scale )
{
	if ( drawObject || drawImage ) {  //don't do anything if nothing is to be drawn!
		int Width = int( scale * width() );
		int Height = int( scale * height() );

		// Set color once
		psky.setPen( color );
		psky.setBrush( NoBrush );
		QColor colorHST  = data->colorScheme()->colorNamed( "HSTColor" );

		double maglim = Options::magLimitDrawDeepSky();

		//FIXME
		//disabling faint limits until the NGC/IC catalog has reasonable mags
		//adjust maglimit for ZoomLevel
		//double lgmin = log10(MINZOOM);
		//double lgmax = log10(MAXZOOM);
		//double lgz = log10(Options::zoomFactor());
		//if ( lgz <= 0.75*lgmax ) maglim -= (Options::magLimitDrawDeepSky() - Options::magLimitDrawDeepSkyZoomOut() )*(0.75*lgmax - lgz)/(0.75*lgmax - lgmin);
		//else
			maglim = 40.0; //show all deep-sky objects

		for ( DeepSkyObject *obj = catalog.first(); obj; obj = catalog.next() ) {
			if ( checkVisibility( obj, fov(), XRange ) ) {
				float mag = obj->mag();
				//only draw objects if flags set and its brighter than maglim (unless mag is undefined (=99.9)
				if ( mag > 90.0 || mag < (float)maglim ) {
					QPoint o = getXY( obj, Options::useAltAz(), Options::useRefraction(), scale );
					if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
						double PositionAngle = findPA( obj, o.x(), o.y(), scale );

						//Draw Image
						if ( drawImage && Options::zoomFactor() > 5.*MINZOOM ) {
							obj->drawImage( psky, o.x(), o.y(), PositionAngle, Options::zoomFactor(), scale );
						}

						//Draw Symbol
						if ( drawObject ) {
							//change color if extra images are available
							// most objects don't have those, so we only change colors temporarily
							// for the few exceptions. Changing color is expensive!!!
							bool bColorChanged = false;
							if ( obj->isCatalogM() && obj->ImageList.count() > 1 ) {
								psky.setPen( colorHST );
								bColorChanged = true;
							} else if ( (!obj->isCatalogM()) && obj->ImageList.count() ) {
								psky.setPen( colorHST );
								bColorChanged = true;
							}

							obj->drawSymbol( psky, o.x(), o.y(), PositionAngle, Options::zoomFactor(), scale );

							// revert temporary color change
							if ( bColorChanged ) {
								psky.setPen( color );
							}
						}
					}
				}
			} else { //Object failed checkVisible(); delete it's Image pointer, if it exists.
				if ( obj->image() ) {
					obj->deleteImage();
				}
			}
		}
	}
}

void SkyMap::drawDeepSkyObjects( QPainter& psky, double scale )
{
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	QImage ScaledImage;

	bool checkSlewing = ( ( slewing || ( clockSlewing && data->clock()->isActive() ) )
				&& Options::hideOnSlew() );

//shortcuts to inform wheter to draw different objects
	bool drawMess( Options::showDeepSky() && ( Options::showMessier() || Options::showMessierImages() ) && !(checkSlewing && Options::hideMessier() ) );
	bool drawNGC( Options::showDeepSky() && Options::showNGC() && !(checkSlewing && Options::hideNGC() ) );
	bool drawOther( Options::showDeepSky() && Options::showOther() && !(checkSlewing && Options::hideOther() ) );
	bool drawIC( Options::showDeepSky() && Options::showIC() && !(checkSlewing && Options::hideIC() ) );
	bool drawImages( Options::showMessierImages() );

	// calculate color objects once, outside the loop
	QColor colorMess = data->colorScheme()->colorNamed( "MessColor" );
	QColor colorIC  = data->colorScheme()->colorNamed( "ICColor" );
	QColor colorNGC  = data->colorScheme()->colorNamed( "NGCColor" );

	// draw Messier catalog
	if ( drawMess ) {
		drawDeepSkyCatalog( psky, data->deepSkyListMessier, colorMess, Options::showMessier(), drawImages, scale );
	}

	// draw NGC Catalog
	if ( drawNGC ) {
		drawDeepSkyCatalog( psky, data->deepSkyListNGC, colorNGC, true, drawImages, scale );
	}

	// draw IC catalog
	if ( drawIC ) {
		drawDeepSkyCatalog( psky, data->deepSkyListIC, colorIC, true, drawImages, scale );
	}

	// draw the rest
	if ( drawOther ) {
		//Use NGC color for now...
		drawDeepSkyCatalog( psky, data->deepSkyListOther, colorNGC, true, drawImages, scale );
	}

	//Draw Custom Catalogs
	for ( register unsigned int i=0; i<data->CustomCatalogs.count(); ++i ) { 
		if ( Options::showCatalog()[i] ) {
			QPtrList<SkyObject> cat = data->CustomCatalogs.at(i)->objList();

			for ( SkyObject *obj = cat.first(); obj; obj = cat.next() ) {

				if ( checkVisibility( obj, fov(), XRange ) ) {
					QPoint o = getXY( obj, Options::useAltAz(), Options::useRefraction(), scale );

					if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {

						if ( obj->type()==0 ) {
							StarObject *starobj = (StarObject*)obj;
							float zoomlim = 7.0 + ( Options::zoomFactor()/MINZOOM)/50.0;
							float mag = starobj->mag();
							float sizeFactor = 2.0;
							int size = int( sizeFactor*(zoomlim - mag) ) + 1;
							if (size>23) size=23;
							if ( size > 0 ) {
								QChar c = starobj->color();
								QPixmap *spixmap = starpix->getPixmap( &c, size );

								// Try to paint with the selected color
								spixmap->fill(QColor( data->CustomCatalogs.at(i)->color() ));
								starobj->draw( psky, sky, spixmap, o.x(), o.y(), true, scale );
								// After drawing the star we display some extra info like
								// the name ...
								bool drawName = ( Options::showStarNames() && (starobj->name() != i18n("star") ) );
								if ( !checkSlewing && (starobj->mag() <= Options::magLimitDrawStarInfo() )
										&& ( drawName || Options::showStarMagnitudes() ) ) {

											psky.setPen( QColor( data->colorScheme()->colorNamed( "SNameColor" ) ) );
											QFont stdFont( psky.font() );
											QFont smallFont( stdFont );
											smallFont.setPointSize( stdFont.pointSize() - 2 );
											if ( Options::zoomFactor() < 10.*MINZOOM ) {
												psky.setFont( smallFont );
											} else {
												psky.setFont( stdFont );
											}

											starobj->drawLabel( psky, o.x(), o.y(), Options::zoomFactor(), drawName, Options::showStarMagnitudes(), scale );

											//reset font
											psky.setFont( stdFont );
								}
							}
						} else {
							DeepSkyObject *dso = (DeepSkyObject*)obj;
							double pa = findPA( dso, o.x(), o.y(), scale );
							dso->drawImage( psky, o.x(), o.y(), pa, Options::zoomFactor() );

							psky.setBrush( NoBrush );
							psky.setPen( QColor( data->CustomCatalogs.at(i)->color() ) );

							dso->drawSymbol( psky, o.x(), o.y(), pa, Options::zoomFactor() );
						}
					}
				}
			}
		}
	}
}

void SkyMap::drawAttachedLabels( QPainter &psky, double scale ) {
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	psky.setPen( data->colorScheme()->colorNamed( "UserLabelColor" ) );

	bool checkSlewing = ( slewing || ( clockSlewing && data->clock()->isActive() ) ) && Options::hideOnSlew();
	bool drawPlanets( Options::showPlanets() && !(checkSlewing && Options::hidePlanets() ) );
	bool drawComets( drawPlanets && Options::showComets() );
	bool drawAsteroids( drawPlanets && Options::showAsteroids() );
	bool drawMessier( Options::showDeepSky() && ( Options::showMessier() || Options::showMessierImages() ) && !(checkSlewing && Options::hideMessier() ) );
	bool drawNGC( Options::showDeepSky() && Options::showNGC() && !(checkSlewing && Options::hideNGC() ) );
	bool drawIC( Options::showDeepSky() && Options::showIC() && !(checkSlewing && Options::hideIC() ) );
	bool drawOther( Options::showDeepSky() && Options::showOther() && !(checkSlewing && Options::hideOther() ) );
	bool drawSAO = ( Options::showStars() );
	bool hideFaintStars( checkSlewing && Options::hideStars() );

	for ( SkyObject *obj = data->ObjLabelList.first(); obj; obj = data->ObjLabelList.next() ) {
		//Only draw an attached label if the object is being drawn to the map
		//reproducing logic from other draw funcs here...not an optimal solution
		if ( obj->type() == SkyObject::STAR ) {
			if ( ! drawSAO ) return;
			if ( obj->mag() > Options::magLimitDrawStar() ) return;
			if ( hideFaintStars && obj->mag() > Options::magLimitHideStar() ) return;
		}
		if ( obj->type() == SkyObject::PLANET ) {
			if ( ! drawPlanets ) return;
			if ( obj->name() == "Sun" && ! Options::showSun() ) return;
			if ( obj->name() == "Mercury" && ! Options::showMercury() ) return;
			if ( obj->name() == "Venus" && ! Options::showVenus() ) return;
			if ( obj->name() == "Moon" && ! Options::showMoon() ) return;
			if ( obj->name() == "Mars" && ! Options::showMars() ) return;
			if ( obj->name() == "Jupiter" && ! Options::showJupiter() ) return;
			if ( obj->name() == "Saturn" && ! Options::showSaturn() ) return;
			if ( obj->name() == "Uranus" && ! Options::showUranus() ) return;
			if ( obj->name() == "Neptune" && ! Options::showNeptune() ) return;
			if ( obj->name() == "Pluto" && ! Options::showPluto() ) return;
		}
		if ( obj->type() >= SkyObject::OPEN_CLUSTER && obj->type() <= SkyObject::GALAXY ) {
			if ( ((DeepSkyObject*)obj)->isCatalogM() && ! drawMessier ) return;
			if ( ((DeepSkyObject*)obj)->isCatalogNGC() && ! drawNGC ) return;
			if ( ((DeepSkyObject*)obj)->isCatalogIC() && ! drawIC ) return;
			if ( ((DeepSkyObject*)obj)->isCatalogNone() && ! drawOther ) return;
		}
		if ( obj->type() == SkyObject::COMET && ! drawComets ) return;
		if ( obj->type() == SkyObject::ASTEROID && ! drawAsteroids ) return;

		if ( checkVisibility( obj, fov(), XRange ) ) {
			QPoint o = getXY( obj, Options::useAltAz(), Options::useRefraction(), scale );
			if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
				drawNameLabel( psky, obj, o.x(), o.y(), scale );
			}
		}
	}

	//Attach a label to the centered object
	if ( focusObject() != NULL && Options::useAutoLabel() ) {
		QPoint o = getXY( focusObject(), Options::useAltAz(), Options::useRefraction(), scale );
		if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height )
			drawNameLabel( psky, focusObject(), o.x(), o.y(), scale );
	}
}

void SkyMap::drawNameLabel( QPainter &psky, SkyObject *obj, int x, int y, double scale ) {
	int size(0);

	QFont stdFont( psky.font() );
	QFont smallFont( stdFont );
	smallFont.setPointSize( stdFont.pointSize() - 2 );
	if ( Options::zoomFactor() < 10.*MINZOOM ) {
		psky.setFont( smallFont );
	} else {
		psky.setFont( stdFont );
	}

	//Stars
	if ( obj->type() == SkyObject::STAR ) {
		((StarObject*)obj)->drawLabel( psky, x, y, Options::zoomFactor(), true, false, scale );
		psky.setFont( stdFont );
		return;

	//Solar system
	} else if ( obj->type() == SkyObject::PLANET
						|| obj->type() == SkyObject::ASTEROID
						|| obj->type() == SkyObject::COMET ) {
		KSPlanetBase *p = (KSPlanetBase*)obj;
		size = int( p->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0 );
		int minsize = 4;
		if ( p->type() == SkyObject::ASTEROID || p->type() == SkyObject::COMET )
			minsize = 2;
		if ( p->name() == "Sun" || p->name() == "Moon" )
			minsize = 8;
		if ( size < minsize )
			size = minsize;
		if ( p->name() == "Saturn" )
			size = int(2.5*size);

	//Other
	} else {
		//Calculate object size in pixels
		float majorAxis = ((DeepSkyObject*)obj)->a();
		if ( majorAxis == 0.0 && obj->type() == 1 ) majorAxis = 1.0; //catalog stars
		size = int( majorAxis * scale * dms::PI * Options::zoomFactor()/10800.0 );
	}

	int offset = int( ( 0.5*size + 4 ) );
	psky.drawText( x+offset, y+offset, obj->translatedName() );

	//Reset font
	psky.setFont( stdFont );
}

void SkyMap::drawPlanetTrail( QPainter& psky, KSPlanetBase *ksp, double scale )
{
	if ( ksp->hasTrail() ) {
		int Width = int( scale * width() );
		int Height = int( scale * height() );

		QColor tcolor1 = QColor( data->colorScheme()->colorNamed( "PlanetTrailColor" ) );
		QColor tcolor2 = QColor( data->colorScheme()->colorNamed( "SkyColor" ) );

		SkyPoint *p = ksp->trail()->first();
		QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
		QPoint cur( o );

		bool doDrawLine(false);
		int i = 0;
		int n = ksp->trail()->count();

		if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
			psky.moveTo(o.x(), o.y());
			doDrawLine = true;
		}

		psky.setPen( QPen( tcolor1, 1 ) );
		for ( p = ksp->trail()->next(); p; p = ksp->trail()->next() ) {
			if ( Options::fadePlanetTrails() ) {
				//Define interpolated color
				QColor tcolor = QColor(
							(i*tcolor1.red()   + (n-i)*tcolor2.red())/n,
							(i*tcolor1.green() + (n-i)*tcolor2.green())/n,
							(i*tcolor1.blue()  + (n-i)*tcolor2.blue())/n );
				++i;
				psky.setPen( QPen( tcolor, 1 ) );
			}

			o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
			if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {

				//Want to disable line-drawing if this point and the last are both outside bounds of display.
				if ( ! rect().contains( o ) && ! rect().contains( cur ) ) doDrawLine = false;
				cur = o;

				if ( doDrawLine ) {
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
					doDrawLine = true;
				}
			}
		}
	}
}

void SkyMap::drawSolarSystem( QPainter& psky, bool drawPlanets, double scale )
{
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	if ( drawPlanets ) {
		//Draw all trails first so they never appear "in front of" solar system bodies
		//draw Trail
		if ( Options::showSun() && data->PCat->findByName("Sun")->hasTrail() ) drawPlanetTrail( psky, data->PCat->findByName("Sun"), scale );
		if ( Options::showMercury() && data->PCat->findByName("Mercury")->hasTrail() ) drawPlanetTrail( psky, data->PCat->findByName("Mercury"), scale );
		if ( Options::showVenus() && data->PCat->findByName("Venus")->hasTrail() ) drawPlanetTrail( psky, data->PCat->findByName("Venus"), scale );
		if ( Options::showMoon() && data->Moon->hasTrail() ) drawPlanetTrail( psky, data->Moon, scale );
		if ( Options::showMars() && data->PCat->findByName("Mars")->hasTrail() ) drawPlanetTrail( psky, data->PCat->findByName("Mars"), scale );
		if ( Options::showJupiter() && data->PCat->findByName("Jupiter")->hasTrail() ) drawPlanetTrail( psky, data->PCat->findByName("Jupiter"), scale );
		if ( Options::showSaturn() && data->PCat->findByName("Saturn")->hasTrail() ) drawPlanetTrail( psky, data->PCat->findByName("Saturn"), scale );
		if ( Options::showUranus() && data->PCat->findByName("Uranus")->hasTrail() ) drawPlanetTrail( psky, data->PCat->findByName("Uranus"), scale );
		if ( Options::showNeptune() && data->PCat->findByName("Neptune")->hasTrail() ) drawPlanetTrail( psky, data->PCat->findByName("Neptune"), scale );
		if ( Options::showPluto() && data->PCat->findByName("Pluto")->hasTrail() ) drawPlanetTrail( psky, data->PCat->findByName("Pluto"), scale );
		if ( Options::showAsteroids() ) {
			for ( KSAsteroid *ast = data->asteroidList.first(); ast; ast = data->asteroidList.next() ) {
				if ( ast->mag() > Options::magLimitAsteroid() ) break;
				if ( ast->hasTrail() ) drawPlanetTrail( psky, ast, scale );
			}
		}
		if ( Options::showComets() ) {
			for ( KSComet *com = data->cometList.first(); com; com = data->cometList.next() ) {
				if ( com->hasTrail() ) drawPlanetTrail( psky, com, scale );
			}
		}

		//Now draw the actual solar system bodies.  Draw furthest to closest.
		//Draw Asteroids
		if ( Options::showAsteroids() ) {
			for ( KSAsteroid *ast = data->asteroidList.first(); ast; ast = data->asteroidList.next() ) {
				if ( ast->mag() > Options::magLimitAsteroid() ) break;

				if ( checkVisibility( ast, fov(), XRange ) ) {
					psky.setPen( QPen( QColor( "gray" ) ) );
					psky.setBrush( QBrush( QColor( "gray" ) ) );
					QPoint o = getXY( ast, Options::useAltAz(), Options::useRefraction(), scale );

					if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) ) {
						int size = int( ast->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0 );
						if ( size < 1 ) size = 1;
						int x1 = o.x() - size/2;
						int y1 = o.y() - size/2;
						psky.drawEllipse( x1, y1, size, size );

						//draw Name
						if ( Options::showAsteroidNames() && ast->mag() < Options::magLimitAsteroidName() ) {
							psky.setPen( QColor( data->colorScheme()->colorNamed( "PNameColor" ) ) );
							drawNameLabel( psky, ast, o.x(), o.y(), scale );
						}
					}
				}
			}
		}

		//Draw Comets
		if ( Options::showComets() ) {
			for ( KSComet *com = data->cometList.first(); com; com = data->cometList.next() ) {
				if ( checkVisibility( com, fov(), XRange ) ) {
					psky.setPen( QPen( QColor( "cyan4" ) ) );
					psky.setBrush( QBrush( QColor( "cyan4" ) ) );
					QPoint o = getXY( com, Options::useAltAz(), Options::useRefraction(), scale );

					if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) ) {
						int size = int( com->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0 );
						if ( size < 1 ) size = 1;
						int x1 = o.x() - size/2;
						int y1 = o.y() - size/2;
						psky.drawEllipse( x1, y1, size, size );

						//draw Name
						if ( Options::showCometNames() && com->rsun() < Options::maxRadCometName() ) {
							psky.setPen( QColor( data->colorScheme()->colorNamed( "PNameColor" ) ) );
							drawNameLabel( psky, com, o.x(), o.y(), scale );
						}
					}
				}
			}
		}

		//Draw Pluto
		if ( Options::showPluto() ) {
			drawPlanet(psky, data->PCat->findByName("Pluto"), QColor( "gray" ), 50.*MINZOOM, 1, scale );
		}

		//Draw Neptune
		if ( Options::showNeptune() ) {
			drawPlanet(psky, data->PCat->findByName("Neptune"), QColor( "SkyBlue" ), 20.*MINZOOM, 1, scale );
		}

		//Draw Uranus
		if ( Options::showUranus() ) {
			drawPlanet(psky, data->PCat->findByName("Uranus"), QColor( "LightSeaGreen" ), 20.*MINZOOM, 1, scale );
		}

		//Draw Saturn
		if ( Options::showSaturn() ) {
			drawPlanet(psky, data->PCat->findByName("Saturn"), QColor( "LightYellow2" ), 20.*MINZOOM, 2, scale );
		}

		//Draw Jupiter and its moons.
		//Draw all moons first, then Jupiter, then redraw moons that are in front of Jupiter.
		if ( Options::showJupiter() ) {
			//Draw Jovian moons
			psky.setPen( QPen( QColor( "white" ) ) );
			if ( Options::zoomFactor() > 10.*MINZOOM ) {
				for ( unsigned int i=0; i<4; ++i ) {
					QPoint o = getXY( data->jmoons->pos(i), Options::useAltAz(), Options::useRefraction(), scale );
					if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) ) {
						psky.drawEllipse( o.x()-1, o.y()-1, 2, 2 );
					}
				}
			}

			drawPlanet(psky, data->PCat->findByName("Jupiter"), QColor( "Goldenrod" ), 20.*MINZOOM, 1, scale );

			//Re-draw Jovian moons which are in front of Jupiter, also draw all 4 moon labels.
			psky.setPen( QPen( QColor( "white" ) ) );
			if ( Options::zoomFactor() > 10.*MINZOOM ) {
				QFont pfont = psky.font();
				QFont moonFont = psky.font();
				moonFont.setPointSize( pfont.pointSize() - 2 );
				psky.setFont( moonFont );

				for ( unsigned int i=0; i<4; ++i ) {
					QPoint o = getXY( data->jmoons->pos(i), Options::useAltAz(), Options::useRefraction(), scale );
					if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) ) {
						if ( data->jmoons->z(i) < 0.0 ) //Moon is nearer than Jupiter
							psky.drawEllipse( o.x()-1, o.y()-1, 2, 2 );

						//Draw Moon name labels if at high zoom
						if ( Options::showPlanetNames() && Options::zoomFactor() > 50.*MINZOOM ) {
							int offset = int(3*scale);
							psky.drawText( o.x() + offset, o.y() + offset, data->jmoons->name(i) );
						}
					}
				}

				//reset font
				psky.setFont( pfont );
			}
		}

		//Draw Mars
		if ( Options::showMars() ) {
			drawPlanet(psky, data->PCat->findByName("Mars"), QColor( "Red" ), 20.*MINZOOM, 1, scale );
		}

		//For the inner planets, we need to determine the distance-order
		//because the order can change with time
		double rv = data->PCat->findByName("Venus")->rearth();
		double rm = data->PCat->findByName("Mercury")->rearth();
		double rs = data->PCat->findByName("Sun")->rearth();
		unsigned int iv(0), im(0), is(0);
		if ( rm > rs ) im++;
		if ( rm > rv ) im++;
		if ( rv > rs ) iv++;
		if ( rv > rm ) iv++;
		if ( rs > rm ) is++;
		if ( rs > rv ) is++;

		for ( unsigned int i=0; i<3; i++ ) {
			if ( i==is ) {
				//Draw Sun
				if ( Options::showSun() ) {
					drawPlanet(psky, data->PCat->findByName("Sun"), QColor( "Yellow" ), MINZOOM, 1, scale );
				}
			} else if ( i==im ) {
				//Draw Mercury
				if ( Options::showMercury() ) {
					drawPlanet(psky, data->PCat->findByName("Mercury"), QColor( "SlateBlue1" ), 20.*MINZOOM, 1, scale );
				}
			} else if ( i==iv ) {
				//Draw Venus
				if ( Options::showVenus() ) {
					drawPlanet(psky, data->PCat->findByName("Venus"), QColor( "LightGreen" ), 20.*MINZOOM, 1, scale );
				}
			}
		}

		//Draw Moon
		if ( Options::showMoon() ) {
			drawPlanet(psky, data->Moon, QColor( "White" ), MINZOOM, 1, scale );
		}
	}
}

void SkyMap::drawPlanet( QPainter &psky, KSPlanetBase *p, QColor c,
		double zoommin, int resize_mult, double scale ) {

	if ( checkVisibility( p, fov(), XRange ) ) {
		int Width = int( scale * width() );
		int Height = int( scale * height() );

		int sizemin = 4;
		if ( p->name() == "Sun" || p->name() == "Moon" ) sizemin = 8;
		sizemin = int( sizemin * scale );

		psky.setPen( c );
		psky.setBrush( c );
		QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

		//Is planet onscreen?
		if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
			int size = int( p->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0 );
			if ( size < sizemin ) size = sizemin;

			//Draw planet image if:
			if ( Options::showPlanetImages() &&  //user wants them,
					int(Options::zoomFactor()) >= int(zoommin) &&  //zoomed in enough,
					!p->image()->isNull() &&  //image loaded ok,
					size < Width ) {  //and size isn't too big.

				//Image size must be modified to account for possibility that rotated image's
				//size is bigger than original image size.  The rotated image is a square
				//superscribed on the original image.  The superscribed square is larger than
				//the original square by a factor of (cos(t) + sin(t)) where t is the angle by
				//which the two squares are rotated (in our case, equal to the position angle +
				//the north angle, reduced between 0 and 90 degrees).
				//The proof is left as an exercise to the student :)
				dms pa( findPA( p, o.x(), o.y(), scale ) );
				double spa, cpa;
				pa.SinCos( spa, cpa );
				cpa = fabs(cpa);
				spa = fabs(spa);
				size = int( size * (cpa + spa) );

				//Because Saturn has rings, we inflate its image size by a factor 2.5
				if ( p->name() == "Saturn" ) size = int(2.5*size);

				if (resize_mult != 1) {
					size *= resize_mult;
				}

				p->scaleRotateImage( size, pa.Degrees() );
				int x1 = o.x() - p->image()->width()/2;
				int y1 = o.y() - p->image()->height()/2;
				psky.drawImage( x1, y1, *(p->image()));

			} else {                                   //Otherwise, draw a simple circle.

				psky.drawEllipse( o.x()-size/2, o.y()-size/2, size, size );
			}

			//draw Name
			if ( Options::showPlanetNames() ) {
				psky.setPen( QColor( data->colorScheme()->colorNamed( "PNameColor" ) ) );
				drawNameLabel( psky, p, o.x(), o.y(), scale );
			}
		}
	}
}

void SkyMap::exportSkyImage( const QPaintDevice *pd ) {
	QPainter p;

	//shortcuts to inform wheter to draw different objects
	bool drawPlanets( Options::showPlanets() );
	bool drawMW( Options::showMilkyWay() );
	bool drawCNames( Options::showCNames() );
	bool drawCLines( Options::showCLines() );
	bool drawCBounds( Options::showCBounds() );
	bool drawGrid( Options::showGrid() );

	p.begin( pd );
	QPaintDeviceMetrics pdm( p.device() );

	//scale image such that it fills 90% of the x or y dimension on the paint device
	double xscale = double(pdm.width()) / double(width());
	double yscale = double(pdm.height()) / double(height());
	double scale = (xscale < yscale) ? xscale : yscale;

	int pdWidth = int( scale * width() );
	int pdHeight = int( scale * height() );
	int x1 = int( 0.5*(pdm.width()  - pdWidth) );
	int y1 = int( 0.5*(pdm.height()  - pdHeight) );

	p.setClipRect( QRect( x1, y1, pdWidth, pdHeight ) );
	p.setClipping( true );

	//Fill background with sky color
	p.fillRect( x1, y1, pdWidth, pdHeight, QBrush( data->colorScheme()->colorNamed( "SkyColor" ) ) );

	if ( x1 || y1 ) p.translate( x1, y1 );

	if ( drawMW ) drawMilkyWay( p, scale );
	if ( drawGrid ) drawCoordinateGrid( p, scale );

	if ( drawCBounds ) drawConstellationBoundaries( p, scale );
	if ( drawCLines ) drawConstellationLines( p, scale );
	if ( drawCNames ) drawConstellationNames( p, scale );

	if ( Options::showEquator() ) drawEquator( p, scale );
	if ( Options::showEcliptic() ) drawEcliptic( p, scale );

	drawStars( p, scale );
	drawDeepSkyObjects( p, scale );
	drawSolarSystem( p, drawPlanets, scale );
	drawAttachedLabels( p, scale );
	drawObservingList( p, scale );
	drawHorizon( p, scale );

	p.end();
}

void SkyMap::setMapGeometry() {
	guidemax = int(Options::zoomFactor()/10.0);

	isPoleVisible = false;
	if ( Options::useAltAz() ) {
		XRange = 1.2*fov()/cos( focus()->alt()->radians() );
		Ymax = fabs( focus()->alt()->Degrees() ) + fov();
	} else {
		XRange = 1.2*fov()/cos( focus()->dec()->radians() );
		Ymax = fabs( focus()->dec()->Degrees() ) + fov();
	}
	if ( Ymax >= 90. ) isPoleVisible = true;

	//at high zoom, double FOV for guide lines so they don't disappear.
	guideFOV = fov();
	guideXRange = XRange;
	if ( Options::zoomFactor() > 10.*MINZOOM ) { guideFOV *= 2.0; guideXRange *= 2.0; }
}
