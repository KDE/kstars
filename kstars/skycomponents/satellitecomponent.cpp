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
#include "skyobjects/skyline.h"
#include "skylabeler.h"
#include "Options.h"

SatelliteComponent::SatelliteComponent(SkyComposite *parent ) :
    SkyComponent(parent),
    LabelPosition( NoLabel ),
    m_skyLabeler( SkyLabeler::Instance() )

{}

SatelliteComponent::~SatelliteComponent()
{}

bool SatelliteComponent::selected()
{
    return Options::showSatellites();
}

// I don't think the ecliptic or the celestial equator should precess. -jbb
void SatelliteComponent::update( KSNumbers * )
{
    if ( ! selected() )
        return;
    KStarsData *data = KStarsData::Instance();

    foreach ( SkyPoint* p, pointList ) {
        //if ( num ) p->updateCoords( num );
        p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    }
}

void SatelliteComponent::initSat( const QString &name, SPositionSat *pSat[], int npos ) {
    KStarsData *data = KStarsData::Instance();
    Label = name;
    LabelPosition = RightEdgeLabel;
    Pen = QPen( QBrush( data->colorScheme()->colorNamed( "SatColor" ) ),
                2.5, Qt::SolidLine );

    for ( int i=0; i<npos; i++ ) {
        SkyPoint* p = new SkyPoint();
        p->setAlt( pSat[i]->sat_ele );
        p->setAz( pSat[i]->sat_azi );
        p->HorizontalToEquatorial( data->lst(), data->geo()->lat() );

        pointList.append( p );
        JDList.append( pSat[i]->jd );

    }
}

void SatelliteComponent::drawLinesComponent( QPainter &psky )
{
    if ( ! selected() )
        return;

    SkyMap *map = SkyMap::Instance();

    psky.setPen( Pen );

    bool isVisible, isVisibleLast;
    SkyPoint  *pLast, *pThis;

    // These are used to keep track of the element that is farrthest left,
    // farthest right, etc.
    float xLeft  = 100000.;
    float xRight = 0.;
    float yTop   = 100000.;
    float yBot   = 0.;

    // These are the indices of the farthest left point, farthest right point,
    // etc.  The are data members so the drawLabels() routine can use them.
    // Zero indicates an index that was never set and is considered invalid
    // inside of drawLabels().
    m_iLeft = m_iRight = m_iTop = m_iBot = 0;

    // We don't draw the label here but we need the proper font in order to set
    // the margins correctly.  Since psky no contains the zoom dependent font as
    // its default font, we need to play the little dance below.
    QFontMetricsF fm( psky.font() );

    // Create the margins within which is is okay to draw the label
    float sideMargin   =  fm.width("MM") + fm.width( Label ) / 2.0;

    float leftMargin  = sideMargin;
    float rightMargin = psky.window().width() - sideMargin;
    float topMargin   = fm.height();
    float botMargin   = psky.window().height() - 2.0 * fm.height();

    QPointF oThis, oLast, oMid;

    pLast = pointList.at( 0 );
    oLast = map->toScreen( pLast, true, &isVisibleLast );

    int limit = pointList.size();

    for ( int i=1 ; i < limit ; i++ ) {
        pThis = pointList.at( i );
        oThis = map->toScreen( pThis, true, &isVisible );

        if ( map->onScreen(oThis, oLast ) ) {
            if ( isVisible && isVisibleLast ) {
                psky.drawLine( oLast, oThis );

                // Keep track of index of leftmost, rightmost, etc point.
                // Only allow points that fit within the margins.
                qreal x = oThis.x();
                qreal y = oThis.y();
                if ( x > leftMargin && x < rightMargin &&
                        y < botMargin  && y > topMargin) {

                    if ( x < xLeft ) {
                        m_iLeft = i;
                        xLeft = x;
                    }
                    if ( x > xRight ) {
                        m_iRight = i;
                        xRight = x;
                    }
                    if (  y > yBot ) {
                        m_iBot = i;
                        yBot = y;
                    }
                    if ( y < yTop ) {
                        m_iTop = i;
                        yTop = y;
                    }
                }
            }

            else if ( isVisibleLast && ! isVisible ) {
                oMid = map->clipLine( pLast, pThis );
                psky.drawLine( oLast, oMid );
            }
            else if ( isVisible && ! isVisibleLast ) {
                oMid = map->clipLine( pThis, pLast );
                psky.drawLine( oMid, oThis );
            }
        }

        pLast = pThis;
        oLast = oThis;
        isVisibleLast = isVisible;
    }

    drawLabels( psky );
}

void SatelliteComponent::draw( QPainter &psky ) {
    drawLinesComponent( psky );

    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    if ( JDList.size() == 0 ) return;

    //Add tickmarks and timestamps along the portion of the
    //satellite track that is above the horizon
    //The time at each position is stored in the pSat array
    //as the julian day.  Parse these days, and interpolate to find
    //times with :00 seconds (e.g., 12:34:00)
    KStarsDateTime dtLast( JDList[0] );
    for ( int i=1; i<JDList.size(); ++i ) {
        KStarsDateTime dt( JDList[i] );
        SkyPoint *sp = pointList.at(i);
        SkyPoint *sp2;
        if ( i<pointList.size()-1 )
            sp2 = pointList.at(i+1);
        else
            sp2 = pointList.at(i-1);

        if ( sp->alt().Degrees() > 0.0
                && dt.time().minute() != dtLast.time().minute() ) {
            double t1 = double(dtLast.time().second());
            double t2 = double(dt.time().second()) + 60.0;
            double f = ( 60.0 - t1 )/( t2 - t1 );

            //Determine the position of the tickmark along
            //the track, corresponding to the even-second time.
            //f is the fractional distance between the endpoints.
            double ra = f*sp->ra().Hours() + (1.0-f)*sp2->ra().Hours();
            double dc = f*sp->dec().Degrees() + (1.0-f)*sp2->dec().Degrees();
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

void SatelliteComponent::drawLabels( QPainter& psky )
{
    if ( LabelPosition == NoLabel ) return;

    SkyMap *map = SkyMap::Instance();

    double comfyAngle = 40.0;  // the first valid candidate with an angle
    // smaller than this gets displayed.  If you set
    // this to > 90. then the first valid candidate
    // will be displayed, regardless of angle.

    // We store info about the four candidate points in arrays to make several
    // of the steps easier, particularly choosing the valid candiate with the
    // smallest angle from the horizontal.

    int     i[4];                                  // index of candidate
    double  a[4] = { 360.0, 360.0, 360.0, 360.0 }; // angle, default to large value
    QPointF o[4];                                  // candidate point
    bool okay[4] = { true, true, true, true };     // flag  candidate false if it
    // overlaps a previous label.

    // Try candidate in different orders depending on if the label was to be
    // near the left or right side of the screen.
    if ( LabelPosition == LeftEdgeLabel ) {
        i[0] = m_iLeft;
        i[1] = m_iRight;
        i[2] = m_iBot;
        i[3] = m_iTop;
    }
    else {
        i[0] = m_iRight;
        i[1] = m_iLeft;
        i[2] = m_iBot;
        i[3] = m_iTop;
    }

    // Make sure we have at least one valid point
    int firstI = 0;

    for ( ; firstI < 4; firstI++ ) {
        if ( i[firstI] ) break;
    }

    // return if there are no valid candidates
    if ( firstI >= 4 ) return;


    // Try the points in order and print the label if we can draw it at
    // a comfortable angle for viewing;
    for ( int j = firstI; j < 4; j++ ) {
        o[j] = angleAt( map, i[j], &a[j] );
        if ( fabs( a[j] ) > comfyAngle ) continue;
        if ( m_skyLabeler->drawGuideLabel( o[j], Label, a[j] ) )
            return;
        okay[j] = false;
    }

    //--- No angle was comfy so pick the one with the smallest angle ---

    // Index of the index/angle/point that gets displayed
    int bestI = firstI;

    // find first valid candidate that does not overlap existing labels
    for ( ; bestI < 4; bestI++ ) {
        if ( i[bestI] && okay[bestI] ) break;
    }

    // return if all candiates either overlap or are invalid
    if ( bestI >= 4 ) return;

    // find the valid non-overlap candidate with the smallest angle
    for ( int j = bestI + 1; j < 4; j++ ) {
        if ( i[j] && okay[j] && fabs(a[j]) < fabs(a[bestI]) ) bestI = j;
    }

    m_skyLabeler->drawGuideLabel( o[bestI], Label, a[bestI] );
}


QPointF SatelliteComponent::angleAt( SkyMap* map, int i, double *angle )
{
    SkyPoint* pThis = pointList.at( i );
    SkyPoint* pLast = pointList.at( i - 1 );

    QPointF oThis = map->toScreen( pThis );
    QPointF oLast = map->toScreen( pLast );

    double sx = double( oThis.x() - oLast.x() );
    double sy = double( oThis.y() - oLast.y() );

    *angle = atan2( sy, sx ) * 180.0 / dms::PI;

    // Never draw the label upside down
    if ( *angle < -90.0 ) *angle += 180.0;
    if ( *angle >  90.0 ) *angle -= 180.0;

    return oThis;
}
