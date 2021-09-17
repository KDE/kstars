/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SKYMAPDRAWABSTRACT_H_
#define SKYMAPDRAWABSTRACT_H_

#include "kstarsdata.h"
#include <QPainter>
#include <QPaintEvent>
#include <QPaintDevice>

class SkyMap;
class SkyQPainter;

/**
 *@short This class defines the methods that both rendering engines
 *       (GLPainter and QPainter) must implement. This also allows us to add
 *       more rendering engines if required.
 *@version 1.0
 *@author Akarsh Simha <akarsh.simha@kdemail.net>
 */

// In summary, this is a class created by stealing all the
// drawing-related methods from the old SkyMap class

class SkyMapDrawAbstract
{
  protected:
    /**
         *@short Virtual Destructor
         */
    virtual ~SkyMapDrawAbstract() = default;

  public:
    /**
         *@short Constructor that sets data and m_SkyMap, and initializes
         * the FPS counters.
         */
    explicit SkyMapDrawAbstract(SkyMap *sm);

    // *********************** "IMPURE" VIRTUAL METHODS ******************* //
    // NOTE: The following methods are implemented using native
    //   QPainter in both cases. So it's virtual, but not pure virtual

    /**Draw the overlays on top of the sky map.  These include the infoboxes,
        	*field-of-view indicator, telescope symbols, zoom box and any other
        	*user-interaction graphics.
        	*
        	*The overlays can be updated rapidly, without having to recompute the entire SkyMap.
        	*The stored Sky image is simply blitted onto the SkyMap widget, and then we call
        	*drawOverlays() to refresh the overlays.
        	*@param p pointer to the Sky pixmap
        	*@param drawFov determines if the FOV should be drawn
        	*/
    void drawOverlays(QPainter &p, bool drawFov = true);

    /**Draw symbols at the position of each Telescope currently being controlled by KStars.
        	*@note The shape of the Telescope symbol is currently a hard-coded bullseye.
        	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap).
        	*/
    void drawTelescopeSymbols(QPainter &psky);

    /**Draw FOV of solved image in Ekos Alignment Module
            *@param psky reference to the QPainter on which to draw (this should be the Sky pixmap).
            */
    void drawSolverFOV(QPainter &psky);

    /**
        	*@short Draw a dotted-line rectangle which traces the potential new field-of-view in ZoomBox mode.
        	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap).
        	*/
    void drawZoomBox(QPainter &psky);

    /**Draw a dashed line from the Angular-Ruler start point to the current mouse cursor,
        	*when in Angular-Ruler mode.
        	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap).
        	*/
    void drawAngleRuler(QPainter &psky);

    /** @short Draw the current Sky map to a pixmap which is to be printed or exported to a file.
        	*
        	*@param pd pointer to the QPaintDevice on which to draw.
        	*@param scale defines if the Sky map should be scaled.
        	*@see KStars::slotExportImage()
        	*@see KStars::slotPrint()
        	*/
    void exportSkyImage(QPaintDevice *pd, bool scale = false);

    /** @short Draw the current Sky map using passed SkyQPainter instance. Required when
          * used QPaintDevice doesn't support drawing using multiple painters (e.g. QSvgGenerator
          * which generates broken SVG output when more than one QPainter subclass is used).
          * Passed painter should already be initialized to draw on selected QPaintDevice subclass
          * using begin() and it won't be ended [end()] by this method.
          *@param painter pointer to the SkyQPainter already set up to paint on selected QPaintDevice subclass.
          *@param scale should sky image be scaled to fit used QPaintDevice?
          */
    void exportSkyImage(SkyQPainter *painter, bool scale = false);

    /** @short Draw "user labels".  User labels are name labels attached to objects manually with
         * the right-click popup menu.  Also adds a label to the FocusObject if the Option UseAutoLabel
         * is true.
         * @param labelObjects QList of pointers to the objects which need labels (excluding the centered object)
         * @note the labelObjects list is managed by the SkyMapComponents class
         */
    void drawObjectLabels(QList<SkyObject *> &labelObjects);

    /**
         *@return true if a draw is in progress or is locked, false otherwise. This is just the value of m_DrawLock
         */
    static inline bool drawLock() { return m_DrawLock; }

    /**
         *@short Acquire / release a draw lock. This prevents other drawing from happening
         */
    static void setDrawLock(bool state);

    // *********************** PURE VIRTUAL METHODS ******************* //
    // NOTE: The following methods differ between GL and QPainter backends
    //       Thus, they are pure virtual and must be implemented by the subclass

  protected:
    /**
         *@short Overridden paintEvent method. Must be implemented by
         *       subclasses to draw the SkyMap. (This method is pure
         *       virtual)
         */
    virtual void paintEvent(QPaintEvent *e) = 0;

    /*
         *NOTE:
         *  Depending on whether the implementation of this class is a
         *  GL-backend, it may need to implement these methods:
         *    virtual void resizeGL(int width, int height);
         *    virtual void initializeGL();
         *  So we will not even bother defining them here.
         *  They must be taken care of in the subclasses
         */

    KStarsData *m_KStarsData;
    SkyMap *m_SkyMap;
    static bool m_DrawLock;

    /** Calculate FPS and dump result to stderr using qDebug */
    //void calculateFPS();
  private:
    // int m_framecount;     // To count FPS
    //QTime m_fpstime;
};

#endif
