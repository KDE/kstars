/***************************************************************************
                          horizoncomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "horizoncomponent.h"

#include <QList>
#include <QPoint>
#include <QPainter>
//Added by qt3to4:
#include <Q3PointArray>

#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"

#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon

HorizonComponent::HorizonComponent(SkyComposite *parent) : SkyComponent(parent)
{
//	Horizon.setAutoDelete(TRUE);
	pts = 0;
}

HorizonComponent::~HorizonComponent()
{
	delete pts;
}

// was KStarsData::initGuides(KSNumbers *num)
// needs dms *LST, *HourAngle from KStarsData
// TODO: ecliptic + equator needs partial code of algorithm
// -> solution:
//	-all 3 objects in 1 component (this is messy)
//	-3 components which share a algorithm class
void HorizonComponent::init(KStarsData *data)
{
	pts = new Q3PointArray(2000)
	
	//Define Horizon
	for ( unsigned int i=0; i<NCIRCLE; ++i ) {
		SkyPoint o = new SkyPoint();
		o->setAz( i*24./NCIRCLE );
		o->setAlt( 0.0 );
		o->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
		Horizon.append( o );
	}
}

void HorizonComponent::update(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	if ( Options::showHorizon() || Options::showGround() ) {
	  foreach ( SkyPoint *p, Horizon ) {
			p->HorizontalToEquatorial( LST, data->geo()->lat() );
		}
	}
}

// was SkyMap::drawHorizon
void HorizonComponent::draw(SkyMap *map, QPainter& psky, double scale)
{
	int Width = int( scale * map->width() );
	int Height = int( scale * map->height() );

	QList<QPoint> points;
//	points.setAutoDelete(true); // obsolete in QT4
	QPoint o, o2;

	//Draw Horizon
	//The horizon should not be corrected for atmospheric refraction, so map->getXY has doRefract=false...
	if (Options::showHorizon() || Options::showGround() ) {
		QPoint OutLeft(0,0), OutRight(0,0);

		psky.setPen( QPen( QColor( map->data()->colorScheme()->colorNamed( "HorzColor" ) ), 1, SolidLine ) );
		psky.setBrush( QColor ( map->data()->colorScheme()->colorNamed( "HorzColor" ) ) );
		int ptsCount = 0;
		int maxdist = int(Options::zoomFactor()/4);

		//index of point near the right or top/bottom edge
		uint index1(0), index2(0);
		int xBig(-100); //ridiculous initial value

		foreach ( SkyPoint *p, Horizon ) {
			o = map->getXY( p, Options::useAltAz(), false, scale );  //false: do not refract the horizon
			bool found = false;

			//first iteration for positioning the "Horizon" label:
			//flag the onscreen equator point with the largest x value
			//we don't draw the label while map->slewing, or if the opaque ground is drawn
			if ( ! map->map->slewing && ( ! Options::showGround() || ! Options::useAltAz() )
						&& o.x() > 0 && o.x() < width() && o.y() > 0 && o.y() < height() ) {
				if ( o.x() > xBig ) {
					xBig = o.x();
					index1 = Horizon.at();
				}
			}

			//Use the QList of points to pre-sort visible horizon points
			if ( o.x() > -100 && o.x() < Width + 100 && o.y() > -100 && o.y() < Height + 100 ) {
				if ( Options::useAltAz() ) {
					unsigned int j;
					for ( j=0; j<points.size(); ++j ) {
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

			for ( int i=1; i<points.size(); ++i ) {
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
//						psky.setPen( QColor ( map->data()->colorScheme()->colorNamed( "SkyColor" ) ) );
//						psky.setBrush( QColor ( map->data()->colorScheme()->colorNamed( "SkyColor" ) ) );
//						psky.drawPolygon( bpts, false, 0, bpCount );
//						//Reset colors for Horizon polygon
//						psky.setPen( QColor ( map->data()->colorScheme()->colorNamed( "HorzColor" ) ) );
//						psky.setBrush( QColor ( map->data()->colorScheme()->colorNamed( "HorzColor" ) ) );
					
					} else {
						pts->setPoint( ptsCount++, Width+100, Height+100 );   //bottom right corner
						pts->setPoint( ptsCount++, -100, Height+100 );         //bottom left corner
					}
						
					//Draw the Horizon polygon
					psky.drawPolygon( ( const Q3PointArray ) *pts, false, 0, ptsCount );

					//remove all items in points list
					//FIXME: JH: Do we really want this done here?
					for ( unsigned int i=0; i<points.count(); ++i ) {
						points.remove(i);
					}
				}

//	Draw compass heading labels along horizon
				SkyPoint *c = new SkyPoint;
				QPoint cpoint;

				if ( Options::showGround() )
					psky.setPen( QColor ( map->data()->colorScheme()->colorNamed( "CompassColor" ) ) );
				else
					psky.setPen( QColor ( map->data()->colorScheme()->colorNamed( "HorzColor" ) ) );

		//North
				c->setAz( 359.99 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );
				cpoint = map->getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "North", "N" ) );
				}

		//NorthEast
				c->setAz( 45.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );
				cpoint = map->getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northeast", "NE" ) );
				}

		//East
				c->setAz( 90.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );
				cpoint = map->getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "East", "E" ) );
				}

		//SouthEast
				c->setAz( 135.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );
				cpoint = map->getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southeast", "SE" ) );
				}

		//South
				c->setAz( 179.99 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );
				cpoint = map->getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "South", "S" ) );
				}

		//SouthWest
				c->setAz( 225.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );
				cpoint = map->getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southwest", "SW" ) );
				}

		//West
				c->setAz( 270.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );
				cpoint = map->getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "West", "W" ) );
				}

		//NorthWest
				c->setAz( 315.0 );
				c->setAlt( 0.0 );
				if ( !Options::useAltAz() ) c->HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );
				cpoint = map->getXY( c, Options::useAltAz(), false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northwest", "NW" ) );
				}

				delete c;
			}
		}

		if ( ! map->slewing && (! Options::showGround() || ! Options::useAltAz() ) && xBig > 0 ) {
			//Draw the "Horizon" label.  We have flagged the rightmost onscreen Horizon point.
			//If the zoom level is below 1000, simply adopt this point as the anchor for the
			//label.  If the zoom level is 1000 or higher, we interpolate to find the exact
			//point at which the Horizon goes offscreen, and anchor from that point.
			SkyPoint *p = Horizon.at(index1);
			double ra0(0.0);  //the ra of our anchor point
			double dec0(0.0); //the dec of our anchor point

			if ( Options::zoomFactor() < 1000. ) {
				ra0 = p->ra()->Hours();
				dec0 = p->dec()->Degrees();
			} else {
				//Somewhere between Horizon point p and its immediate neighbor, the Horizon goes
				//offscreen.  Determine the exact point at which this happens.
				index2 = index1 + 1;
				if ( Horizon.count() &&  index2 > Horizon.count() - 1 ) index2 -= Horizon.count();
				SkyPoint *p2 = Horizon.at(index2);

				QPoint o1 = map->getXY( p, Options::useAltAz(), false, scale );
				QPoint o2 = map->getXY( p2, Options::useAltAz(), false, scale );

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
			LabelPoint.EquatorialToHorizontal( map->data()->LST, map->data()->geo()->lat() );
			LabelPoint.setAlt( LabelPoint.alt()->Degrees() - 800./Options::zoomFactor() );
			LabelPoint.setAz( LabelPoint.az()->Degrees() - 4000./Options::zoomFactor() );
			LabelPoint.HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );
			o = map->getXY( &LabelPoint, Options::useAltAz(), false, scale );
			if ( o.x() > width() || o.x() < 0 ) {
				//the LabelPoint is offscreen.  Either we are in the Southern hemisphere,
				//or the sky is rotated upside-down.  Use an azimuth offset of +2.0 degrees
			LabelPoint.setAlt( LabelPoint.alt()->Degrees() + 1600./Options::zoomFactor() );
				LabelPoint.setAz( LabelPoint.az()->Degrees() + 8000./Options::zoomFactor() );
				LabelPoint.HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );
				o = map->getXY( &LabelPoint, Options::useAltAz(), false, scale );
			}

			//p2 is a skypoint offset from LabelPoint by +/-1 degree azimuth (scaled by
			//2000./zoomFactor).  We use p2 to determine the rotation angle for the
			//Horizon label, which we want to be parallel to the line between LabelPoint and p2.
			SkyPoint p2( LabelPoint.ra(), LabelPoint.dec() );
			p2.EquatorialToHorizontal( map->data()->LST, map->data()->geo()->lat() );
			p2.setAz( p2.az()->Degrees() + 2000./Options::zoomFactor() );
			p2.HorizontalToEquatorial( map->data()->LST, map->data()->geo()->lat() );

			//o and o2 are the screen positions of LabelPoint and p2
			o = map->getXY( &LabelPoint, Options::useAltAz(), false, scale );
			o2 = map->getXY( &p2, Options::useAltAz(), false, scale );

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
