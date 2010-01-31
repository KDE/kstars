/***************************************************************************
                          polylist.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-10
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef POLYLIST_H
#define POLYLIST_H

#include <QHash>
#include <QPolygonF>
#include <QPointF>


/* @class PolyList is almost a clone of LineList but since they have no data
 * members in common, this is a new class instead of a subclass
 *
 *
 * @author James B. Bowlin @version 0.1
 */
class PolyList
{
public:
    /* @short trivial constructor that also sets the name.   It was
     * convenient to specify the name at construction time.
     */
    PolyList( QString name) : m_wrapRA(false) { m_name = name; };

    /* @short returns the QPolygonF that holds the points. */
    const QPolygonF* poly() { return &m_poly; }

    /* @short we need a new append() method to append QPointF's
     * instead of SkyPoints.
     */
    void append( const QPointF &p ) { m_poly.append( p ); }

    /* @short returns the name. */
    const QString& name() { return m_name; }

    bool wrapRA() { return m_wrapRA; }
    
    void setWrapRA( bool wrap ) { m_wrapRA = wrap; }
private:
    QPolygonF m_poly;
    QString   m_name;
    bool      m_wrapRA;
};

#endif
