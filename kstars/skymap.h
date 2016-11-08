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
#include "printing/legend.h"

#include <config-kstars.h>

class QPainter;
class QPaintDevice;
class QPixmap;

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

/** @class SkyMap
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

    Q_OBJECT

    friend class SkyMapDrawAbstract; // FIXME: SkyMapDrawAbstract requires a lot of access to SkyMap
    friend class SkyMapQDraw; // FIXME: SkyMapQDraw requires access to computeSkymap

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

    /** @return the angular field of view of the sky map, in degrees.
    *@note it must use either the height or the width of the window to calculate the
    *FOV angle.  It chooses whichever is larger.
    */
    float fov();

    /** @short Update object name and coordinates in the Focus InfoBox */
    void showFocusCoords();

    /** @short Update the focus position according to current options. */
    void updateFocus();

    /** @short Retrieve the Focus point; the position on the sky at the
        *center of the skymap.
        *@return a pointer to the central focus point of the sky map
        */
    SkyPoint* focus() { return &Focus; }

    /** @short retrieve the Destination position.
        *
        *The Destination is the point on the sky to which the focus will
        *be moved.
        *
        *@return a pointer to the destination point of the sky map
        */
    SkyPoint* destination() { return &Destination; }

    /** @short retrieve the FocusPoint position.
        *
        *The FocusPoint stores the position on the sky that is to be
        *focused next.  This is not exactly the same as the Destination
        *point, because when the Destination is set, it will begin slewing
        *immediately.
        *
        *@return a pointer to the sky point which is to be focused next.
        */
    SkyPoint* focusPoint() { return &FocusPoint; }

    /** @short sets the central focus point of the sky map.
        *@param f a pointer to the SkyPoint the map should be centered on
        */
    void setFocus( SkyPoint *f );

    /** @short sets the focus point of the skymap, using ra/dec coordinates
        *
        *@note This function behaves essentially like the above function.
        *It differs only in the data types of its arguments.
        *
        *@param ra the new right ascension
        *@param dec the new declination
        */
    void setFocus( const dms &ra, const dms &dec );

    /** @short sets the focus point of the sky map, using its alt/az coordinates
        *@param alt the new altitude
        *@param az the new azimuth
        */
    void setFocusAltAz( const dms &alt, const dms & az);

    /** @short sets the destination point of the sky map.
        *@note setDestination() emits the destinationChanged() SIGNAL,
        *which triggers the SLOT function SkyMap::slewFocus().  This
        *function iteratively steps the Focus point toward Destination,
        *repainting the sky at each step (if Options::useAnimatedSlewing()==true).
        *@param f a pointer to the SkyPoint the map should slew to
        */
    void setDestination( const SkyPoint& f );

    /** @short sets the destination point of the skymap, using ra/dec coordinates.
        *
        *@note This function behaves essentially like the above function.
        *It differs only in the data types of its arguments.
        *
        *@param ra the new right ascension
        *@param dec the new declination
        */
    void setDestination( const dms &ra, const dms &dec );

    /** @short sets the destination point of the sky map, using its alt/az coordinates.
        *@param alt the new altitude
        *@param az the new azimuth
        */
    void setDestinationAltAz( const dms &alt, const dms & az);

    /** @short set the FocusPoint; the position that is to be the next Destination.
        *@param f a pointer to the FocusPoint SkyPoint.
        */
    void setFocusPoint( SkyPoint *f ) { if ( f ) FocusPoint = *f; }

    /** @short Retrieve the ClickedPoint position.
        *
        *When the user clicks on a point in the sky map, the sky coordinates of the mouse
        *cursor are stored in the private member ClickedPoint.  This function retrieves
        *a pointer to ClickedPoint.
        *@return a pointer to ClickedPoint, the sky coordinates where the user clicked.
        */
    SkyPoint* clickedPoint() { return &ClickedPoint; }

    /** @short Set the ClickedPoint to the skypoint given as an argument.
        *@param f pointer to the new ClickedPoint.
        */
    void setClickedPoint( SkyPoint *f );

    /** @short Retrieve the object nearest to a mouse click event.
        *
        *If the user clicks on the sky map, a pointer to the nearest SkyObject is stored in
        *the private member ClickedObject.  This function returns the ClickedObject pointer,
        *or NULL if there is no CLickedObject.
        *@return a pointer to the object nearest to a user mouse click.
        */
    SkyObject* clickedObject() const { return ClickedObject; }

    /** @short Set the ClickedObject pointer to the argument.
        *@param o pointer to the SkyObject to be assigned as the ClickedObject
        */
    void setClickedObject( SkyObject *o );

    /** @short Retrieve the object which is centered in the sky map.
        *
        *If the user centers the sky map on an object (by double-clicking or using the
        *Find Object dialog), a pointer to the "focused" object is stored in
        *the private member FocusObject.  This function returns a pointer to the
        *FocusObject, or NULL if there is not FocusObject.
        *@return a pointer to the object at the center of the sky map.
        */
    SkyObject* focusObject() const { return FocusObject; }

    /** @short Set the FocusObject pointer to the argument.
        *@param o pointer to the SkyObject to be assigned as the FocusObject
        */
    void setFocusObject( SkyObject *o );

    /** @short Call to set up the projector before a draw cycle. */
    void setupProjector();

    /** @ Set zoom factor.
      *@param factor zoom factor
      */
    void setZoomFactor(double factor);

    bool isSlewing() const;

    // NOTE: This method is draw-backend independent.
    /** @short update the geometry of the angle ruler. */
    void updateAngleRuler();

    /** @return true if the object currently has a user label attached.
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
    inline const Projector * projector() const { return m_proj; }

    // NOTE: These dynamic casts must not segfault. If they do, it's good because we know that there is a problem.
    /**
     *@short Proxy method for SkyMapDrawAbstract::exportSkyImage()
     */
    inline void exportSkyImage( QPaintDevice *pd, bool scale = false ) { dynamic_cast<SkyMapDrawAbstract *>(m_SkyMapDraw)->exportSkyImage( pd, scale ); }

    inline void exportSkyImage( SkyQPainter *painter, bool scale = false ) { dynamic_cast<SkyMapDrawAbstract *>(m_SkyMapDraw)->exportSkyImage( painter, scale ); }

    SkyMapDrawAbstract* getSkyMapDrawAbstract() { return dynamic_cast<SkyMapDrawAbstract *>(m_SkyMapDraw); }

    /**
     *@short Proxy method for SkyMapDrawAbstract::drawObjectLabels()
     */
    inline void drawObjectLabels( QList< SkyObject* >& labelObjects ) { dynamic_cast<SkyMapDrawAbstract *>(m_SkyMapDraw)->drawObjectLabels( labelObjects ); }

    void setPreviewLegend(bool preview) { m_previewLegend = preview; }

    void setLegend(const Legend &legend) { m_legend = legend; }

    bool isInObjectPointingMode() const { return m_objPointingMode; }

    void setObjectPointingMode(bool enabled) { m_objPointingMode = enabled; }

    void setFovCaptureMode(bool enabled) { m_fovCaptureMode = enabled; }

    bool isInFovCaptureMode() const { return m_fovCaptureMode; }

    SkyPoint getCenterPoint();

public slots:
    /** Recalculates the positions of objects in the sky, and then repaints the sky map.
     * If the positions don't need to be recalculated, use update() instead of forceUpdate().
     * This saves a lot of CPU time.
     * @param now if true, paintEvent() is run immediately.  Otherwise, it is added to the event queue
     */
    void forceUpdate( bool now=false );

    /** @short Convenience function; simply calls forceUpdate(true).
     * @see forceUpdate()
     */
    void forceUpdateNow() { forceUpdate( true ); }

    /**
     * @short Update the focus point and call forceUpdate()
     * @param now is passed on to forceUpdate()
     */
    void slotUpdateSky( bool now );

    /** Toggle visibility of geo infobox */
    void slotToggleGeoBox(bool);

    /** Toggle visibility of focus infobox */
    void slotToggleFocusBox(bool);

    /** Toggle visibility of time infobox */
    void slotToggleTimeBox(bool);

    /** Toggle visibility of all infoboxes */
    void slotToggleInfoboxes(bool);

    /** Step the Focus point toward the Destination point.  Do this iteratively, redrawing the Sky
     * Map after each step, until the Focus point is within 1 step of the Destination point.
     * For the final step, snap directly to Destination, and redraw the map.
     */
    void slewFocus();

    /** @short Center the display at the point ClickedPoint.
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

    /** @short Popup menu function: Display 1st-Generation DSS image with the Image Viewer.
     * @note the URL is generated using the coordinates of ClickedPoint.
     */
    void slotDSS();

    /** @short Popup menu function: Display Sloan Digital Sky Survey image with the Image Viewer.
     * @note the URL is generated using the coordinates of ClickedPoint.
     */
    void slotSDSS();

    /** @short Popup menu function: Show webpage about ClickedObject
     * (only available for some objects).
     */
    void slotInfo();

    /** @short Popup menu function: Show image of ClickedObject
     * (only available for some objects).
     */
    void slotImage();

    /** @short Popup menu function: Show the Detailed Information window for ClickedObject. */
    void slotDetail();

    /** Add ClickedObject to KStarsData::ObjLabelList, which stores pointers to SkyObjects which
     * have User Labels attached.
     */
    void slotAddObjectLabel();

    /** Remove ClickedObject from KStarsData::ObjLabelList, which stores pointers to SkyObjects which
     * have User Labels attached.
     */
    void slotRemoveObjectLabel();

    /** @short Add a Planet Trail to ClickedObject.
     * @note Trails are added simply by calling KSPlanetBase::addToTrail() to add the first point.
     * as long as the trail is not empty, new points will be automatically appended to it.
     * @note if ClickedObject is not a Solar System body, this function does nothing.
     * @see KSPlanetBase::addToTrail()
     */
    void slotAddPlanetTrail();

    /** @short Remove the PlanetTrail from ClickedObject.
     * @note The Trail is removed by simply calling KSPlanetBase::clearTrail().  As long as
     * the trail is empty, no new points will be automatically appended.
     * @see KSPlanetBase::clearTrail()
     */
    void slotRemovePlanetTrail();

    /** Checks whether the timestep exceeds a threshold value.  If so, sets
     * ClockSlewing=true and sets the SimClock to ManualMode.
     */
    void slotClockSlewing();


    /** Enables the angular distance measuring mode. It saves the first
     * position of the ruler in a SkyPoint. It makes difference between
     * having clicked on the skymap and not having done so
     * \note This method is draw-backend independent.
    */
    void slotBeginAngularDistance();

    void slotBeginStarHop(); // TODO: Add docs

    /** Computes the angular distance, prints the result in the status
     * bar and disables the angular distance measuring mode
     * If the user has clicked on the map the status bar shows the
     * name of the clicked object plus the angular distance. If
     * the user did not clicked on the map, just pressed ], only
     * the angular distance is printed
     * \note This method is draw-backend independent.
    */
    void slotEndRulerMode();

    /** Disables the angular distance measuring mode. Nothing is printed
     * in the status bar */
    void slotCancelRulerMode();

    /** @short Open Flag Manager window with clickedObject() RA and Dec entered.
      */
    void slotAddFlag();

    /** @short Open Flag Manager window with selected flag focused and ready to edit.
      *@param flagIdx index of flag to be edited.
      */
    void slotEditFlag( int flagIdx );

    /** @short Delete selected flag.
      *@param flagIdx index of flag to be deleted.
      */
    void slotDeleteFlag( int flagIdx );

#ifdef HAVE_OPENGL
    void slotToggleGL();
#endif

#ifdef HAVE_XPLANET
    /** Run Xplanet to print a view in a Window*/
    void slotXplanetToWindow();

    /** Run Xplanet to print a view in a file */
    void slotXplanetToFile();
#endif

    /** Render eyepiece view */
    void slotEyepieceView();

    /** Zoom in one step. */
    void slotZoomIn();

    /** Zoom out one step. */
    void slotZoomOut();

    /** Set default zoom. */
    void slotZoomDefault();

    /** Object pointing for Printing Wizard done */
    void slotObjectSelected();

    void slotCancelLegendPreviewMode();

    void slotFinishFovCaptureMode();

    void slotCaptureFov();

signals:
    /** Emitted by setDestination(), and connected to slewFocus().  Whenever the Destination
     * point is changed, slewFocus() will iteratively step the Focus toward Destination
     * until it is reached.
     * @see SkyMap::setDestination()
     * @see SkyMap::slewFocus()
     */
    void destinationChanged();

    /** Emitted when zoom level is changed. */
    void zoomChanged();

    /** Emitted when current object changed. */
    void objectChanged(SkyObject*);

    /** Emitted when pointing changed. (At least should) */
    void positionChanged(SkyPoint*);

    /** Emitted when position under mouse changed. */
    void mousePointChanged(SkyPoint*);

    /** Emitted when a position is clicked */
    void positionClicked(SkyPoint*);

protected:
    /** Process keystrokes:
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

    /** When keyRelease is triggered, just set the "slewing" flag to false,
     * and update the display (to draw objects that are hidden when slewing==true). */
    virtual void keyReleaseEvent( QKeyEvent *e );

    /** Determine RA, Dec coordinates of clicked location.  Find the SkyObject
     * which is nearest to the clicked location.
     *
     * If left-clicked: Set set mouseButtonDown==true, slewing==true; display
     * nearest object name in status bar.
     * If right-clicked: display popup menu appropriate for nearest object.
     */
    virtual void mousePressEvent( QMouseEvent *e );

    /** set mouseButtonDown==false, slewing==false */
    virtual void mouseReleaseEvent( QMouseEvent *e );

    /** Center SkyMap at double-clicked location  */
    virtual void mouseDoubleClickEvent( QMouseEvent *e );

    /** This function does several different things depending on the state of the program:
     * @li If Angle-measurement mode is active, update the end-ruler point to the mouse cursor,
     *     and continue this function.
     * @li If we are defining a ZoomBox, update the ZoomBox rectangle, redraw the screen,
     *     and return.
     * @li If dragging the mouse in the map, update focus such that RA, Dec under the mouse
     *     cursor remains constant.
     * @li If just moving the mouse, simply update the curso coordinates in the status bar.
     */
    virtual void mouseMoveEvent( QMouseEvent *e );

    /** Zoom in and out with the mouse wheel. */
    virtual void wheelEvent( QWheelEvent *e );

    /** If the skymap will be resized, the sky must be new computed. So this
     * function calls explicitly new computing of the skymap.
     */
    virtual void resizeEvent( QResizeEvent * );

private slots:
    /** @short display tooltip for object under cursor. It's called by m_HoverTimer.
     *  if mouse didn't moved for last HOVER_INTERVAL milliseconds.
     */
    void slotTransientLabel();

    /** Set the shape of mouse cursor to a cross with 4 arrows. */
    void setMouseMoveCursor();

private:

    /** @short Sets the shape of the default mouse cursor to a cross. */
    void setDefaultMouseCursor();

    /** @short Sets the shape of the mouse cursor to a magnifying glass. */
    void setZoomMouseCursor();

    /** Calculate the zoom factor for the given keyboard modifier
     */
    double zoomFactor( const int modifier );

    /** calculate the magnitude factor (1, .5, .2, or .1) for the given
     * keyboard modifier.
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

    void beginRulerMode( bool starHopRuler ); // TODO: Add docs


#ifdef HAVE_XPLANET
    /**
     * @short Strart xplanet.
     * @param outputFile Output file path.
     */
    void startXplanet ( const QString & outputFile="" );
#endif

    bool mouseButtonDown, midMouseButtonDown;
    // true if mouseMoveEvent; needed by setMouseMoveCursor
    bool mouseMoveCursor;
    bool slewing, clockSlewing;
    //if false only old pixmap will repainted with bitBlt(), this
    // saves a lot of cpu usage
    bool computeSkymap;
    // True if we are either looking for angular distance or star hopping directions
    bool rulerMode;
    // True only if we are looking for star hopping directions. If
    // false while rulerMode is true, it means we are measuring angular
    // distance. FIXME: Find a better way to do this
    bool starHopDefineMode;
    double y0;

    double m_Scale;

    KStarsData *data;
    KSPopupMenu *pmenu;

    /** @short Coordinates of point under cursor. It's update in
     * function mouseMoveEvent
     */
    SkyPoint m_MousePoint;

    SkyPoint  Focus, ClickedPoint, FocusPoint, Destination;
    SkyObject *ClickedObject, *FocusObject;

    Projector *m_proj;

    SkyLine AngularRuler; //The line for measuring angles in the map
    QRect ZoomRect; //The manual-focus circle.

    // Mouse should not move for that interval to display tooltip
    static const int HOVER_INTERVAL = 500;
    // Timer for tooltips
    QTimer m_HoverTimer;

    // InfoBoxes. Used in desctructor to save state
    InfoBoxWidget* m_timeBox;
    InfoBoxWidget* m_geoBox;
    InfoBoxWidget* m_objBox;
    InfoBoxes*     m_iboxes;

    // legend
    bool m_previewLegend;
    Legend m_legend;

    bool m_objPointingMode;
    bool m_fovCaptureMode;

    QWidget *m_SkyMapDraw; // Can be dynamic_cast<> to SkyMapDrawAbstract

    // NOTE: These are pointers to the individual widgets
    #ifdef HAVE_OPENGL
    SkyMapQDraw *m_SkyMapQDraw;
    SkyMapGLDraw *m_SkyMapGLDraw;
    #endif

    QGraphicsScene *m_Scene;

    static SkyMap* pinstance;
    const SkyPoint *m_rulerStartPoint; // Good to keep the original ruler start-point for purposes of dynamic_cast

};

#endif
