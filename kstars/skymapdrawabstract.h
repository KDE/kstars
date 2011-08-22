/***************************************************************************
                 skymapdrawabstract.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Dec 20 2010 05:04 AM UTC-6
    copyright            : (C) 2010 Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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

class SkyMapDrawAbstract {

 protected:
    /**
     *@short Virtual Destructor
     */
    virtual ~SkyMapDrawAbstract() { };

 public:

    /**
     *@short Constructor that sets data and m_SkyMap, and initializes
     * the FPS counters.
     */
    SkyMapDrawAbstract( SkyMap *sm );

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
    	*@param pm pointer to the Sky pixmap
    	*/
    void drawOverlays( QPainter& p, bool drawFov = true );

    /**Draw symbols at the position of each Telescope currently being controlled by KStars.
    	*@note The shape of the Telescope symbol is currently a hard-coded bullseye.
    	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap). 
    	*/
    void drawTelescopeSymbols(QPainter &psky);

    /**
    	*@short Draw a dotted-line rectangle which traces the potential new field-of-view in ZoomBox mode.
    	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap). 
    	*/
    void drawZoomBox( QPainter &psky );

    /**Draw a dashed line from the Angular-Ruler start point to the current mouse cursor,
    	*when in Angular-Ruler mode.
    	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap). 
    	*/
    void drawAngleRuler( QPainter &psky );


    /**@short Draw the current Sky map to a pixmap which is to be printed or exported to a file.
    	*
    	*@param pd pointer to the QPaintDevice on which to draw.  
    	*@see KStars::slotExportImage()
    	*@see KStars::slotPrint()
    	*/
    void exportSkyImage( QPaintDevice *pd, bool scale = false );

    /**@short Draw the current Sky map using passed SkyQPainter instance. Required when
      * used QPaintDevice doesn't support drawing using multiple painters (e.g. QSvgGenerator
      * which generates broken SVG output when more than one QPainter subclass is used).
      * Passed painter should already be initialized to draw on selected QPaintDevice subclass
      * using begin() and it won't be ended [end()] by this method.
      *@param painter pointer to the SkyQPainter already set up to paint on selected QPaintDevice subclass.
      *@param scale should sky image be scaled to fit used QPaintDevice?
      */
    void exportSkyImage( SkyQPainter *painter, bool scale = false );

    /**@short Draw "user labels".  User labels are name labels attached to objects manually with
     * the right-click popup menu.  Also adds a label to the FocusObject if the Option UseAutoLabel
     * is true.
     * @param labelObjects QList of pointers to the objects which need labels (excluding the centered object)
     * @param psky painter for the sky
     * @note the labelObjects list is managed by the SkyMapComponents class
     */
    void drawObjectLabels( QList< SkyObject* >& labelObjects );



    // *********************** PURE VIRTUAL METHODS ******************* //
    // NOTE: The following methods differ between GL and QPainter backends
    //       Thus, they are pure virtual and must be implemented by the sublcass


 protected:
    /**
     *@short Overridden paintEvent method. Must be implemented by
     *       subclasses to draw the SkyMap. (This method is pure
     *       virtual)
     */
    virtual void paintEvent( QPaintEvent *e ) = 0;

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

    /** Calculate FPS and dump result to stderr using kDebug */
    void calculateFPS();
private:
    int m_framecount;     // To count FPS
    QTime m_fpstime;
};

#endif
