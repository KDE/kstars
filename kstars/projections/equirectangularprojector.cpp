/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) <year>  <name of author>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include "equirectangularprojector.h"

#include "ksutils.h"
#include "kstarsdata.h"
#include "skycomponents/skylabeler.h"

EquirectangularProjector::EquirectangularProjector(const ViewParams& p)
    : Projector(p)
{

}

SkyMap::Projection EquirectangularProjector::type() const
{
    return SkyMap::Equirectangular;
}

double EquirectangularProjector::radius() const
{
    return 1.0;
}

Vector2f EquirectangularProjector::toScreenVec(SkyPoint* o, bool oRefract, bool* onVisibleHemisphere) const
{
    double Y, dX;
    Vector2f p;

    oRefract &= m_vp.useRefraction;
    if ( m_vp.useAltAz ) {
        if ( oRefract )
            Y = SkyPoint::refract( o->alt() ).radians(); //account for atmospheric refraction
        else
            Y = o->alt().radians();
        dX = m_vp.focus->az().reduce().radians() - o->az().reduce().radians();
        
        p[1] = 0.5*m_vp.height - m_vp.zoomFactor*(Y - m_vp.focus->alt().radians());
    } else {
        dX = o->ra().reduce().radians() - m_vp.focus->ra().reduce().radians();
        Y = o->dec().radians();
        p[1] = 0.5*m_vp.height - m_vp.zoomFactor*(Y - m_vp.focus->dec().radians());
    }

    dX = KSUtils::reduceAngle(dX, -dms::PI, dms::PI);

    p[0] = 0.5*m_vp.width - m_vp.zoomFactor*dX;
    
    if ( onVisibleHemisphere )
        //Is fabs(dX) < M_PI/2?
        *onVisibleHemisphere = dX*dX < M_PI*M_PI/4.;

    return p;
}

SkyPoint EquirectangularProjector::fromScreen(const QPointF& p, dms* LST, const dms* lat) const
{
    SkyPoint result;

    //Convert pixel position to x and y offsets in radians
    double dx = (0.5*m_vp.width  - p.x())/m_vp.zoomFactor;
    double dy = (0.5*m_vp.height - p.y())/m_vp.zoomFactor;

        if ( m_vp.useAltAz ) {
            dms az, alt;
            dx = -1.0*dx;  //Azimuth goes in opposite direction compared to RA
            az.setRadians( dx + m_vp.focus->az().radians() );
            alt.setRadians( dy + m_vp.focus->alt().radians() );
            result.setAz( az.reduce() );
            if ( m_vp.useRefraction )
                alt = SkyPoint::unrefract( alt );
            result.setAlt( alt );
            result.HorizontalToEquatorial( LST, lat );
            return result;
        } else {
            dms ra, dec;
            ra.setRadians( dx + m_vp.focus->ra().radians() );
            dec.setRadians( dy + m_vp.focus->dec().radians() );
            result.set( ra.reduce(), dec );
            result.EquatorialToHorizontal( LST, lat );
            return result;
        }
}

QVector< Vector2f > EquirectangularProjector::groundPoly(SkyPoint* labelpoint, bool* drawLabel) const
{
    if( m_vp.useAltAz ) {
        SkyPoint belowFocus;
        belowFocus.setAz( m_vp.focus->az().Degrees() );
        belowFocus.setAlt( 0.0 );

        Vector2f obf = toScreenVec( &belowFocus, false );

        //If the horizon is off the bottom edge of the screen,
        //we can return immediately
        if ( obf.y() > m_vp.height ) {
            if( drawLabel ) drawLabel = false;
            return QVector<Vector2f>();
        }

        //We can also return if the horizon is off the top edge,
        //as long as the ground poly is not being drawn
        if ( obf.y() < 0. && m_vp.fillGround == false ) {
            if( drawLabel ) drawLabel = false;
            return QVector<Vector2f>();
        }

        QVector<Vector2f> ground;
        //Construct the ground polygon, which is a simple rectangle in this case
        ground << Vector2f( -10.f, obf.y() )
               << Vector2f( m_vp.width + 10.f, obf.y() )
               << Vector2f( m_vp.width + 10.f, m_vp.height + 10.f )
               << Vector2f( -10.f, m_vp.height + 10.f );

        if( labelpoint ) {
            QPointF pLabel( m_vp.width-30., obf.y() );
            KStarsData *data = KStarsData::Instance();
            *labelpoint = fromScreen(pLabel, data->lst(), data->geo()->lat());
        }
        if( drawLabel )
            *drawLabel = true;
        
        return ground;
    } else {
        return Projector::groundPoly(labelpoint, drawLabel);
    }
}

