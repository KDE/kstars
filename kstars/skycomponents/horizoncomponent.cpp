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
#include <QPointF>

#include "Options.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#endif
#include "skyobjects/skypoint.h" 
#include "dms.h"
#include "skylabeler.h"
#include "skypainter.h"

#include "projections/projector.h"

#define NCIRCLE 360   //number of points used to define equator, ecliptic and horizon

HorizonComponent::HorizonComponent(SkyComposite *parent )
        : PointListComponent( parent )
{
    KStarsData *data = KStarsData::Instance();
    emitProgressText( i18n("Creating horizon" ) );

    //Define Horizon
    for ( unsigned int i=0; i<NCIRCLE; ++i ) {
        SkyPoint *o = new SkyPoint();
        o->setAz( i*360./NCIRCLE );
        o->setAlt( 0.0 );

        o->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        pointList().append( o );
    }
}

HorizonComponent::~HorizonComponent()
{}

bool HorizonComponent::selected()
{
    return Options::showHorizon() || Options::showGround();
}

void HorizonComponent::update( KSNumbers * )
{
    if ( ! selected() )
        return;
    KStarsData *data = KStarsData::Instance();
    foreach ( SkyPoint *p, pointList() ) {
        p->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
    }
}

//Only half of the Horizon circle is ever valid, the invalid half is "behind" the observer.
//To select the valid half, we start with the azimuth of the central focus point.
//The valid horizon points have azimuth between this az +- 90
//This is true for Equatorial or Horizontal coordinates
void HorizonComponent::draw( SkyPainter *skyp )
{
    if( !selected() )
        return;

    KStarsData *data = KStarsData::Instance();
    
    // If we are in GL mode, we need to flush buffers now since we don't do it before 
    // drawing ground, otherwise, objects remaining in buffers will be drawn after 
    // the ground and will appears over it.
    if ( Options::useGL() )
        skyp->end();

    skyp->setPen( QPen( QColor( data->colorScheme()->colorNamed( "HorzColor" ) ), 2, Qt::SolidLine ) );

    if ( Options::showGround() )
        skyp->setBrush( QColor ( data->colorScheme()->colorNamed( "HorzColor" ) ) );
    else
        skyp->setBrush( Qt::NoBrush );

    SkyPoint labelPoint;
    bool drawLabel;
    skyp->drawHorizon( Options::showGround(), &labelPoint, &drawLabel );

    if( drawLabel ) {
        SkyPoint labelPoint2;
        labelPoint2.setAlt(0.0);
        labelPoint2.setAz( labelPoint.az().Degrees() + 1.0 );
        labelPoint2.HorizontalToEquatorial( data->lst(), data->geo()->lat() );
    }
    #ifdef __GNUC__
    #warning Still have to port HorizonComponent::draw()
    #endif
    #if 0
    if ( ! selected() ) return;

    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    float Width = map->scale() * map->width();
    float Height = map->scale() * map->height();
    QPolygonF groundPoly;
    SkyPoint *pAnchor(0), *pAnchor2(0);

    static const QString horizonLabel = i18n("Horizon");
    float marginLeft, marginRight, marginTop, marginBot;
    SkyLabeler::Instance()->getMargins( horizonLabel, &marginLeft, &marginRight,
                                        &marginTop, &marginBot );

    psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "HorzColor" ) ), 2, Qt::SolidLine ) );

    if ( Options::useAltAz() && Options::showGround() ) {
        psky.setBrush( QColor ( data->colorScheme()->colorNamed( "HorzColor" ) ) );
    }
    else {
        psky.setBrush( Qt::NoBrush );
    }

    double daz = 90.;
    if ( Options::useAltAz() ) {
        daz = 0.5*Width*57.3/Options::zoomFactor(); //center to edge, in degrees
        if ( Options::projection() == SkyMap::Orthographic ) {
            daz = daz * 1.4;
        }
    }
    if ( daz > 90.0 ) daz = 90.0;

    double az1 = map->focus()->az().Degrees() - daz;
    double az2 = map->focus()->az().Degrees() + daz;

    QPointF o;
    bool allGround(true);
    bool allSky(true);

    //Special case for equirectangular mode and Alt/Az coordinates
    if ( Options::projection() == SkyMap::Equirectangular && Options::useAltAz() == true ) {
        SkyPoint belowFocus;
        belowFocus.setAz( map->focus()->az().Degrees() );
        belowFocus.setAlt( 0.0 );

        QPointF obf = map->toScreen( &belowFocus, false );

        //If the horizon is off the bottom edge of the screen, 
        //we can return immediately
        if ( obf.y() > Height ) return;

        //We can also return if the horizon is off the top edge, 
        //as long as the ground poly is not being drawn
        if ( obf.y() < 0. ) {
            if ( Options::showGround() == false ) return;
            obf.setY( -10. );
        }

        //Construct the ground polygon, which is a simple rectangle in this case
        groundPoly << QPointF( -10., obf.y() ) << QPointF( Width + 10., obf.y() )
            << QPointF( Width + 10., Height + 10. )
            << QPointF( -10., Height + 10. );

        psky.drawPolygon( groundPoly );

        //Draw the Horizon Label
        if ( Options::showGround() && Options::useAltAz() )
            psky.setPen( QColor ( data->colorScheme()->colorNamed( "CompassColor" ) ) );
        else
            psky.setPen( QColor ( data->colorScheme()->colorNamed( "HorzColor" ) ) );

        QPointF pLabel( Width-30., obf.y() );
        SkyLabeler::Instance()->drawGuideLabel( pLabel, horizonLabel, 0.0 );

        drawCompassLabels( psky );
        return;
    }

    //Add points on the left that may be slightly West of North
    if ( az1 < 0. ) {
        az1 += 360.;
        foreach ( SkyPoint *p, pointList() ) {
            if ( p->az().Degrees() > az1 ) {
                o = map->toScreen( p, false );
                if ( ! map->isPointNull( o ) ) {
                    groundPoly << o;

                    //Set the anchor point if this point is onscreen
                    if ( o.x() < marginRight && o.y() > marginTop && o.y() < marginBot )
                        pAnchor = p;

                    if ( o.y() > 0. ) allGround = false;
                    if ( o.y() < Height ) allSky = false;
                }
            }
        }
        az1 = 0.0;
    }

    //Add points in normal range, 0 to 360
    foreach ( SkyPoint *p, pointList() ) {
        if ( p->az().Degrees() > az1 && p->az().Degrees() < az2 ) {
            o = map->toScreen( p, false );
            if ( ! map->isPointNull( o ) ) {
                groundPoly << o;

                //Set the anchor point if this point is onscreen
                if ( o.x() < marginRight && o.y() > marginTop && o.y() < marginBot )
                    pAnchor = p;

                if ( o.y() > 0. ) allGround = false;
                if ( o.y() < Height ) allSky = false;
            }
        } else if ( p->az().Degrees() > az2 )
            break;
    }

    //Add points on the right that may be slightly East of North
    if ( az2 > 360. ) {
        az2 -= 360.;
        foreach ( SkyPoint *p, pointList() ) {
            if ( p->az().Degrees() < az2 ) {
                o = map->toScreen( p, false );
                if ( ! map->isPointNull( o ) ) {
                    groundPoly << o;

                    //Set the anchor point if this point is onscreen
                    if ( o.x() < marginRight && o.y() > marginTop && o.y() < marginBot )
                        pAnchor = p;

                    if ( o.y() > 0. ) allGround = false;
                    if ( o.y() < Height ) allSky = false;
                }
            } else {
                break;
            }
        }
    }

    if ( allSky ) return; //no horizon onscreen

    if ( allGround ) {
        //Ground fills the screen.  Reset groundPoly to surround screen perimeter
        //Just draw the poly (if ground is filled)
        //No need for compass labels or "Horizon" label
        if ( Options::useAltAz() && Options::showGround() ) {
            groundPoly.clear();
            groundPoly << QPointF( -10., -10. )
            << QPointF( Width + 10., -10. )
            << QPointF( Width + 10., Height + 10. )
            << QPointF( -10., Height + 10. );
            psky.drawPolygon( groundPoly );
        }
        return;
    }

    //groundPoly now contains QPointF's of the screen coordinates of points
    //along the "front half" of the Horizon, in order from left to right.
    //If we are using Equatorial coords, then all we need to do is connect
    //these points.
    //Do not connect points if they are widely separated at low zoom (this
    //avoids connected horizon lines to invalid offscreen points)
    if ( ! Options::useAltAz() ) {
        for ( int i=1; i < groundPoly.size(); ++i ) {
            if ( Options::zoomFactor() > 5000 )
                psky.drawLine( groundPoly.at(i-1), groundPoly.at(i) );
            else {
                float dx = (groundPoly.at(i-1).x() - groundPoly.at(i).x());
                float dy = (groundPoly.at(i-1).y() - groundPoly.at(i).y());
                float r2 = dx*dx + dy*dy;
                if ( r2 < 40000. )  //separated by less than 200 pixels?
                    psky.drawLine( groundPoly.at(i-1), groundPoly.at(i) );
            }
        }

        //If we are using Horizontal coordinates, there is more work to do.
        //We need to complete the ground polygon by going right to left.
        //If the zoomLevel is high (as indicated by (daz<75.0)), then we
        //complete the polygon by simply adding points along thebottom edge
        //of the screen.  If the zoomLevel is high (daz>75.0), then we add points
        //along the bottom edge of the sky circle.  The sky circle has
        //a radius determined by the projection scheme, and the endpoints of the current
        //groundPoly points lie 180 degrees apart along its circumference.
        //We determine the polar angles t1, t2 corresponding to these end points,
        //and then step along the circumference, adding points between them.
        //(In Horizontal coordinates, t1 and t2 are always 360 and 180, respectively).
    } else { //Horizontal coords

        //In Gnomonic projection, or if sufficiently zoomed in, we can complete 
        //the ground polygon by simply adding offscreen points
        if ( daz < 25.0 || Options::projection() == SkyMap::Gnomonic ) { 
            groundPoly << QPointF( Width + 10., groundPoly.last().y() )
                << QPointF( Width + 10., Height + 10. )
                << QPointF( -10., Height + 10. )
                << QPointF( -10., groundPoly.first().y() );

        //For other projections at low zoom, we complete the ground polygon by tracing 
        //along the bottom of sky's horizon circle (i.e., the locus of points that are 
        //90 degrees from the focus point)
        } else { 
            //r0, the radius of the sky circle is determined by the projection scheme:
            double r0 = map->scale()*Options::zoomFactor();
            switch ( Options::projection() ) {
            case SkyMap::Lambert:
                //r0 * 2 * sin(PI/4) = 1.41421356
                r0 = r0*1.41421356;
                break;
            case SkyMap::AzimuthalEquidistant:
                //r0*PI/2 = 1.57079633
                r0 = r0*1.57079633;
                break;
            case SkyMap::Orthographic:
                //r0*sin(PI/2) = 1.0
                break;
            case SkyMap::Stereographic:
                //r0*2*tan(PI/4) = 2.0
                r0 = 2.0*r0;
                break;
            default:
                qWarning() << i18n("Unrecognized coordinate projection: ") << Options::projection() ;
                //default to Orthographic
                break;
            }

            double t1 = 360.;
            double t2 = 180.;

            //NOTE: Uncomment if we ever want opaque ground while using Equatorial coords
            //NOTE: We would have to add consideration for useAntialias to this commented code
            // 		if ( ! Options::useAltAz() ) { //compute t1,t2
            // 			//groundPoly.last() is the point on the Horizon that intersects
            // 			//the visible sky circle on the right
            // 			t1 = -1.0*acos( (groundPoly.last().x() - 0.5*Width)/r0/Options::zoomFactor() )/dms::DegToRad; //angle in degrees
            // 			//Resolve quadrant ambiguity
            // 			if ( groundPoly.last().y() < 0. ) t1 = 360. - t1;
            //
            // 			t2 = t1 - 180.;
            // 		}

            for ( double t=t1; t >= t2; t-=2. ) {  //step along circumference
                dms a( t );
                double sa(0.), ca(0.);
                a.SinCos( sa, ca );
                float xx = 0.5*Width  + r0*ca;
                float yy = 0.5*Height - r0*sa;

                groundPoly << QPoint( int(xx), int(yy) );
            }
        }

        //Finally, draw the ground Polygon.
        psky.drawPolygon( groundPoly );
    }

    //Set color for compass labels and "Horizon" label.
    if ( Options::showGround() && Options::useAltAz() )
        psky.setPen( QColor ( data->colorScheme()->colorNamed( "CompassColor" ) ) );
    else
        psky.setPen( QColor ( data->colorScheme()->colorNamed( "HorzColor" ) ) );


    //Draw Horizon name label
    //pAnchor contains the last point of the Horizon before it went offcreen
    //on the right/top/bottom edge.  oAnchor2 is the next point after oAnchor.
    if ( ! pAnchor ) {
        drawCompassLabels( psky );
        return;
    }

    int iAnchor = pointList().indexOf( pAnchor );
    if ( iAnchor == pointList().size()-1 ) iAnchor = 0;
    else iAnchor++;
    pAnchor2 = pointList().at( iAnchor );

    QPointF o1 = map->toScreen( pAnchor, false );
    QPointF o2 = map->toScreen( pAnchor2, false );

    float x1, x2;
    //there are 3 possibilities:  (o2.x() > width()); (o2.y() < 0); (o2.y() > height())
    if ( o2.x() > Width ) {
        x1 = (Width - o1.x())/(o2.x() - o1.x());
        x2 = (o2.x() - Width)/(o2.x() - o1.x());
    } else if ( o2.y() < 0 ) {
        x1 = o1.y()/(o1.y() - o2.y());
        x2 = -1.0*o2.y()/(o1.y() - o2.y());
    } else if ( o2.y() > Height ) {
        x1 = (Height - o1.y())/(o2.y() - o1.y());
        x2 = (o2.y() - Height)/(o2.y() - o1.y());
    } else {  //should never get here
        x1 = 0.0;
        x2 = 1.0;
    }

    //ra0 is the exact RA at which the Horizon intersects a screen edge
    double ra0 = x1*pAnchor2->ra().Hours() + x2*pAnchor->ra().Hours();
    //dec0 is the exact Dec at which the Horizon intersects a screen edge
    double dec0 = x1*pAnchor2->dec().Degrees() + x2*pAnchor->dec().Degrees();

    //LabelPoint is offset from the anchor point by -2.0 degrees in azimuth
    //and -0.4 degree altitude, scaled by 2000./zoomFactor so that they are
    //independent of zoom.
    SkyPoint LabelPoint(ra0, dec0);
    LabelPoint.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    LabelPoint.setAlt( LabelPoint.alt().Degrees() - 800./Options::zoomFactor() );
    LabelPoint.setAz( LabelPoint.az().Degrees() - 4000./Options::zoomFactor() );
    LabelPoint.HorizontalToEquatorial( data->lst(), data->geo()->lat() );

    o = map->toScreen( &LabelPoint, false );

    if ( o.x() > Width || o.x() < 0 ) {
        //the LabelPoint is offscreen.  Either we are in the Southern hemisphere,
        //or the sky is rotated upside-down.  Use an azimuth offset of +2.0 degrees
        LabelPoint.setAlt( LabelPoint.alt().Degrees() + 1600./Options::zoomFactor() );
        LabelPoint.setAz( LabelPoint.az().Degrees() + 8000./Options::zoomFactor() );
        LabelPoint.HorizontalToEquatorial( data->lst(), data->geo()->lat() );
    }

    //p2 is a skypoint offset from LabelPoint by +/-1 degree azimuth (scaled by
    //2000./zoomFactor).  We use p2 to determine the rotation angle for the
    //Horizon label, which we want to be parallel to the line between LabelPoint and p2.
    SkyPoint p2 = LabelPoint;
    p2.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    p2.setAz( p2.az().Degrees() + 2000./Options::zoomFactor() );
    p2.HorizontalToEquatorial( data->lst(), data->geo()->lat() );

    o2 = map->toScreen( &p2, false );

    float sx = o.x() - o2.x();
    float sy = o.y() - o2.y();
    float angle = atan2( sy, sx )*180.0/dms::PI;
    if ( angle < -90.0 ) angle += 180.0;
    if ( angle >  90.0 ) angle -= 180.0;

    SkyLabeler::Instance()->drawGuideLabel( o1, horizonLabel, angle );
#endif
    drawCompassLabels();
}

void HorizonComponent::drawCompassLabels() {
#ifndef KSTARS_LITE
    SkyPoint c;
    QPointF cpoint;
    bool visible;

    const Projector *proj = SkyMap::Instance()->projector();
    KStarsData *data = KStarsData::Instance();
    
    SkyLabeler* skyLabeler = SkyLabeler::Instance();
    // Set proper color for labels
    QColor color( data->colorScheme()->colorNamed( "CompassColor" ) );
    skyLabeler->setPen( QPen( QBrush(color), 1, Qt::SolidLine) );

    double az = -0.01;
    static QString name[8];
    name[0] = i18nc( "Northeast", "NE" );
    name[1] = i18nc( "East", "E" );
    name[2] = i18nc( "Southeast", "SE" );
    name[3] = i18nc( "South", "S" );
    name[4] = i18nc( "Southwest", "SW" );
    name[5] = i18nc( "West", "W" );
    name[6] = i18nc( "Northwest", "NW" );
    name[7] = i18nc( "North", "N" );

    for ( int i = 0; i < 8; i++ ) {
        az += 45.0;
        c.setAz( az );
        c.setAlt( 0.0 );
        if ( !Options::useAltAz() ) {
            c.HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        }
        
        cpoint = proj->toScreen( &c, false, &visible );
        if ( visible && proj->onScreen(cpoint) ) {
            skyLabeler->drawGuideLabel( cpoint, name[i], 0.0 );
        }
    }
#endif
}
