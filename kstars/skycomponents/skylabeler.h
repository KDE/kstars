/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skylabel.h"

#include <QFontMetricsF>
#include <QList>
#include <QVector>
#include <QPainter>
#include <QPicture>
#include <QFont>

class QString;
class QPointF;
class SkyMap;
class Projector;
struct LabelRun;

typedef QList<LabelRun *> LabelRow;
typedef QVector<LabelRow *> ScreenRows;

/**
 *@class SkyLabeler
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
 *   3) Either:
 *
 *	 A) Call drawLabel() or drawOffsetLabel(), or
 *
 *	 B) Call addLabel() or addOffsetLabel()
 *
 *  4) Call draw() if addLabel() or addOffsetLabel() were used.
 *
 *
 *
 * SkyLabeler works totally on a first come, first served basis which is almost
 * the direct opposite of a z-buffer where the objects drawn last are most
 * visible.  This is why the addLabel() and draw() routines were created.
 * They allow us to time-shift the drawing of labels and thus gives us control
 * over their priority.  The drawLabel() routines are still available but are
 * not being used.  The addLabel() routines adds a label to a specific buffer.
 * Each type of label has its own buffer which lets us control the font and
 * color as well as the priority.  The priority is now manually set in the
 * draw() routine by adjusting the order in which the various buffers get
 * drawn.
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
 * -- James B. Bowlin  2007-08-02
 */
class SkyLabeler
{
  protected:
    SkyLabeler();
    SkyLabeler(SkyLabeler &skyLabler);

  public:
    enum label_t
    {
        STAR_LABEL,
        ASTEROID_LABEL,
        COMET_LABEL,
        PLANET_LABEL,
        JUPITER_MOON_LABEL,
        SATURN_MOON_LABEL,
        DEEP_SKY_LABEL,
        CONSTEL_NAME_LABEL,
        SATELLITE_LABEL,
        RUDE_LABEL, ///Rude labels block other labels FIXME: find a better solution
        NUM_LABEL_TYPES
    };

    //----- Static Methods ----------------------------------------------//

    static SkyLabeler *Instance();

    /**
         * @short returns the zoom dependent label offset.  This is used in this
         * class and in SkyObject.  It is important that the offsets be the same
         * so highlighted labels are always drawn exactly on top of the normally
         * drawn labels.
         */
    static double ZoomOffset();

    /**
         * @short static version of addLabel() below.
         */
    inline static void AddLabel(SkyObject *obj, label_t type) { pinstance->addLabel(obj, type); }

    //--------------------------------------------------------------------//
    ~SkyLabeler();

    /**
         * @short clears the virtual screen (if needed) and resizes the virtual
         * screen (if needed) to match skyMap.  A font must be specified which
         * is taken to be the average or normal font that will be used.  The
         * size of the horizontal strips will be (slightly) optimized for this
         * font.  We also adjust the font size in psky to smaller fonts if the
         * screen is zoomed out.  You can mimic this setting with the static
         * method SkyLabeler::setZoomFont( psky ).
         */
    void reset(SkyMap *skyMap);

/**
         * @short KStars Lite version of the function above
         */
#ifdef KSTARS_LITE
    void reset();
#endif

    /**
         * @short Draws labels using the given painter
         * @param p the painter to draw labels with
         */
    void draw(QPainter &p);

    //----- Font Setting -----//

    /**
         * @short adjusts the font in psky to be smaller if we are zoomed out.
         */
    void setZoomFont();

    /**
         * @short sets the pen used for drawing labels on the sky.
         */
    void setPen(const QPen &pen);

    /**
         * @short tells the labeler the font you will be using so it can figure
         * out the height and width of the labels.  Also sets this font in the
         * psky since this is almost always what is wanted.
         */
    void setFont(const QFont &font);

    /**
         * @short decreases the size of the font in psky and in the SkyLabeler
         * by the delta points.  Negative deltas will increase the font size.
         */
    void shrinkFont(int delta);

    /**
         * @short sets the font in SkyLabeler and in psky to the font psky
         * had originally when reset() was called.  Used by ConstellationNames.
         */
    void useStdFont();

    /**
         * @short sets the font in SkyLabeler and in psky back to the zoom
         * dependent value that was set in reset().  Also used in
         * ConstellationLines.
         */
    void resetFont();

    /**
         * @short returns the fontMetricsF we have already created.
         */
    QFontMetricsF &fontMetrics() { return m_fontMetrics; }

    //----- Drawing/Adding Labels -----//

    /**
         *@short sets four margins for help in keeping labels entirely on the
         * screen.
         */
    void getMargins(const QString &text, float *left, float *right, float *top, float *bot);

    /**
         * @short Tries to draw the text at the position and angle specified. If
         * the label would overlap an existing label it is not drawn and we
         * return false, otherwise the label is drawn, its position is marked
         * and we return true.
         */
    bool drawGuideLabel(QPointF &o, const QString &text, double angle);

    /**
         * @short Tries to draw a label for an object.
         * @param obj the object to draw the label for
         * @param _p the position of that object
         * @return true if the label was drawn
         * //FIXME: should this just take an object pointer and do its own projection?
         *
         * \p padding_factor is the factor by which the real label size is scaled
         */
    bool drawNameLabel(SkyObject *obj, const QPointF &_p, const qreal padding_factor = 1);

    /**
         *@short draw the object's name label on the map, without checking for
         *overlap with other labels.
         *@param obj reference to the QPainter on which to draw (either the sky pixmap or printer device)
         *@param _p The screen position for the label (in pixels; typically as found by SkyMap::toScreen())
         */
    void drawRudeNameLabel(SkyObject *obj, const QPointF &_p);

    /**
         * @short queues the label in the "type" buffer for later drawing.
         */
    void addLabel(SkyObject *obj, label_t type);

#ifdef KSTARS_LITE
    /**
         * @short queues the label in the "type" buffer for later drawing. Doesn't calculate the position of
         * SkyObject but uses pos as a position of label.
         */
    void addLabel(SkyObject *obj, QPointF pos, label_t type);
#endif
    /**
         *@short draws the labels stored in all the buffers.  You can change the
         * priority by editing the .cpp file and changing the order in which
         * buffers are drawn.  You can also change the fonts and colors there
         * too.
         */
    void drawQueuedLabels();

    /**
         * @short a convenience routine that draws all the labels from a single
         * buffer. Currently this is only called from within draw() above.
         */
    void drawQueuedLabelsType(SkyLabeler::label_t type);

    //----- Marking Regions -----//

    /**
         * @short tells the labeler the location and text of a label you want
         * to draw.  Returns true if there is room for the label and returns
         * false otherwise.  If it returns true, the location of the label is
         * marked on the virtual screen so future labels won't overlap it.
         *
         * It is usually easier to use drawLabel() or drawLabelOffest() instead
         * which both call mark() internally.
         *
         * \p padding_factor is the factor by which the real label size is
         * scaled
         */
    bool markText(const QPointF &p, const QString &text, qreal padding_factor = 1);

    /**
         * @short Works just like markText() above but for an arbitrary
         * rectangular region bounded by top, bot, left, and right.
         */
    bool markRegion(qreal left, qreal right, qreal top, qreal bot);

    //----- Diagnostics and Information -----//

    /**
         * @short diagnostic. the *percentage* of pixels that have been filled.
         * Expect return values between 0.0 and 100.0.  A fillRatio above 20
         * is pretty busy and crowded.  I think a fillRatio of about 10 looks
         * good.  The fillRatio will be lowered of the screen is zoomed out
         * so are big blank spaces around the celestial sphere.
         */
    float fillRatio();

    /**
         * @short diagnostic, the number of times mark() returned true divided by
         * the total number of times mark was called multiplied by 100.  Expect
         * return values between 0.0 an 100.  A hit ratio of 100 means no labels
         * would have overlapped.  A ratio of zero means no labels got drawn
         * (which should never happen).  A hitRatio around 50 might be a good
         * target to shoot for.  Expect it to be lower when fully zoomed out and
         * higher when zoomed in.
         */
    float hitRatio();

    /**
         * @short diagnostic, prints some brief statistics to the console.
         * Currently this is connected to the "b" key in SkyMapEvents.
         */
    void printInfo();

    inline QFont stdFont() { return m_stdFont; }
    inline QFont skyFont() { return m_skyFont; }
#ifdef KSTARS_LITE
    inline QFont drawFont() { return m_drawFont; }
#endif

    int hits() { return m_hits; }
    int marks() { return m_marks; }

  private:
    ScreenRows screenRows;
    int m_maxX { 0 };
    int m_maxY { 0 };
    int m_size { 0 };
    /// When to merge two adjacent regions
    int m_minDeltaX { 30 };
    int m_marks { 0 };
    int m_hits { 0 };
    int m_misses { 0 };
    int m_elements { 0 };
    int m_errors { 0 };
    qreal m_yScale { 0 };
    double m_offset { 0 };
    QFont m_stdFont, m_skyFont;
    QFontMetricsF m_fontMetrics;
//In KStars Lite this font should be used wherever font of m_p was changed or used
#ifdef KSTARS_LITE
    QFont m_drawFont;
#endif
    QPainter m_p;
    QPicture m_picture;
    QVector<LabelList> labelList;
    const Projector *m_proj { nullptr };
    static SkyLabeler *pinstance;
};
