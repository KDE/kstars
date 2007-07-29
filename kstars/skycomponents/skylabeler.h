/***************************************************************************
                         skylabeler.h  -  K Desktop Planetarium
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

#ifndef SKYLABELER_H
#define SKYLABELER_H

#include <QFontMetricsF>
#include <QList>
#include <QVector>

#include <QFont>

class QString;
class QPointF;

class SkyMap;
class LabelRun;

typedef QList<LabelRun*>    LabelRow;
typedef QVector<LabelRow*>  ScreenRows;

/*@class SkyLabeler
 * The purpose of this class is to prevent labels from overlapping.  We do this
 * by creating a virtual (lower Y-resolution) screen. Each "pixel" of this
 * screen essentially contains a boolean value telling us whether or not there
 * is an existing label covering at least part of that pixel.  Before you draw
 * a label, call mark( QPointF, QString ) of that label.  We will check to see
 * if it would overlap any existing label.  If there is overlap we return false.
 * If there is no overlap then we mark all the pixels that cover the new label
 * and return true.  
 * 
 * Since we need to check for overlap for every label every time it is
 * potentially drawn on the screen, efficiency is essential.  So instead of
 * having a 2-dimensional array of boolean values we use Run Length Encoding
 * and store the virtual array in a QVector of QLists.  Each element of the
 * vector, a LabelRow, corresponds to a horizontal strip of pixels on the actual
 * screen.  How many vertical pixels are in each strip is controlled by
 * m_yDensity.  The higher the density, the fewer vertical pixels per strip and
 * hence a larger number of strips are needed to cover the screen.  As
 * m_yDensity increases so does the density of the strips.
 *
 * The information in the X-dimension is completed run length encoded. A
 * consecutive run of pixels in one strip that are covered by one or more labels
 * is stored in a LabelRun object that merely stores the start pixel and the end
 * pixel.  A LabelRow is a list of LabelRun's stored in ascending order.  This
 * saves a lot of space over an explicit array and it also makes checking for
 * overlaps faster and even makes inserting new overlaps faster on average.
 * 
 * Synopsis:
 *
 *   1) Create a new SkyLabeler
 *
 *   2) every time you want to draw a new screen, reset the labeler.
 *
 *   3) call setFont() for the font of the labels you want to draw.
 *
 *   4) before drawing a label call mark( QPointF, QString) with the position
 *      and text of the label.  If mark() returns true, it is safe to draw
 *      the label and the pixels in the virtual screen that cover the new label
 *      will be marked.  If mark returns false, the label would overlap. No
 *      change is made to the virtual screen.
 *
 * Implications:  This works totally on a first come, first served basis.  So
 * unlike a z-buffer where you draw the most important things last, here you
 * want to draw the most important things first.  Some care needs to be taken
 * with the order in which labels are drawn.  We may, someday, want to give the
 * user the control over the label order/priority.
 *
 * I've created a simple SkyLabel class that holds a QPointF and a QString.
 * These can be used to store labels while drawing objects which will allow us
 * to unlink the time objects are drawn from the time their associated labels
 * are drawn.  An alternative approach would be to simple iterate through the
 * objects twice, once to draw the objects and a second time to draw the labels.
 * We will just have to see which approach is most efficient.
 *
 * Finally, even though this code was written to be very efficient, we might
 * want to take some care in how many labels we throw at it.  Sending it 
 * a large number of overlapping labels can be wasteful. Also, if one type
 * of object floods it with labels early on then there may not be any room
 * left for other types of labels.  Therefore for some types of objects (stars)
 * we may want to have a zoom dependent label threshold magnitude just like
 * we have for drawing the stars themselves.  This would throw few labels
 * at the labeler when we are zoomed at and they would mostly overlap anyway
 * and it would give us more labels when the user is zoomed in and there
 * is more room for them.  The "b" key currently causes the labeler statistics
 * to be printed.  This may be useful in figuring out the best settings for
 * the magnitude limits.  It may even be possible to have KStars do some of
 * this throttling automatically but I haven't really thought about that
 * problem yet.
 * 
 * -- James B. Bowlin  2007-07-22
 */


class SkyLabeler {
    private:
        ScreenRows screenRows;       

        int m_maxX;
        int m_maxY;
        int m_size;
        int m_minDeltaX;

        int m_marks;
        int m_hits;
        int m_misses;
        int m_elements;
        int m_errors;

        qreal m_yDensity;
        qreal m_yScale;

        QFontMetricsF m_fontMetrics;

        void reset( SkyMap* skyMap );

	public:
        /* trivial constructor and destructor.
         */
        SkyLabeler();
        ~SkyLabeler();

        /* @short clears the virtual screen (if needed) and resizes the virtual
         * screen (if needed) to match skyMap.  A font must be specified which
         * is taken to be the average or normal font that will be used.  The
         * size of the horizontal strips will be (slightly) optimized for this
         * font.
         */
        void reset( SkyMap* skyMap, const QFont& font );

       /* @short tells the labeler the font you will be using so it can figure
        * out the height and width of the labels.
        */
        void setFont( const QFont& font );

        /* @short tells the labeler the location and text of a label you want
         * to draw.  Returns true if there is room for the label and returns
         * false otherwise.  If it returns true, the location of the label is
         * marked on the virtual screen so future labels won't overlap it.
         */
        bool mark( const QPointF& p, const QString& text);

        /* @short diagnostic. the *percentage* of pixels that have been filled.
         * Expect return values between 0.0 and 100.0.  A fillRatio above 20
         * is pretty busy and crowded.  I think a fillRatio of about 10 looks
         * good.  The fillRatio will be lowered of the screen is zoomed out
         * so are big blank spaces around the celestial sphere.
         */
        float fillRatio();

        /* @short diagnostic, the number of times mark() returned true divided by
         * the total number of times mark was called multiplied by 100.  Expect
         * return values between 0.0 an 100.  A hit ratio of 100 means no labels
         * would have overlapped.  A ratio of zero means no labels got drawn
         * (which should never happen).  A hitRatio around 50 might be a good
         * target to shoot for.  Expect it to be lower when fully zoomed out and
         * higher when zoomed in.
         */
        float hitRatio();

        /* @short diagnostic, prints some brief statistics to the console.
         * Currently this is connected to the "b" key in SkyMapEvents.
         */
        void printInfo();

        void incDensity();
        void decDensity();

        int hits()  { return m_hits; };
        int marks() { return m_marks; }

};

#endif
