/***************************************************************************
               linelistindex.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-04
    copyright            : (C) 2007 James B. Bowlin
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

#ifndef LINELISTINDEX_H
#define LINELISTINDEX_H

#include <QList>
#include <QHash>

#include "skycomposite.h"
#include "skymesh.h"
#include "typedef.h"

class QPainter;
class KStars;
class LineList;

/* @class LineListIndex
 * Contains almost all the code needed for indexing and drawing and clipping
 * lines and polygons.
 *
 * @author James B. Bowlin @version 0.1
 */

class LineListIndex : public SkyComposite
{
    public:
        /* @short Constructor
         * Simply set the internal skyMesh, parent, and name.
         * @param parent Pointer to the parent SkyComponent object
         * @param mesh Pointer to the universal SkyMesh instance
         * @param name name of the subclass used for debugging
         */
        LineListIndex( SkyComponent *parent, bool (*visibleMethod)(), const char* name="");

        /* @short this constuctor allows for instances (and subclasses) that
         * don't have/need a (*visibleMethod)().
         */
        LineListIndex( SkyComponent *parent, const char* name="" );

        /* @short does nothing.  Can be overridden if needed
         */
        //virtual void update( KStarsData *data, KSNumbers *num );
        virtual void JITupdate( KStarsData *data, LineList* lineList );

        /* @short Returns a QList of LineList objects.
         */
        //LineListList* listList()  { return &m_listList;  }

        /* @short Returns the SkyMesh object.
         */
        SkyMesh* skyMesh() { return  m_skyMesh;   }

        /* @short Returns the Hash of QLists of LineLists that
         * is used for doing the indexing line segments.
         */
        LineListHash* lineIndex() { return m_lineIndex; }

        /* @short Returns the Hash of QLists of LineLists that
         * is used for indexing filled polygons.
         */
        LineListHash* polyIndex() { return m_polyIndex; }

        void reindexLines();

        /* @short Typically called from within a subclasses init() method.
         * Adds the trixels covering the outline of lineList to the lineIndex.
         *
         * @param debug if greater than zero causes the number of trixels found
         * to be printed.
         */
        void appendLine( LineList* lineList, int debug=0 );

        /* @short Typically called from within a subclasses init() method.
         * Adds the trixels covering the full lineList to the polyIndex.
         *
         * @param debug if greater than zero causes the number of trixels found
         * to be printed.
         */
        void appendPoly( LineList* lineList, int debug=0 );

        /* @short a convenience method that adds a lineList to both the lineIndex
         * and the polyIndex.
         */
        void appendBoth( LineList* lineList, int debug=0 );
        
        /* @short Gives the subclasses access to the top of the draw() method.
         * Typically used for setting the QPen, etc. in the QPainter being
         * passed in.  Defaults to setting a thin white pen.
         */
        virtual void preDraw( KStars *ks, QPainter &psky );

        virtual MeshBufNum_t drawBuffer() { return DRAW_BUF; }

        /* @short Returns an IndexHash from the SkyMesh that contains the set of
         * trixels that cover lineList.  Overridden by SkipListIndex so it can
         * pass SkyMesh an IndexHash indicating which line segments should not
         * be indexed @param lineList contains the list of points to be covered.
         */
        virtual const IndexHash& getIndexHash(LineList* lineList, int debug);

        /* @short Also overridden by SkipListIndex.  Controls skipping inside of
         * the draw() routines.  The default behavior is to simply return false
         * but this was moved into the .cpp file to prevent this header file
         * from generating repeated unused parameter warnings.
         */
        virtual bool skipAt( LineList* lineList, int i );

        /* @short.  The top level draw routine.  Draws all the LineLists for any
         * subclass in one fell swoop which minimizes some of the loop overhead.
         * Overridden by MilkWay so it can decide whether to draw outlines or
         * filled.  Therefore MilkyWay does not need to override preDraw().  The
         * MilkyWay draw() routine calls all of the more specific draw()
         * routines below.
         */
        virtual void draw( KStars *ks, QPainter &psky, double scale );

        void drawAllLinesInt( KStars *ks, QPainter &psky, double scale );

        void drawAllLinesFloat( KStars *ks, QPainter &psky, double scale );

        /* @short Draws all the lines in m_listList as simple lines in integer
         * mode.
         */
        void drawLinesInt( KStars *ks, QPainter &psky, double scale );

        /* @short Draws all the lines in m_listList as simple lines in float
         * mode.
         */
        void drawLinesFloat( KStars *ks, QPainter &psky, double scale );

        /* @short Draws all the lines in m_listList as filled polygons in
         * integer mode.
         */
        void drawFilledInt(KStars *ks, QPainter& psky, double scale);

        /* @short Draws all the lines in m_listList as filled polygons in float
         * mode.
         */
        void drawFilledFloat(KStars *ks, QPainter& psky, double scale);

        /* @short Adds a LineList to the end of the internal list of LineLists.
         */
        //inline void append( LineList* lineList) {
        //    m_listList.append( lineList );
        //}

        /* @short the name is used mostly for debugging and is not for public
         * consumption (which is why it is a char* and not a QString).  The name
         * is specied in the constructor so each subclass can provide its own
         * unique identifier.  When the indexLines() or indexPolygons() routines
         * are called with a non-zero debug flag then the name of the subclass
         * will be printed out in addition to the debugging info.
         */
        const char* name() { return m_name; }

        void intro();
        void summary();

    private:
        const char*  m_name;
        int          m_lineIndexCnt;
        int          m_polyIndexCnt;

        //LineListList  m_listList;
        SkyMesh*      m_skyMesh;
        LineListHash* m_lineIndex;
        LineListHash* m_polyIndex;

};

#endif
