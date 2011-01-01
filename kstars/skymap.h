/***************************************************************************
                          skymap.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Feb 10 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYMAP_H_
#define SKYMAP_H_

#define USEGL

#include <QTimer>
#include <QGraphicsView>
#include <QPixmap>
#include <QTime>


#include "skyobjects/skypoint.h"
#include "skyobjects/skyline.h"

#include "skymapdrawabstract.h"
#include "skymapqdraw.h"

#include <config-kstars.h>

#define HOVER_INTERVAL 500

class QPainter;
class QPaintDevice;
class QPixmap;

//DEBUG_KIO_JOB
class KJob;

class dms;
class KStarsData;
class KSPopupMenu;
class SkyObject;
class InfoBoxWidget;
class InfoBoxes;
class Projector;

class QGraphicsScene;

#ifdef HAVE_OPENGL
class SkyMapGLDraw;
class SkyMapQDraw;
#endif

/**@class SkyMap
	*
	*This is the canvas on which the sky is painted.  It's the main widget for KStars.
	*Contains SkyPoint members for the map's Focus (current central position), Destination
	*(requested central position), FocusPoint (next queued position to be focused),
	*MousePoint (position of mouse cursor), and ClickedPoint (position of last mouse click).
	*Also contains the InfoBoxes for on-screen data display.
	*
	*SkyMap handles most user interaction events (both mouse and keyboard).
	*
	*@short Canvas widget for displaying the sky bitmap; also handles user interaction events.
	*@author Jason Harris
	*@version 1.0
	*/

class SkyMap : public QGraphicsView {

    Q_OBJECT;
        
    friend class SkyMapDrawAbstract;
    friend class SkyMapQDraw; // FIXME: This is the ugliest thing I've done in my life. Please, please, do something else.

 protected:
    /**
    *Constructor.  Read stored settings from KConfig object (focus position,
    *zoom factor, sky color, etc.).  Run initPopupMenus().
    */
    explicit SkyMap();

 public:
    static SkyMap* Create();

    static SkyMap* Instance();

    static bool IsSlewing() { return pinstance->isSlewing(); }

    /** Destructor (empty) */
    ~SkyMap();

    enum Projection { Lambert,
                      AzimuthalEquidistant,
                      Orthographic,
                      Equirectangular,
                      Stereographic,
                      Gnomonic,
                      UnknownProjection };
		      
    /**@return the angular field of view of the sky map, in degrees. 	 
    *@note it must use either the height or the width of the window to calculate the 	 
    *FOV angle.  It chooses whichever is larger. 	 
    */ 	 
    float fov();

    /**@short Update object name and coordinates in the Focus InfoBox */
    void showFocusCoords();

    /**@short Update the focus position according to current options. */
    void updateFocus();

    /**@short Retrieve the Focus point; the position on the sky at the
    	*center of the skymap.
    	*@return a pointer to the central focus point of the sky map
    	*/
    SkyPoint* focus() { return &Focus; }

    /**@short retrieve the Destination position.
    	*
    	*The Destination is the point on the sky to which the focus will
    	*be moved.  
    	*
    	*@return a pointer to the destination point of the sky map
    	*/
    SkyPoint* destination() { return &Destination; }

    /**@short retrieve the FocusPoint position.
    	*
    	*The FocusPoint stores the position on the sky that is to be 
    	*focused next.  This is not exactly the same as the Destination 
    	*point, because when the Destination is set, it will begin slewing 
    	*immediately.
    	*
    	*@return a pointer to the sky point which is to be focused next.
    	*/
    SkyPoint* focusPoint() { return &FocusPoint; }

    /**@short sets the central focus point of the sky map.
    	*@param f a pointer to the SkyPoint the map should be centered on
    	*/
    void setFocus( SkyPoint *f );

    /**@short sets the focus point of the skymap, using ra/dec coordinates
    	*
    	*@note This function behaves essentially like the above function.  
    	*It differs only in the data types of its arguments.
    	*
    	*@param ra the new right ascension
    	*@param dec the new declination
    	*/
    void setFocus( const dms &ra, const dms &dec );

    /**@short sets the focus point of the sky map, using its alt/az coordinates
    	*@param alt the new altitude
    	*@param az the new azimuth
    	*/
    void setFocusAltAz( const dms &alt, const dms & az);

    /**@short sets the destination point of the sky map.
    	*@note setDestination() emits the destinationChanged() SIGNAL,
    	*which triggers the SLOT function SkyMap::slewFocus().  This
    	*function iteratively steps the Focus point toward Destination, 
    	*repainting the sky at each step (if Options::useAnimatedSlewing()==true).
    	*@param f a pointer to the SkyPoint the map should slew to
    	*/
    void setDestination( SkyPoint *f );

    /**@short sets the destination point of the skymap, using ra/dec coordinates.
    	*
    	*@note This function behaves essentially like the above function.  
    	*It differs only in the data types of its arguments.
    	*
    	*@param ra the new right ascension
    	*@param dec the new declination
    	*/
    void setDestination( const dms &ra, const dms &dec );

    /**@short sets the destination point of the sky map, using its alt/az coordinates.
    	*@param alt the new altitude
    	*@param az the new azimuth
    	*/
    void setDestinationAltAz( const dms &alt, const dms & az);

    /**@short set the FocusPoint; the position that is to be the next Destination.
    	*@param f a pointer to the FocusPoint SkyPoint.
    	*/
    void setFocusPoint( SkyPoint *f ) { if ( f ) FocusPoint = *f; }

    /**@short Retrieve the ClickedPoint position.
    	*
    	*When the user clicks on a point in the sky map, the sky coordinates of the mouse
    	*cursor are stored in the private member ClickedPoint.  This function retrieves
    	*a pointer to ClickedPoint.
    	*@return a pointer to ClickedPoint, the sky coordinates where the user clicked.
    	*/
    SkyPoint* clickedPoint() { return &ClickedPoint; }

    /**@short Set the ClickedPoint to the skypoint given as an argument.
    	*@param f pointer to the new ClickedPoint.
    	*/
    void setClickedPoint( SkyPoint *f );

    /**@short Retrieve a pointer to MousePoint, the sky coordinates of the mouse cursor.
    	*
    	*When the user moves the mouse in the sky map, the sky coordinates of the mouse
    	*cursor are continually stored in MousePoint by the function mouseMoveEvent().  
    	*@return a pointer to MousePoint, the current sky coordinates of the mouse cursor.
    	*/
    SkyPoint* mousePoint() { return &MousePoint; }

    /**@short Set the MousePoint to the skypoint given as an argument.
    	*@note In this function, the argument is a SkyPoint, not a pointer to a SkyPoint.
    	*This is because setMousePoint always uses the function dXdYToRaDec() for the 
    	*argument, and this function returns by value.
    	*@param f the new MousePoint (typically the output of dXdYToRaDec()).
    	*/
    void setMousePoint( SkyPoint f ) { MousePoint = f; }

    /**@short Retrieve the object nearest to a mouse click event.
    	*
    	*If the user clicks on the sky map, a pointer to the nearest SkyObject is stored in
    	*the private member ClickedObject.  This function returns the ClickedObject pointer,
    	*or NULL if there is no CLickedObject.
    	*@return a pointer to the object nearest to a user mouse click.
    	*/
    SkyObject* clickedObject() const { return ClickedObject; }

    /**@short Set the ClickedObject pointer to the argument.
    	*@param o pointer to the SkyObject to be assigned as the ClickedObject
    	*/
    void setClickedObject( SkyObject *o );

    /**@short Retrieve the object which is centered in the sky map.
    	*
    	*If the user centers the sky map on an object (by double-clicking or using the
    	*Find Object dialog), a pointer to the "focused" object is stored in
    	*the private member FocusObject.  This function returns a pointer to the 
    	*FocusObject, or NULL if there is not FocusObject.
    	*@return a pointer to the object at the center of the sky map.
    	*/
    SkyObject* focusObject() const { return FocusObject; }

    /**@short Set the FocusObject pointer to the argument.
    	*@param o pointer to the SkyObject to be assigned as the FocusObject
    	*/
    void setFocusObject( SkyObject *o );

    /**@short Retrieve the object nearest to the point at which the mouse has hovered.
    	*
    	*When the mouse hovers near an object, it is set as the TransientObject (so named
    	*because a transient name label will be attached to it).  This function returns 
    	*a pointer to the current TransientObject, or NULL if no TransientObject is set.
    	*@return pointer to the SkyObject nearest to the mouse hover position.
    	*@see SkyMap::slotTransientLabel()
    	*/
    SkyObject* transientObject() const { return TransientObject; }

    /**@short Set the TransientObject pointer to the argument.
    	*@param o pointer to the SkyObject to be assigned as the TransientObject.
    	*/
    void setTransientObject( SkyObject *o ) { TransientObject = o; }

    /** @short Call to set up the projector before a draw cycle.
      */
    void setupProjector();

    /** Set zoom factor. */
    void setZoomFactor(double factor);

    bool isSlewing() const;

    bool isPointNull( const QPointF &p );

    // NOTE: This method is draw-backend independent.
    /**@short update the geometry of the angle ruler. */
    void updateAngleRuler();

    /**@return true if the object currently has a user label attached.
    	*@note this function only checks for a label explicitly added to the object
    	*with the right-click popup menu; other kinds of labels are not detected by
    	*this function.
    	*@param o pointer to the sky object to be tested for a User label.
    	*/
    bool isObjectLabeled( SkyObject *o );

    /*@*@short Convenience function for shutting off tracking mode.  Just calls KStars::slotTrack().
    	*/
    void stopTracking();

    /** Get the current projector.
        @return a pointer to the current projector. */
    const Projector * projector() const;

    // NOTE: These dynamic casts must not segfault. If they do, it's good because we know that there is a problem.
    /**
     *@short Proxy method for SkyMapDrawAbstract::exportSkyImage()
     */
    inline void exportSkyImage( QPaintDevice *pd ) { dynamic_cast<SkyMapDrawAbstract *>(m_SkyMapDraw)->exportSkyImage( pd ); }

    /**
     *@short Proxy method for SkyMapDrawAbstract::drawObjectLabels()
     */
    inline void drawObjectLabels( QList< SkyObject* >& labelObjects ) { dynamic_cast<SkyMapDrawAbstract *>(m_SkyMapDraw)->drawObjectLabels( labelObjects ); }



public slots:
    //DEBUG_KIO_JOB
    void slotJobResult( KJob *j );

    /**@short This overloaded function is used internally to resize the Sky pixmap to match the window size. */
    virtual void setGeometry( int x, int y, int w, int h );

    /**@short This overloaded function is used internally to resize the Sky pixmap to match the window size.
     * This function behaves essentially like the above function.  It differs only in the data types 	*of its arguments.
     */
    virtual void setGeometry( const QRect &r );

    /**Recalculates the positions of objects in the sky, and then repaints the sky map.
     * If the positions don't need to be recalculated, use update() instead of forceUpdate().
     * This saves a lot of CPU time.
     * @param now if true, paintEvent() is run immediately.  Otherwise, it is added to the event queue
     */
    void forceUpdate( bool now=false );

    /**@short Convenience function; simply calls forceUpdate(true).
     * @see forceUpdate()
     */
    void forceUpdateNow() { forceUpdate( true ); }

    /** Toggle visibility of geo infobox */
    void slotToggleGeoBox(bool);

    /** Toggle visibility of focus infobox */
    void slotToggleFocusBox(bool);

    /** Toggle visibility of time infobox */
    void slotToggleTimeBox(bool);

    /** Toggle visibility of all infoboxes */
    void slotToggleInfoboxes(bool);

    /**Step the Focus point toward the Destination point.  Do this iteratively, redrawing the Sky
     * Map after each step, until the Focus point is within 1 step of the Destination point.
     * For the final step, snap directly to Destination, and redraw the map.
     */
    void slewFocus();

    /**@short Center the display at the point ClickedPoint.
     * 
     * The essential part of the function is to simply set the Destination point, which will emit 
     * the destinationChanged() SIGNAL, which triggers the slewFocus() SLOT.  Additionally, this
     * function performs some bookkeeping tasks, such updating whether we are tracking the new 
     * object/position, adding a Planet Trail if required, etc.
     * 
     * @see destinationChanged()
     * @see slewFocus()
     */
    void slotCenter();

    /**@short Popup menu function: Display 1st-Generation DSS image with the Image Viewer.
     * @note the URL is generated using the coordinates of ClickedPoint.
     */
    void slotDSS();

    /**@short Popup menu function: Display Sloan Digital Sky Survey image with the Image Viewer.
     * @note the URL is generated using the coordinates of ClickedPoint.
     */
    void slotSDSS();

    /**@short Popup menu function: Show webpage about ClickedObject
     * (only available for some objects). 
     */
    void slotInfo();

    /**@short Popup menu function: Show image of ClickedObject
     * (only available for some objects). 
     */
    void slotImage();

    /**@short Popup menu function: Show the Detailed Information window for ClickedObject. */
    void slotDetail();

    /**Add ClickedObject to KStarsData::ObjLabelList, which stores pointers to SkyObjects which
     * have User Labels attached.
     */
    void slotAddObjectLabel();

    /**Remove ClickedObject from KStarsData::ObjLabelList, which stores pointers to SkyObjects which
     * have User Labels attached.
     */
    void slotRemoveObjectLabel();

    /**@short Add a Planet Trail to ClickedObject.
     * @note Trails are added simply by calling KSPlanetBase::addToTrail() to add the first point.
     * as long as the trail is not empty, new points will be automatically appended to it.
     * @note if ClickedObject is not a Solar System body, this function does nothing.
     * @see KSPlanetBase::addToTrail()
     */
    void slotAddPlanetTrail();

    /**@short Remove the PlanetTrail from ClickedObject.
     * @note The Trail is removed by simply calling KSPlanetBase::clearTrail().  As long as
     * the trail is empty, no new points will be automatically appended.
     * @see KSPlanetBase::clearTrail()
     */
    void slotRemovePlanetTrail();

    /**Popup menu function: Add a custom Image or Information URL.
     * Opens the AddLinkDialog window.
     */
    void addLink();

    /**Checks whether the timestep exceeds a threshold value.  If so, sets
     * ClockSlewing=true and sets the SimClock to ManualMode. 
     */
    void slotClockSlewing();

    // NOTE: This method is draw-backend independent.
    /**Enables the angular distance measuring mode. It saves the first
     * position of the ruler in a SkyPoint. It makes difference between
     * having clicked on the skymap and not having done so */
    void slotBeginAngularDistance();

    // NOTE: This method is draw-backend independent.
    /**Computes the angular distance, prints the result in the status
     * bar and disables the angular distance measuring mode
     * If the user has clicked on the map the status bar shows the 
     * name of the clicked object plus the angular distance. If 
     * the user did not clicked on the map, just pressed ], only 
     * the angular distance is printed */
    void slotEndAngularDistance();

    /**Disables the angular distance measuring mode. Nothing is printed
     * in the status bar */
    void slotCancelAngularDistance();

#ifdef HAVE_OPENGL
    void slotToggleGL();
#endif

#ifdef HAVE_XPLANET
    /**Run Xplanet to print a view on the screen*/
    void slotXplanetToScreen();

    /**Run Xplanet to print a view in a file */
    void slotXplanetToFile();
#endif

    /** Zoom in one step. */
    void slotZoomIn();

    /** Zoom out one step. */
    void slotZoomOut();

    /** Set default zoom. */
    void slotZoomDefault();

signals:
    /**Emitted by setDestination(), and connected to slewFocus().  Whenever the Destination
     * point is changed, slewFocus() will iteratively step the Focus toward Destination 
     * until it is reached.
     * @see SkyMap::setDestination()
     * @see SkyMap::slewFocus()
     */
    void destinationChanged();

    /**Emitted by SkyMap::addLink().  This Signal is used to inform the Details Dialog
     * that it needs to update its lists of URL links.
     */
    void linkAdded();

    /** Emitted when zoom level is changed. */
    void zoomChanged();

    /** Emitted when current object changed. */
    void objectChanged(SkyObject*);

    /** Emitted when pointing changed. (At least should) */
    void positionChanged(SkyPoint*);

    /** Emitted when position under mouse changed. */
    void mousePointChanged(SkyPoint*);

protected:
    /**Process keystrokes:
     * @li arrow keys  Slew the map
     * @li +/- keys  Zoom in and out
     * @li <i>Space</i>  Toggle between Horizontal and Equatorial coordinate systems
     * @li 0-9  Go to a major Solar System body (0=Sun; 1-9 are the major planets, except 3=Moon)
     * @li [  Place starting point for measuring an angular distance
     * @li ]  End point for Angular Distance; display measurement.
     * @li <i>Escape</i>  Cancel Angular measurement
     * @li ,/<  Step backward one time step
     * @li ./>  Step forward one time step
     */
    virtual void keyPressEvent( QKeyEvent *e );

    /**When keyRelease is triggered, just set the "slewing" flag to false,
     * and update the display (to draw objects that are hidden when slewing==true). */
    virtual void keyReleaseEvent( QKeyEvent *e );

    /**Determine RA, Dec coordinates of clicked location.  Find the SkyObject
     * which is nearest to the clicked location.
     *
     * If left-clicked: Set set mouseButtonDown==true, slewing==true; display
     * nearest object name in status bar.
     * If right-clicked: display popup menu appropriate for nearest object.
     */
    virtual void mousePressEvent( QMouseEvent *e );

    /**set mouseButtonDown==false, slewing==false */
    virtual void mouseReleaseEvent( QMouseEvent *e );

    /**Center SkyMap at double-clicked location  */
    virtual void mouseDoubleClickEvent( QMouseEvent *e );

    /**This function does several different things depending on the state of the program:
     * @li If Angle-measurement mode is active, update the end-ruler point to the mouse cursor,
     *     and continue this function.
     * @li If we are defining a ZoomBox, update the ZoomBox rectangle, redraw the screen,
     *     and return.
     * @li If dragging the mouse in the map, update focus such that RA, Dec under the mouse
     *     cursor remains constant.
     * @li If just moving the mouse, simply update the curso coordinates in the status bar.
     */
    virtual void mouseMoveEvent( QMouseEvent *e );

    /**Zoom in and out with the mouse wheel. */
    virtual void wheelEvent( QWheelEvent *e );

    /**If the skymap will be resized, the sky must be new computed. So this
     * function calls explicitly new computing of the skymap.
     */
    virtual void resizeEvent( QResizeEvent * );

    /**
     *@short Asks the SkyMapDrawWidget to repaint itself
     */
    virtual void paintEvent( QPaintEvent *e );

private slots:
    /**Gradually fade the Transient Hover Label into the background sky color, and
    	*redraw the screen after each color change.  Once it has faded fully, set the 
    	*TransientObject pointer to NULL to remove the label.
    	*/
    void slotTransientTimeout();

    // NOTE: Akarsh believes that this method is backend-independent, and is pretty confident about that, but he thinks that it really requires a second inspection.
    /**@short attach transient label to object nearest the mouse cursor.
    	*This slot is connected to the timeout() signal of the HoverTimer, which is restarted
    	*in every mouseMoveEvent().  So this slot is executed only if the mouse does not move for 
    	*HOVER_INTERVAL msec.  It points TransientObject at the SkyObject nearest the 
    	*mouse cursor, and the TransientObject is subsequently labeled in paintEvent().
    	*Note that when TransientObject is not NULL, the next mouseMoveEvent() calls 
    	*fadeTransientLabel(), which fades the label color and then sets TransientLabel to NULL.
    	*@sa mouseMoveEvent(), paintEvent(), slotTransientTimeout(), fadeTransientLabel()
    	*/
    void slotTransientLabel();

    /**Set the shape of mouse cursor to a cross with 4 arrows. */
    void setMouseMoveCursor();

private:

    /**@short Begin fading out the name label attached to TransientObject.
    	*
    	*mouseMoveEvent() will call fadeTransientLabel() when TransientObject is not a 
    	*NULL pointer, and the TransientTimer is not already active.  These conditions 
    	*are met when the mouse did not move for HOVER_INTERVAL msec (triggering a 
    	*TransientLabel), but the mouse has since been moved, thus ending the Hover event.  
    	*This function merely starts the TransientTimer, whose timeout SIGNAL is 
    	*connected to the slotTransientTimeout() SLOT, which handles the actual fading 
    	*of the transient label, and eventually resets TransientObject to NULL.
    	*@sa SkyMap::slotTransientLabel(), SkyMap::slotTransientTimeout()
    	*/
    void fadeTransientLabel() { TransientTimer.start( TransientTimeout ); }

    /**@short Sets the shape of the default mouse cursor to a cross.
    	*/
    void setDefaultMouseCursor();

    /**@short Sets the shape of the mouse cursor to a magnifying glass.
    	*/
    void setZoomMouseCursor();

    /** Calculate the zoom factor for the given keyboard modifier
     * @param modifier
     */
    double zoomFactor( const int modifier );

    /** calculate the magnitude factor (1, .5, .2, or .1) for the given
     * keyboard modifier.
     * @param modifier
     */
    double magFactor( const int modifier );

    /** Decrease the magnitude limit by a step size determined by the
     * keyboard modifier.
     * @param modifier
     */
    void decMagLimit( const int modifier );

     /** Increase the magnitude limit by a step size determined by the
     * keyboard modifier.
     * @param modifier
     */
    void incMagLimit( const int modifier );

    /** Convenience routine to either zoom in or incraase mag limit
     * depending on the Alt modifier.  The Shift and Control modiifers
     * will adjust the size of the zoom or the mag step.
     * @param modifier 
     */
    void zoomInOrMagStep( const int modifier );

    /** Convenience routine to either zoom out or decraase mag limit
     * depending on the Alt modifier.  The Shift and Control modiifers
     * will adjust the size of the zoom or the mag step.
     * @param modifier 
     */
    void zoomOutOrMagStep( const int modifier );

#ifdef HAVE_XPLANET
    /**
     * @short Strart xplanet.
     * @param outputFile Output file path.
     */
    void startXplanet ( const QString & outputFile="" );
#endif

    bool mouseButtonDown, midMouseButtonDown;
    bool mouseMoveCursor;  // true if mouseMoveEvent; needed by setMouseMoveCursor
    bool slewing, clockSlewing;
    bool computeSkymap;  //if false only old pixmap will repainted with bitBlt(), this saves a lot of cpu usage
    bool angularDistanceMode;
    int scrollCount;
    double y0;

    double m_Scale;

    KStarsData *data;
    KSPopupMenu *pmenu;
    SkyPoint  Focus, ClickedPoint, FocusPoint, MousePoint, Destination;
    SkyObject *ClickedObject, *FocusObject, *TransientObject;

    Projector *m_proj;

    SkyLine AngularRuler; //The line for measuring angles in the map
    QRect ZoomRect; //The manual-focus circle.

    //data for transient object labels
    QTimer TransientTimer, HoverTimer;
    QColor TransientColor;
    unsigned int TransientTimeout;

    // InfoBoxes. Used in desctructor to save state
    InfoBoxWidget* m_timeBox;
    InfoBoxWidget* m_geoBox;
    InfoBoxWidget* m_objBox;
    InfoBoxes*     m_iboxes;


    QWidget *m_SkyMapDraw; // Can be dynamic_cast<> to SkyMapDrawAbstract

    // NOTE: These are pointers to the individual widgets
    #ifdef HAVE_OPENGL
    SkyMapQDraw *m_SkyMapQDraw;
    SkyMapGLDraw *m_SkyMapGLDraw;
    #endif

    QGraphicsScene *m_SkyScene;

    static SkyMap* pinstance;

};

#endif
