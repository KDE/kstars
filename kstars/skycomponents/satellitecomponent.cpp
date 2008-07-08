/***************************************************************************
                          satellitecomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 14 July 2006
    copyright            : (C) 2006 by Jason Harris
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

#include "satellitecomponent.h"

#include <QBrush>
#include <QPainter>

#include "kstarsdata.h"
#include "skymap.h"
#include "skyline.h"
#include "Options.h"

SatelliteComponent::SatelliteComponent(SkyComponent *parent )
        : LineListComponent(parent)
{}

SatelliteComponent::~SatelliteComponent()
{}

bool SatelliteComponent::selected()
{
    return Options::showSatellites();
}


void SatelliteComponent::init( const QString &name, KStarsData *data, SPositionSat *pSat[], int npos ) {
    setLabel( name );
    setLabelPosition( LineListComponent::RightEdgeLabel );

    setPen( QPen( QBrush( data->colorScheme()->colorNamed( "SatColor" ) ),
                  2.5, Qt::SolidLine ) );

    for ( int i=0; i<npos; i++ ) {
        SkyPoint* p = new SkyPoint();
        p->setAlt( pSat[i]->sat_ele );
        p->setAz( pSat[i]->sat_azi );
        p->HorizontalToEquatorial( data->lst(), data->geo()->lat() );

        appendP( p );
        jdList().append( pSat[i]->jd );

    }
}

void SatelliteComponent::draw( QPainter &psky ) {
    LineListComponent::draw( psky );

    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    if ( jdList().size() == 0 ) return;

    //Add tickmarks and timestamps along the portion of the
    //satellite track that is above the horizon
    //The time at each position is stored in the pSat array
    //as the julian day.  Parse these days, and interpolate to find
    //times with :00 seconds (e.g., 12:34:00)
    KStarsDateTime dtLast( jdList()[0] );
    for ( int i=1; i<jdList().size(); ++i ) {
        KStarsDateTime dt( jdList()[i] );
        SkyPoint *sp = points()->at(i);
        SkyPoint *sp2;
        if ( i<points()->size()-1 )
            sp2 = points()->at(i+1);
        else
            sp2 = points()->at(i-1);

        if ( sp->alt()->Degrees() > 0.0
                && dt.time().minute() != dtLast.time().minute() ) {
            double t1 = double(dtLast.time().second());
            double t2 = double(dt.time().second()) + 60.0;
            double f = ( 60.0 - t1 )/( t2 - t1 );

            //Determine the position of the tickmark along
            //the track, corresponding to the even-second time.
            //f is the fractional distance between the endpoints.
            double ra = f*sp->ra()->Hours() + (1.0-f)*sp2->ra()->Hours();
            double dc = f*sp->dec()->Degrees() + (1.0-f)*sp2->dec()->Degrees();
            SkyPoint sTick1(ra, dc);
            sTick1.EquatorialToHorizontal( data->lst(), data->geo()->lat() );

            //To draw a line perpendicular to the satellite track at the position of the tick,
            //We take advantage of QLineF::normalVector().  So first generate a QLineF that
            //lies along the satellite track, from sTick1 to sp2 (which is a nearby position
            //along the track).  Then change its length to 10 pixels, and finall use
            //normalVector() to rotate it 90 degrees.
            QLineF seg( map->toScreen( &sTick1 ), map->toScreen( sp2 ) );
            seg.setLength( 10.0 );
            QLineF tick = seg.normalVector();

            //If the tick is extending below the satellite track, rotate it by 180 degrees
            if ( tick.y2() < tick.y1() ) {
                //Rotate tick by 180 degrees
                double x1 = tick.x1();
                double y1 = tick.y1();
                tick = QLineF( x1, y1, x1 - tick.dx(), y1 - tick.dy() );
            }
            psky.drawLine( tick );

            //Now, add a label to the tickmark showing the time
            double labelpa = atan2( tick.dy(), tick.dx() )/dms::DegToRad;
            if ( labelpa > 270.0 ) labelpa -= 360.0;
            else if ( labelpa > 90.0 ) labelpa -= 180.0;
            else if ( labelpa < -270.0 ) labelpa += 360.0;
            else if ( labelpa < -90.0  ) labelpa += 180.0;

            QTime tlabel = dt.time().addSecs( int(3600.0*data->geo()->TZ()) );
            psky.save();
            QFont stdFont( psky.font() );
            QFont smallFont( stdFont );
            smallFont.setPointSize( stdFont.pointSize() - 2 );
            psky.setFont( smallFont );
            psky.translate( tick.p2() );
            psky.rotate( labelpa );
            psky.drawText( QRectF(-25.0, 6.0, 40.0, 10.0), Qt::AlignCenter,
                           tlabel.toString( "hh:mm" ) );
            psky.restore();
            psky.setFont( stdFont );
        }

        dtLast = dt;
    }

}

