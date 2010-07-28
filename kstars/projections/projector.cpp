/*
    Copyright (C) 2010 Henry de Valence <hdevalence@gmail.com>

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

#include "projector.h"

#include "ksutils.h"
#include "kstarsdata.h"

namespace {
    void toXYZ(SkyPoint* p, double *x, double *y, double *z) {
        double sinRa, sinDec, cosRa, cosDec;

        p->ra().SinCos(  sinRa,  cosRa );
        p->dec().SinCos( sinDec, cosDec );
        *x = cosDec * cosRa;
        *y = cosDec * sinRa;
        *z = sinDec;
    }
}

Projector::Projector(const ViewParams& p)
{
    m_data = KStarsData::Instance();
    setViewParams(p);
}

Projector::~Projector()
{

}

void Projector::setViewParams(const ViewParams& p)
{
    m_vp = p;
    m_sinY0 = 0;
    m_cosY0 = 0;
    //Precompute Sin/Cos for focus point
    if( m_vp.useAltAz ) {
        m_vp.focus->alt().SinCos( m_sinY0, m_cosY0 );
    } else {
        m_vp.focus->dec().SinCos( m_sinY0, m_cosY0 );
    }
}

QPointF Projector::toScreen(SkyPoint* o, bool oRefract, bool* onVisibleHemisphere) const
{
    return KSUtils::vecToPoint( toScreenVec(o, oRefract, onVisibleHemisphere) );
}

bool Projector::onScreen(const QPointF& p) const
{
    return (0 <= p.x() && p.x() <= m_vp.width &&
            0 <= p.y() && p.y() <= m_vp.height);
}

bool Projector::onScreen(const Vector2f& p) const
{
    return (0 <= p.x() && p.x() <= m_vp.width &&
            0 <= p.y() && p.y() <= m_vp.height);
}

QPointF Projector::clipLine( SkyPoint *p1, SkyPoint *p2 ) const
{
    return KSUtils::vecToPoint( clipLineVec(p1,p2));
}

Vector2f Projector::clipLineVec( SkyPoint *p1, SkyPoint *p2 ) const
{
    /* ASSUMES p1 was not clipped but p2 was.
     * Return the QPoint that barely clips in the line twixt p1 and p2.
     */
    //TODO: iteration = ceil( 0.5*log2( w^2 + h^2) )??
    //      also possibly rewrite this
    //     --hdevalence
    int iteration = 15;          // For "perfect" clipping:
    // 2^interations should be >= max pixels/line
    bool isVisible = true;       // so we start at midpoint
    SkyPoint mid;
    Vector2f oMid;
    double x, y, z, dx, dy, dz, ra, dec;
    int newx, newy, oldx, oldy;
    oldx = oldy = -10000;        // any old value that is not the first omid

    toXYZ( p1, &x, &y, &z );
    // -jbb printf("\np1: %6.4f %6.4f %6.4f\n", x, y, z);

    toXYZ( p2, &dx, &dy, &dz );

    // -jbb printf("p2: %6.4f %6.4f %6.4f\n", dx, dy, dz);
    dx -= x;
    dy -= y;
    dz -= z;
    // Successive approximation to point on line that just clips.
    while(iteration-- > 0) {
        dx *= .5;
        dy *= .5;
        dz *= .5;
        if ( ! isVisible ) {              // move back toward visible p1
            x -= dx;
            y -= dy;
            z -= dz;
        }
        else {                        // move out toward clipped p2
            x += dx;
            y += dy;
            z += dz;
        }

        // -jbb printf("  : %6.4f %6.4f %6.4f\n", x, y, z);
        // [x, y, z] => [ra, dec]
        ra = atan2( y, x );
        dec = asin( z / sqrt(x*x + y*y + z*z) );

        mid = SkyPoint( ra * 12. / dms::PI, dec * 180. / dms::PI );
        mid.EquatorialToHorizontal( m_data->lst(), m_data->geo()->lat() );

        oMid = toScreenVec( &mid, false, &isVisible );
        newx = (int) oMid.x();
        newy = (int) oMid.y();

        // -jbb printf("new x/y: %4d %4d", newx, newy);
        if ( (oldx == newx) && (oldy == newy) ) {
            break;
        }
        oldx = newx;
        oldy = newy;
    }
    return  oMid;
}