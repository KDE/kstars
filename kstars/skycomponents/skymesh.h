/***************************************************************************
                          skymesh.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-03
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

#ifndef SKYMESH_H
#define SKYMESH_H


#include <QHash>
#include <QList>
#include <QObject>

#include <QPainter>

#include "../htmesh/HTMesh.h"
#include "../htmesh/MeshIterator.h"
#include "../htmesh/MeshBuffer.h"

#include "typedef.h"

// These defines control the trixel storage.  Separate buffers are available for
// indexing and intersecting.  Currently only one buffer is required.  Multiple
// buffers could be used for intermingling drawing and searching or to have a
// non-precessed aperture for objects that don't precess.  If a buffer number
// is greater than or equal to NUM_BUF a brief error message wil be priinted
// and then KStars will crash.  This is a feature, not a bug.

#define DRAW_BUF        (0)
#define OBJ_NEAREST_BUF (1)
#define IN_CONSTELL_BUF (2)
#define NUM_BUF         (3)

class SkyPoint;
class QPolygonF;

class KStars;
class QPainter;

/*@class SkyMesh
 * Provides an interface to the Hierarchical Triangular Mesh (HTM) library
 * written by A. Szalay, John Doug Reynolds, Jim Gray, and Peter Z. Kunszt.
 *
 * HTM divides the celestial sphere up into little triangles (trixels) and
 * gives each triangle a unique integer id.  We index objects by putting a
 * pointer to the object in a data structure accessible by the id number, for
 * example QList<QList<StarObject*> or QHash<int, QList<LineListComponent*>.
 *
 * Use aperture(centerPoint, radius) to get a list of trixels visible in the
 * circle.  Then use a MeshIterator to iterate over the indices of the visible
 * trixels.  Look up each index in the top level QHash or QList data structure
 * and the union of all the elements of the QList's returned will contain all
 * the objects visible in the circle in the sky specfied in the aperture()
 * call.
 *
 * The drawID is used for extended objects that may be covered by more than
 * one trixel.  Every time aperture() is called, drawID is incremented by one.
 * Extended objects should each contain their own drawID.  If an object's
 * drawID is the same as the current drawID then it shouldn't be drawn again
 * because it was already drawn in this draw cycle.  If the drawID is
 * different then it should be drawn and then the drawID should be set to the
 * current drawID() so it doesn't get drawn again this cycle.
 *
 * The list of visible trixels found from an aperature() call or one of the
 * primitive index() by creating a new MeshIterator instance:
 *
 *      MeshIterator( HTMesh *mesh )
 *
 * The MeshIterator has its own bool hasNext(), int next(), and int size()
 * methods for iterating through the integer indices of the found trixels or
 * for just getting the total number of found trixels.
 *
 * NOTE: to save space and time, the result set of trixel indices is SHARED.
 * If you perform a new aperture() or index*() call, the previous result set
 * will get overwritten even if there is an outstanding MeshIterator.  It
 * would be easy to change the HTMesh class so that each iterator held its own
 * copy of the result set but this would be wasteful of time and space since
 * we typically either use the result set just once, or if we need to use the
 * result set multiple times, there is no intervening index() or aperture()
 * call.  You have been warned.
 */

class SkyMesh : public HTMesh
{
    private:
        unsigned int m_drawID;
        int errLimit;
        int callCnt;
        int m_debug;

        IndexHash indexHash;

    public:

       /* Constructor.  The level indicates how fine a mesh we will use. The
        * number of triangles (trixels) in the mesh will be 8 * 4^level so a
        * mesh of level 5 will have 8 * 4^5 = 8 * 2^10 = 8192 trixels.
        * The size of the triangles are roughly pi / * 2^(level + 1) so
        * a level 5 mesh will have triagles size roughly of .05 radians or 2.8
        * degrees.
        */
        SkyMesh( int level );

       /* @return the current drawID which gets incremented each time aperture()
        * is calld.
        */
        DrawID drawID( ) { return m_drawID; }

       /**@short finds the set of trixels that cover the circular aperture
        * specified after first performing a reverse precession correction on
        * the center so we don't have to re-index objects simply due to
        * precession.  The drawID also gets incremented which is useful for
        * drawing extended objects.  Typically a safety factor of about one
        * degree is added to the radius to account for proper motion,
        * refraction and other imperfections.
        */
        void aperture( SkyPoint *center, double radius, BufNum bufNum=0 );
        
       /* @short returns the index of the trixel containing p.
        */
        Trixel index( SkyPoint *p );

       /* @short finds the indices of the trixels covering the circle
        * specified by center and radius.
        */
        void index( SkyPoint *center, double radius );

       /* @short finds the indices of the trixels covering the line segment
        * connecting p1 and p2.
        */
        void index( SkyPoint* p1, SkyPoint* p2 );

       /* @short finds the indices of the trixels covering the triangle
        * specified by vertices: p1, p2, and p3.
        */
        void index( SkyPoint* p1, SkyPoint* p2, SkyPoint* p3 );

        void index( const QPointF &p1, const QPointF &p2, const QPointF &p3 );


       /* @short finds the indices of the trixels covering the quadralateral 
        * specified by the vertices: p1, p2, p3, and p4.
        */
        void index( SkyPoint* p1, SkyPoint* p2, SkyPoint* p3, SkyPoint* p4 );

        void index( const QPointF &p1, const QPointF &p2, const QPointF &p3, const QPointF &p4 );

       /* @short fills a QHash with the trixel indices needed to cover all
        * the line segments specified in the QVector<SkyPoints*> points.
        *
        * @param points the points of the line segment.
        * @debug causes extra info to be printed.
        */
        const IndexHash& indexLine( SkyList* points, int debug=0 );

       /* @short as above but allows for skipping the indexing of some of
        * the points. 
        *
        * @param points the line segment to be indexed.
        * @param indexHash the hash to return the indices in.
        * @param skip a hash indicating which points to skip
        * @param debug optional flag to have the routine print out
        * debugging info.
        */
        const IndexHash& indexLine( SkyList* points, IndexHash* skip, int debug=0);

        /* @short fills a QHash with the trixel indices needed to cover the
        * polygon specified in the QList<SkyPoints*> points.  There is no
        * version with a skip parameter because it does not make sense to
        * skip some of the edges of a filled polygon.
        *
        * @param points the points of the line segment.
        * @debug causes extra info to be printed.
        */
        const IndexHash& indexPoly( SkyList* points, int debug=0);

       /* @short does the same as above but with a QPolygonF as the
        * container for the points.
        */
        const IndexHash& indexPoly( const QPolygonF &points, int debug=0);

       /* @short resets the count of calls to indexPoly baco to zero.
        */
        void resetCnt() { callCnt = 0; }

        int debug() { return m_debug; }

        void debug( int debug ) { m_debug = debug; }

        void incDrawID() { m_drawID++; }
        

        void draw( KStars *kstars, QPainter& psky, double scale, BufNum bufNum=0 );

};


#endif
