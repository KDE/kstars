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

#include "lambertprojector.h"

#include "ksutils.h"

LambertProjector::LambertProjector(const ViewParams& p) : Projector(p)
{
}

SkyMap::Projection LambertProjector::type() const
{
    return SkyMap::Lambert;
}

bool LambertProjector::unusablePoint(const QPointF& p) const
{
    //r0 is the angular size of the sky horizon, in radians
    double r0 = 1.41421356;
    //If the zoom is high enough, all points are usable
    //The center-to-corner distance, in radians
    double r = 0.5*1.41421356*m_vp.width/m_vp.zoomFactor;
    if( r < r0 ) return false;
    //At low zoom, we have to determine whether the point is beyond the sky horizon
    //Convert pixel position to x and y offsets in radians
    double dx = (0.5*m_vp.width  - p.x())/m_vp.zoomFactor;
    double dy = (0.5*m_vp.height - p.y())/m_vp.zoomFactor;
    return (dx*dx + dy*dy) > r0*r0;
}

SkyPoint LambertProjector::fromScreen(const QPointF& p, dms* LST, const dms* lat) const
{
    dms c;
    double sinc, cosc;
    /** N.B. We don't cache these sin/cos values in the inverse
      * projection because it causes 'shaking' when moving the sky.
      */
    double sinY0, cosY0;
    //Convert pixel position to x and y offsets in radians
    double dx = (0.5*m_vp.width  - p.x())/m_vp.zoomFactor;
    double dy = (0.5*m_vp.height - p.y())/m_vp.zoomFactor;

    double r = sqrt( dx*dx + dy*dy );
    c.setRadians( 2.0*asin(0.5*r) );
    c.SinCos( sinc, cosc );

    if( m_vp.useAltAz ) {
        dx = -1.0*dx; //Azimuth goes in opposite direction compared to RA
        m_vp.focus->alt().SinCos( sinY0, cosY0 );
    } else {
        m_vp.focus->dec().SinCos( sinY0, cosY0 );
    }

    double Y = asin( cosc*sinY0 + ( dy*sinc*cosY0 )/r );
    double atop = dx*sinc;
    double abot = r*cosY0*cosc - dy*sinY0*sinc;
    double A = atan2( atop, abot );

    SkyPoint result;
    if ( m_vp.useAltAz ) {
        dms alt, az;
        alt.setRadians( Y );
        az.setRadians( A + m_vp.focus->az().radians() );
        if ( m_vp.useRefraction )
            alt = SkyPoint::unrefract( alt );
        result.setAlt( alt );
        result.setAz( az );
        result.HorizontalToEquatorial( LST, lat );
    } else {
        dms ra, dec;
        dec.setRadians( Y );
        ra.setRadians( A + m_vp.focus->ra().radians() );
        result.set( ra.reduce(), dec );
        result.EquatorialToHorizontal( LST, lat );
    }

    return result;
}

// Lambert Azimuthal Equal-Area:
// http://mathworld.wolfram.com/LambertAzimuthalEqual-AreaProjection.html
Vector2f LambertProjector::toScreenVec(SkyPoint* o, bool oRefract, bool* onVisibleHemisphere) const
{
    double Y, dX;
    double sindX, cosdX, sinY, cosY;

    oRefract &= m_vp.useRefraction;
    if ( m_vp.useAltAz ) {
        if ( oRefract )
            Y = SkyPoint::refract( o->alt() ).radians(); //account for atmospheric refraction
        else
            Y = o->alt().radians();
        dX = m_vp.focus->az().reduce().radians() - o->az().reduce().radians();
    } else {
        dX = o->ra().reduce().radians() - m_vp.focus->ra().reduce().radians();
        Y = o->dec().radians();
    }

    dX = KSUtils::reduceAngle(dX, -dms::PI, dms::PI);

    //Convert dX, Y coords to screen pixel coords, using GNU extension if available
    #if ( __GLIBC__ >= 2 && __GLIBC_MINOR__ >=1 )
    sincos( dX, &sindX, &cosdX );
    sincos( Y, &sinY, &cosY );
    #else
    sindX = sin(dX);   cosdX = cos(dX);
    sinY  = sin(Y);    cosY  = cos(Y);
    #endif

    //c is the cosine of the angular distance from the center
    double c = m_sinY0*sinY + m_cosY0*cosY*cosdX;

    //If c is less than 0.0, then the "field angle" (angular distance from the focus)
    //is more than 90 degrees.  This is on the "back side" of the celestial sphere
    //and should not be drawn.
    if( onVisibleHemisphere )
        *onVisibleHemisphere = (c >= 0.0);

    double k = sqrt( 2.0/( 1.0 + c ) );

    return Vector2f( 0.5*m_vp.width  - m_vp.zoomFactor*k*cosY*sindX,
                     0.5*m_vp.height - m_vp.zoomFactor*k*( m_cosY0*sinY - m_sinY0*cosY*cosdX ) );
}

