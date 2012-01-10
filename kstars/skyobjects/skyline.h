/***************************************************************************
                          skyline.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon June 26 2006
    copyright            : (C) 2006 by Jason Harris
    email                : kstarss@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYLINE_H_
#define SKYLINE_H_

#include "skypoint.h"

class dms;
class KStarsData;

/**
 * @class SkyLine
 *
 * A series of connected line segments in the sky, composed of SkyPoints at 
 * its vertices.  SkyLines are used for constellation lines and boundaries, 
 * the coordinate grid, and the equator, ecliptic and horizon.
 *
 * @note the SkyLine segments are always straight lines, they are not 
 * Great Circle segments joining the two endpoints.  Therefore, line segments 
 * that need to follow great circles must be approximated with many short 
 * SkyLine segments.
 */
class SkyLine {
public:
    /** Default Constructor (empty). */
    SkyLine();

    /** Destructor */
    ~SkyLine();

    /**Append a segment to the list by adding a new endpoint.
     * @param p the new endpoint to be added
     */
    void append( SkyPoint *p );

    /**@return a const pointer to a point in the SkyLine
     * param i the index position of the point
     */
    inline SkyPoint* point( int i ) const { return m_pList[i]; }

    inline QList<SkyPoint*>& points() { return m_pList; }

    /** Remove all points from list */
    void clear();

    /**Set point i in the SkyLine
     * @param i the index position of the point to modify
     * @param p1 the new SkyPoint
     */
    void setPoint( int i, SkyPoint *p );

    /**@return the angle subtended by any line segment along the SkyLine.
     * @param i the index of the line segment to be measured.
     * If no argument is given, the first segment is assumed.
     */
    dms angularSize( int i=0 ) const;

    void update( KStarsData *data, KSNumbers *num=0 );

private:
    QList<SkyPoint*> m_pList;
};

#endif
