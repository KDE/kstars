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

#ifndef PROJECTOR_H
#define PROJECTOR_H

#include <Eigen/Core>
USING_PART_OF_NAMESPACE_EIGEN

#include <QPointF>

#include "skyobjects/skypoint.h"
#include "skymap.h"

class KStarsData;

/** This is just a container that holds infromation needed to do projections. */
class ViewParams
{
public:
    float width, height;
    float zoomFactor;
    bool useRefraction, useAltAz;
    SkyPoint *focus;
};

/** This class serves as an interface to handle projections. */
class Projector
{
public:
    /** Constructor.
      * @param p the ViewParams for this projection
      */
    Projector( const ViewParams& p );

    virtual ~Projector();

    /** Update cached values for projector */
    void setViewParams( const ViewParams& p );

    /** Return the type of this projection */
    virtual SkyMap::Projection type() const = 0;

    /**Check if the current point on screen is a valid point on the sky. This is needed
        *to avoid a crash of the program if the user clicks on a point outside the sky (the
        *corners of the sky map at the lowest zoom level are the invalid points).
        *@param p the screen pixel position
        */
    virtual bool unusablePoint( const QPointF &p ) const = 0;
    
    /**Given the coordinates of the SkyPoint argument, determine the
     * pixel coordinates in the SkyMap.
     * @return Vector2f containing screen pixel x, y coordinates of SkyPoint.
     * @param o pointer to the SkyPoint for which to calculate x, y coordinates.
     * @param useRefraction true = use Options::useRefraction() value.
     *   false = do not use refraction.  This argument is only needed
     *   for the Horizon, which should never be refracted.
     * @param onVisibleHemisphere pointer to a bool to indicate whether the point is
     *   on the visible part of the Celestial Sphere.
     */
    virtual Vector2f toScreenVec( SkyPoint *o,
                                  bool oRefract = true,
                                  bool* onVisibleHemisphere = 0) const = 0;

    /** This is exactly the same as toScreenVec but it returns a QPointF.
        It just calls toScreenVec and converts the result.
        @see toScreenVec()
      */
    QPointF toScreen( SkyPoint *o,
                      bool oRefract = true,
                      bool* onVisibleHemisphere = 0) const;

    /**@short Determine RA, Dec coordinates of the pixel at (dx, dy), which are the
     * screen pixel coordinate offsets from the center of the Sky pixmap.
     * @param the screen pixel position to convert
     * @param LST pointer to the local sidereal time, as a dms object.
     * @param lat pointer to the current geographic laitude, as a dms object
     */
    virtual SkyPoint fromScreen( const QPointF &p, dms *LST, const dms *lat ) const = 0;


    /**ASSUMES *p1 did not clip but *p2 did.  Returns the QPointF on the line
     * between *p1 and *p2 that just clips.
     */
    QPointF clipLine( SkyPoint *p1, SkyPoint *p2 ) const;

    /**ASSUMES *p1 did not clip but *p2 did.  Returns the Vector2f on the line
     * between *p1 and *p2 that just clips.
     */
    Vector2f clipLineVec( SkyPoint *p1, SkyPoint *p2 ) const;
protected:
    KStarsData *m_data;
    ViewParams m_vp;
    double m_sinY0, m_cosY0;
};

#endif // PROJECTOR_H
