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

#ifndef SKYMAP_H
#define SKYMAP_H

#include <qtimer.h>
#include <qwidget.h>

#include "skypoint.h"
#include "starpixmap.h"

#define HOVER_INTERVAL 500 

class QPainter;
class QPaintDevice;
class QPoint;
class QPointArray;
class QPixmap;

class dms;
class InfoBoxes;
class KStars;
class KStarsData;
class KSPlanetBase;
class KSPopupMenu;
class SkyObject;
class DeepSkyObject;

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

class SkyMap : public QWidget  {
   Q_OBJECT
public:
/**
	*Constructor.  Read stored settings from KConfig object (focus position,
	*zoom factor, sky color, etc.).  Run initPopupMenus().
	*/
	SkyMap( KStarsData *d, QWidget *parent=0, const char *name=0);

/**
	*Destructor (empty)
	*/
	~SkyMap();

/**
	*@return pointer to InfoBoxes object.
	*/
	InfoBoxes* infoBoxes() const { return IBoxes; }

/**@short Update object name and coordinates in the Focus InfoBox
	*/
	void showFocusCoords( bool coordsOnly = false );

/**@short Update the focus position according to current options.
	*/
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

/**@short retrieve the last focus posiiton.
	*
	*We store the previous focus point to determine how much the focus 
	*position has changed.  
	*
	*@return a pointer to the previous central focus point of the sky map
	*/
	SkyPoint* oldfocus() { return &OldFocus; }

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

/**@short sets the focus point of the sky map, using ra/dec coordinates
	*
	*@note This function behaves essentially like the above function.  
	*It differs only in the data types of its arguments.
	*
	*@param ra the new right ascension
	*@param dec the new declination
	*/
	void setFocus(double ra, double dec);

/**@short sets the focus point of the sky map, using its alt/az coordinates
	*@param alt the new altitude
	*@param az the new azimuth
	*/
	void setFocusAltAz( const dms &alt, const dms & az);

/**@short sets the central focus point of the sky map, using alt/az coordinates
	*
	*@note This function behaves essentially like the above function.  
	*It differs only in the data types of its arguments.
	*
	*@param alt the new altitude
	*@param az the new azimuth
	*/
	void setFocusAltAz(double alt, double az);

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

/**@short sets the destination point of the sky map, using ra/dec coordinates
	*
	*@note This function behaves essentially like the above function.  
	*It differs only in the data types of its arguments.
	*
	*@param ra the new right ascension
	*@param dec the new declination
	*/
	void setDestination(double ra, double dec);

/**@short sets the destination point of the sky map, using its alt/az coordinates.
	*@param alt the new altitude
	*@param az the new azimuth
	*/
	void setDestinationAltAz( const dms &alt, const dms & az);

/**@short sets the destination point of the sky map, using its alt/az coordinates.
	*
	*@note This function behaves essentially like the above function.  
	*It differs only in the data types of its arguments.
	*
	*@param alt the new altitude
	*@param az the new azimuth
	*/
	void setDestinationAltAz(double alt, double az);

/**@short set the previous central focus point of the sky map.
	*@param f a pointer to the SkyPoint the map was centered on
	*/
	void setOldFocus( SkyPoint *f ) { OldFocus.set( f->ra(), f->dec() ); }

/**@short set the FocusPoint; the position that is to be the next Destination.
	*@param f a pointer to the FocusPoint SkyPoint.
	*/
	void setFocusPoint( SkyPoint *f ) { if ( f ) FocusPoint.set( f->ra(), f->dec() ); }

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
	void setClickedPoint( SkyPoint *f ) { ClickedPoint.set( f->ra(), f->dec() ); }

/**@short Retrieve the PreviousClickedPoint position.
	*@return a pointer to PreviousClickedPoint, the sky coordinates of the 
	*penultimate mouse click.
	*/
	SkyPoint* previousClickedPoint() { return &PreviousClickedPoint; }

/**Sets the PreviousClickedPoint to the skypoint given as an argument.
	*@param f pointer to the new PreviousClickedPoint.
	*/
	void setPreviousClickedPoint( SkyPoint *f ) { PreviousClickedPoint.set( f->ra(), f->dec() ); }

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
	void setMousePoint( SkyPoint f ) { MousePoint.set( f.ra(), f.dec() ); }

	/**@short Attempt to find a named object near the SkyPoint argument.  
		*
		*There is a object-type preference order for selecting among nearby objects:  
		*objects of a less-preferred type will be selected only if they are twice as close 
		*to the SkyPoint as the nearest object of a more-preferred type.  The order (from 
		*most to least preferred) is:  Solar System, custom object, Messier, 
		*NGC, IC, stars.  If no named object was found within the zoom-dependent maximum 
		*search radius of about 4 pixels, then the function returns a NULL pointer.
		*
		*@note This code used to be in mousePressEvent(), but now we need it in 
		*slotTransientLabel() and other parts of the code as well.
		*@param p pointer to the skypoint around which to search for an object.
		*@return a pointer to the nearest named object to point p, or NULL if 
		*no object was found.
		*/
	SkyObject* objectNearest( SkyPoint *p );

/**@short Retrieve the object nearest to a mouse click event.
	*
	*If the user clicks on the sky map, a pointer to the nearest SkyObject is stored in
	*the private member ClickedObject.  This function returns the ClickedObject pointer,
	*or NULL if there is no CLickedObject.
	*@return a pointer to the object nearest to a user mouse click.
	*/
	SkyObject* clickedObject( void ) const { return ClickedObject; }

/**@short Set the ClickedObject pointer to the argument.
	*@param o pointer to the SkyObject to be assigned as the ClickedObject
	*/
	void setClickedObject( SkyObject *o ) { ClickedObject = o; }

/**@short Retrieve the object which is centered in the sky map.
	*
	*If the user centers the sky map on an object (by double-clicking or using the
	*Find Object dialog), a pointer to the "focused" object is stored in
	*the private member FocusObject.  This function returns a pointer to the 
	*FocusObject, or NULL if there is not FocusObject.
	*@return a pointer to the object at the center of the sky map.
	*/
	SkyObject* focusObject( void ) const { return FocusObject; }

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
	SkyObject* transientObject( void ) const { return TransientObject; }
	
/**@short Set the TransientObject pointer to the argument.
	*@param o pointer to the SkyObject to be assigned as the TransientObject.
	*/
	void setTransientObject( SkyObject *o ) { TransientObject = o; }

/**@return the current setting of the color mode for stars (0=real colors, 
	*1=solid red, 2=solid white or 3=solid black).
	*/
	int starColorMode( void ) const { return starpix->mode(); }

/**@short Set the color mode for stars (0=real colors, 1=solid red, 2=solid
	*white or 3=solid black).
	*/
	void setStarColorMode( int mode ) { starpix->setColorMode( mode ); }

/**@short Retrieve the color-intensity value for stars.
	*
	*When using the "realistic colors" mode for stars, stars are rendered as 
	*white circles with a colored border.  The "color intensity" setting modulates
	*the relative thickness of this colored border, so it effectively adjusts
	*the color-saturation level for star images.
	*@return the current setting of the color intensity setting for stars.
	*/
	int starColorIntensity( void ) const { return starpix->intensity(); }

/**@short Sets the color-intensity value for stars.
	*
	*When using the "realistic colors" mode for stars, stars are rendered as 
	*white circles with a colored border.  The "color intensity" setting modulates
	*the relative thickness of this colored border, so it effectively adjusts
	*the color-saturation level for star images.
	*/
	void setStarColorIntensity( int value ) { starpix->setIntensity( value ); }

/**@short set up variables for the checkVisibility function.
	*
	*checkVisibility() uses some variables to assist it in determining whether points are 
	*on-screen or not.  The values of these variables do not depend on which object is being tested,
	*so we save a lot of time by bringing the code which sets their values outside of checkVisibility()
	*(which must be run for each and every SkyPoint).  setMapGeometry() is called once in paintEvent().
	*The variables set by setMapGeometry are:
	*@li isPoleVisible TRUE if a coordinate Pole is on-screen
	*@li XMax the horizontal center-to-edge angular distance
	*@li guideXMax a version of XMax used for guide lines (same as XMax at low zoom; 2x XMAX otherwise)
	*@li guideFOV similar to guideXMax, but for the vertical direction.
	*@see SkyMap::checkVisibility()
	*@see SkyMap::paintEvent()
	*/
	void setMapGeometry( void );

/**@short Call keyPressEvent, as if the key given as an argument had been pressed. */
	void invokeKey( int key );

/**@return true if the angular distance measuring mode is on 
 */
	bool isAngleMode() const {return angularDistanceMode;}

/**@short update the geometry of the angle ruler
 */
	void updateAngleRuler();


/**@return true if the object currently has a user label attached.
	*@note this function only checks for a label explicitly added to the object
	*with the right-click popup menu; other kinds of labels are not detected by
	*this function.
	*@param o pointer to the sky object to be tested for a User label.
	*/
	bool isObjectLabeled( SkyObject *o );

/**@short Convenience function for shutting off tracking mode.  Just calls KStars::slotTrack().
	*/
	void stopTracking();

/**@short Draw the current Sky map to a pixmap which is to be printed or exported to a file.
	*
	*Each of the draw functions is called, with a value for the Scale parameter computed to fit the 
	*geometry of the QPaintDevice.
	*@param pd pointer to the QPaintDevice on which to draw.  
	*@see KStars::slotExportImage()
	*@see KStars::slotPrint()
	*/
	void exportSkyImage( const QPaintDevice *pd );

public slots:
/**@short This overloaded function is used internally to resize the Sky pixmap to match the window size.
	*/
	virtual void setGeometry( int x, int y, int w, int h );
	
/**@short This overloaded function is used internally to resize the Sky pixmap to match the window size.
	*
	*This function behaves essentially like the above function.  It differs only in the data types 	*of its arguments.
	*/
	virtual void setGeometry( const QRect &r );

/**Recalculates the positions of objects in the sky, and then repaints the sky map.
	*If the positions don't need to be recalculated, use update() instead of forceUpdate().
	*This saves a lot of CPU time.
	*@param now if true, paintEvent() is run immediately.  Otherwise, it is added to the event queue
	*/
	void forceUpdate( bool now=false );

/**@short Convenience function; simply calls forceUpdate(true).
	*@see forceUpdate()
	*/
	void forceUpdateNow() { forceUpdate( true ); }

/**Estimate the effect of atmospheric refraction on object positions.  Refraction  
	*affects only the Altitude angle of objects.  Also, the correction should not be applied 
	*to the horizon, which is not beyond the atmosphere.
	*
	*To estimate refraction, we use a simple analytic equation.  To save time, we store
	*values of the correction for 0.5-degree Altitude intervals.  Individual objects are then 
	*simply assigned the nearest stored value.  The precaclulated values are stored in the 
	*RefractCorr1 and RefractCorr2 arrays, and these are initialized in the SkyMap constructor.
	*
	*There are two cases:  the true altitude is known, and the apparent altitude is needed;
	*or the apparent altitude is known and the true altitude is needed.
	*@param alt The input altitude
	*@param findApparent if TRUE, then alt is the true altitude, and we'll find the apparent alt.
	*@return the corrected altitude, as a dms object.
	*/
	dms refract( const dms *alt, bool findApparent );

/**Step the Focus point toward the Destination point.  Do this iteratively, redrawing the Sky 
	*Map after each step, until the Focus point is within 1 step of the Destination point.
	*For the final step, snap directly to Destination, and redraw the map.
	*/
	void slewFocus( void );

/**@short Center the display at the point ClickedPoint. 
	*
	*The essential part of the function is to simply set the Destination point, which will emit 
	*the destinationChanged() SIGNAL, which triggers the slewFocus() SLOT.  Additionally, this
	*function performs some bookkeeping tasks, such updating whether we are tracking the new 
	*object/position, adding a Planet Trail if required, etc.
	*
	*@see destinationChanged()
	*@see slewFocus()
	*/
	void slotCenter( void );

/**@short Popup menu function: Display 1st-Generation DSS image with the Image Viewer. 
	*@note the URL is generated using the coordinates of ClickedPoint.
	*/
	void slotDSS( void );

/**@short Popup menu function: Display 2nd-Generation DSS image with the Image Viewer. 
	*@note the URL is generated using the coordinates of ClickedPoint.
	*/
	void slotDSS2( void );

/**@short Popup menu function: Show webpage about ClickedObject 
	*(only available for some objects). 
	*@param id the popup-menu ID entry of the selected information page
	*/
	void slotInfo( int id );

/**@short Popup menu function: Show image of ClickedObject 
	*(only available for some objects). 
	*@param id the popup-menu ID entry of the selected image
	*/
	void slotImage( int id );

/**@short Popup menu function: Show the Detailed Information window for ClickedObject. 
	*/
	void slotDetail( void );

/**Add ClickedObject to KStarsData::ObjLabelList, which stores pointers to SkyObjects which 
	*have User Labels attached.
	*/
	void slotAddObjectLabel( void );

/**Remove ClickedObject from KStarsData::ObjLabelList, which stores pointers to SkyObjects which 
	*have User Labels attached.
	*/
	void slotRemoveObjectLabel( void );

/**@short Add a Planet Trail to ClickedObject.
	*@note Trails are added simply by calling KSPlanetBase::addToTrail() to add the first point.
	*as long as the trail is not empty, new points will be automatically appended to it.
	*@note if ClickedObject is not a Solar System body, this function does nothing.
	*@see KSPlanetBase::addToTrail()
	*/
	void slotAddPlanetTrail( void );
	
/**@short Remove the PlanetTrail from ClickedObject.
	*@note The Trail is removed by simply calling KSPlanetBase::clearTrail().  As long as
	*the trail is empty, no new points will be automatically appended.
	*@see KSPlanetBase::clearTrail()
	*/
	void slotRemovePlanetTrail( void );

/**Popup menu function: Add a custom Image or Information URL.  
	*Opens the AddLinkDialog window.
	*/
	void addLink( void );

/**Checks whether the timestep exceeds a threshold value.  If so, sets
	*ClockSlewing=true and sets the SimClock to ManualMode. 
	*/
	void slotClockSlewing();

/**Enables the angular distance measuring mode. It saves the first 
	*position of the ruler in a SkyPoint. It makes difference between
	*having clicked on the skymap and not having done so */
	void slotBeginAngularDistance(void);

/**Computes the angular distance, prints the result in the status 
	*bar and disables the angular distance measuring mode
	*If the user has clicked on the map the status bar shows the 
	*name of the clicked object plus the angular distance. If 
	*the user did not clicked on the map, just pressed ], only 
	*the angular distance is printed */
	void slotEndAngularDistance(void);

/**Disables the angular distance measuring mode. Nothing is printed
	*in the status bar */
	void slotCancelAngularDistance(void);

signals:
/**Emitted by setDestination(), and connected to slewFocus().  Whenever the Destination 
	*point is changed, slewFocus() will iteratively step the Focus toward Destination 
	*until it is reached.
	*@see SkyMap::setDestination()
	*@see SkyMap::slewFocus()
	*/
	void destinationChanged();
	
/**Emitted by SkyMap::addLink().  This Signal is used to inform the Details Dialog 
	*that it needs to update its lists of URL links.
	*/
	void linkAdded();

protected:
/**Draw the Sky, and all objects in it. */
	virtual void paintEvent( QPaintEvent *e );

/**Process keystrokes: 
	*@li arrow keys  Slew the map
	*@li +/- keys  Zoom in and out 
	*@li N/E/S/W keys  Go to the cardinal points on the Horizon
	*@li Z  Go to the Zenith
	*@li <i>Space</i>  Toggle between Horizontal and Equatorial coordinate systems
	*@li 0-9  Go to a major Solar System body (0=Sun; 1-9 are the major planets, except 3=Moon)
	*@li [  Place starting point for measuring an angular distance
	*@li ]  End point for Angular Distance; display measurement.
	*@li <i>Escape</i>  Cancel Angular measurement
	*@li ,/<  Step backward one time step
	*@li ./>  Step forward one time step
	*/
	virtual void keyPressEvent( QKeyEvent *e );

/**When keyRelease is triggered, just set the "slewing" flag to false,
	*and update the display (to draw objects that are hidden when slewing==true). */
	virtual void keyReleaseEvent( QKeyEvent *e );

/**Determine RA, Dec coordinates of clicked location.  Find the SkyObject
	*which is nearest to the clicked location.
	*
	*If left-clicked: Set set mouseButtonDown==true, slewing==true; display 
	*nearest object name in status bar.
	*If right-clicked: display popup menu appropriate for nearest object.
	*/
	virtual void mousePressEvent( QMouseEvent *e );

/**set mouseButtonDown==false, slewing==false */
	virtual void mouseReleaseEvent( QMouseEvent *e );

/**Center SkyMap at double-clicked location  */
	virtual void mouseDoubleClickEvent( QMouseEvent *e );

/**This function does several different things depending on the state of the program:
	*@li If Angle-measurement mode is active, update the end-ruler point to the mouse cursor,
	*and continue this function.
	*@li If we are dragging an InfoBox, simply redraw the screen and return.
	*@li If we are defining a ZoomBox, update the ZoomBox rectangle, redraw the screen, 
	*and return.
	*@li If dragging the mouse in the map, update focus such that RA, Dec under the mouse 
	*cursor remains constant. 
	*@li If just moving the mouse, simply update the curso coordinates in the status bar.
	*/
	virtual void mouseMoveEvent( QMouseEvent *e );

/**Zoom in and out with the mouse wheel. */
	virtual void wheelEvent( QWheelEvent *e );

/**If the skymap will be resized, the sky must be new computed. So this 
	*function calls explicitly new computing of the skymap.
	*It also repositions the InfoBoxes, if they are anchored to a window edge. 
	*/
	virtual void resizeEvent( QResizeEvent * );

private slots:
/**Gradually fade the Transient Hover Label into the background sky color, and
	*redraw the screen after each color change.  Once it has faded fully, set the 
	*TransientObject pointer to NULL to remove the label.
	*/
	void slotTransientTimeout();

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
// Drawing functions.  Each takes a QPainter reference and a scaling factor as arguments.
// The QPainter is usually the Sky pixmap, but it can also be the Export-to-Image pixmap, or the 
// Printer device.  The scaling factors are 1.0 by default (for screen images).  The scale factor
// is used to resize the image to fit the page when printing or exporting to a file.

/**@short Draw the Milky Way contour.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawMilkyWay( QPainter& psky, double scale = 1.0 );
	
/**@short Draw the coordinate system grid lines.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawCoordinateGrid( QPainter& psky, double scale = 1.0 );
	
/**@short Draw the Celestial Equator line.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawEquator( QPainter& psky, double scale = 1.0 );
	
/**@short Draw the Ecliptic line.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawEcliptic( QPainter& psky, double scale = 1.0 );
	
/**@short Draw the Horizon Line, the compass point labels, and the opaque ground.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	
	void drawHorizon( QPainter& psky, double scale = 1.0 );
/**@short Draw the Constellation Lines.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawConstellationLines( QPainter& psky, double scale = 1.0 );
	
/**@short Draw the Constellation Boundaries.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawConstellationBoundaries( QPainter& psky, double scale = 1.0 );
	
/**@short Draw the Constellation Names.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawConstellationNames( QPainter& psky, double scale = 1.0 );
	
/**@short Draw the Stars.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawStars( QPainter& psky, double scale = 1.0 );
	
/**@short Draw the Deep-Sky Objects.  
	*
	*Calls drawDeepSkyCatalog() for each catalog (Messier/NGC/IC/Custom)
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*@see SkyMap::drawDeepSkyCatalog()
	*/
	void drawDeepSkyObjects( QPainter& psky, double scale = 1.0 );
	
/**@short Draw a Deep-Sky Catalog.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param catalog List of pointers to the objects in a particular deep-sky catalog.
	*@param color The color to be used when drawing the symbols for this catalog.
	*@param drawObject if TRUE, the object symbols will be drawn
	*@param drawImage if TRUE, the object images will be drawn
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawDeepSkyCatalog( QPainter& psky, QPtrList<DeepSkyObject>& catalog, QColor& color, bool drawObject, bool drawImage, double scale = 1.0 );
	
/**@short Draw the Planet Trails.
	*
	*"Planet Trails" can be attached to any solar system body; they are lists of SkyPoints
	*tracing the path of a body across the sky.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param ksp pointer to the solar-system bosy whose trail is to be drawn.
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawPlanetTrail( QPainter& psky, KSPlanetBase *ksp, double scale = 1.0 );
	
/**@short Draw all solar system bodies: Sun, Moon, 8 planets, comets, asteroids.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param drawPlanets if FALSE, do nothing
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawSolarSystem( QPainter& psky, bool drawPlanets, double scale = 1.0 );
	
/**@short Draw "User Labels".  User labels are name labels attached to objects manually with 
	*the right-click popup menu.  Also adds a label to the FocusObject if the Option UseAutoLabel
	*is TRUE.
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*/
	void drawAttachedLabels( QPainter &psky, double scale = 1.0 );
	
/**@short Attach a name label to a SkyObject.  This Function is called by the object-specific 
	*draw functions and also by drawAttachedLabels().
	*@param psky reference to the QPainter on which to draw (either the sky pixmap or printer device)
	*@param obj pointer to the SkyObject which is to be labeled.
	*@param x The screen X-coordinate for the label (in pixels; typically as found by SkyMap::getXY())
	*@param y The screen Y-coordinate for the label (in pixels; typically as found by SkyMap::getXY())
	*@param scale the scaling factor.  We use the default value (1.0) everywhere, except when printing.
	*@see SkyMap::drawAttachedLabels()
	*@see SkyMap::getXY()
	*/
	void drawNameLabel( QPainter &psky, SkyObject *obj, int x, int y, double scale );

/**Draw a planet.  This is an image if the planet image is loaded, the zoomlevel
	*is high enough (but not so high that the image fills the screen), and the
	*user has selected that planet images be shown.  If one of these conditions
	*is false, then a simple colored circle is drawn instead.  
	*@param psky reference to the QPainter on which to draw
	*@param p pointer to the KSPlanetBase to be drawn
	*@param c the color of the circle to be used if not plotting an image of the planet
	*@param zoommin the minimum zoomlevel for drawing the planet image
	*@param resize_mult scale factor for angular size of planet.  This is used only for Saturn, 
	*because its image includes the Rings, which are not included in its angular size measurement. 
	*(there's probably a better way to handle this)
	*@param scale the scale factor used for printing the sky image.
	*/
	void drawPlanet(QPainter &psky, KSPlanetBase *p, QColor c,
			double zoommin, int resize_mult = 1, double scale = 1.0 );
	
/**Draw the overlays on top of the sky map.  These include the infoboxes,
	*field-of-view indicator, telescope symbols, zoom box and any other
	*user-interaction graphics.
	*
	*The overlays can be updated rapidly, without having to recompute the entire SkyMap.
	*The stored Sky image is simply blitted onto the SkyMap widget, and then we call
	*drawOverlays() to refresh the overlays.
	*@param pm pointer to the Sky pixmap
	*/
	void drawOverlays( QPixmap *pm );
	
/**Draw the Focus, Geo and Time InfoBoxes.  This is called by drawOverlays().
	*@param p reference to the QPainter on which to draw (this should be the Sky pixmap).
	*@note there is no scale factor because this is only used for drawing onto the screen, not printing.
	*@see SkyMap::drawOverlays()
	*/
	void drawBoxes( QPainter &p );
	
/**Draw symbols at the position of each Telescope currently being controlled by KStars.
	*@note The shape of the Telescope symbol is currently a hard-coded bullseye.
	*@note there is no scale factor because this is only used for drawing onto the screen, not printing.
	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap). 
	*/
	void drawTelescopeSymbols(QPainter &psky);
	
/**@short Draw symbols for objects in the observing list.
	*@param psky reference to the QPainter on which to draw (this should be the sky pixmap)
	*@note there is no scale factor because this is only used for drawing onto the screen, not printing.
	*/
	void drawObservingList( QPainter &psky, double scale = 1.0 );

/**Draw a dotted-line rectangle which traces the potential new field-of-view in ZoomBox mode.
	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap). 
	*@note there is no scale factor because this is only used for drawing onto the screen, not printing.
	*/
	void drawZoomBox( QPainter &psky);
	
/**Draw the current TransientLabel.  TransientLabels are triggered when the mouse 
	*hovers on an object.
	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap). 
	*@note there is no scale factor because this is only used for drawing onto the screen, not printing.
	*@sa SkyMap::slotTransientLabel(), SkyMap::slotTransientTimeout()
	*/
	void drawTransientLabel( QPainter &psky );

/**Draw a dashed line from the Angular-Ruler start point to the current mouse cursor,
	*when in Angular-Ruler mode.
	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap). 
	*@note there is no scale factor because this is only used for drawing onto the screen, not printing.
	*/
	void drawAngleRuler( QPainter &psky );


/**Given the coordinates of the SkyPoint argument, determine the
	*pixel coordinates in the SkyMap.
	*@return QPoint containing screen pixel x, y coordinates of SkyPoint.
	*@param o pointer to the SkyPoint for which to calculate x, y coordinates.
	*@param Horiz if TRUE, use Alt/Az coordinates.
	*@param doRefraction if TRUE, correct for atmospheric refraction
	*@param scale scaling factor (unused?)
	*/
	QPoint getXY( SkyPoint *o, bool Horiz, bool doRefraction=true, double scale = 1.0 );

/**Determine RA, Dec coordinates of the pixel at (dx, dy), which are the
	*screen pixel coordinate offsets from the center of the Sky pixmap.
	*@param dx horizontal pixel offset from center of SkyMap.
	*@param dy vertical pixel offset from center of SkyMap.
	*@param Horiz if TRUE, the SkyMap is displayed using the Horizontal coordinate system
	*@param LSTh pointer to the local sidereal time, as a dms object.
	*@param lat pointer to the current geographic laitude, as a dms object
	*@param doRefraction if TRUE, correct for atmospheric refraction
	*/
	SkyPoint dXdYToRaDec( double dx, double dy, bool Horiz, dms *LST, const dms *lat, bool doRefraction=true );

/**@return the angular field of view of the sky map, in degrees.
	*@note it must use either the height or the width of the window to calculate the 
	*FOV angle.  It chooses whichever is larger.
	*/
	float fov();

/**@short Determine if the skypoint p is likely to be visible in the display 
	*window.
	*
	*checkVisibility() is an optimization function.  It determines whether an object
	*appears within the bounds of the skymap window, and therefore should be drawn.
	*The idea is to save time by skipping objects which are off-screen, so it is 
	*absolutely essential that checkVisibility() is significantly faster than
	*the computations required to draw the object to the screen.
	*
	*The function first checks the difference between the Declination/Altitude
	*coordinate of the Focus position, and that of the point p.  If the absolute 
	*value of this difference is larger than fov, then the function returns FALSE.
	*For most configurations of the sky map window, this simple check is enough to 
	*exclude a large number of objects.
	*
	*Next, it determines if one of the poles of the current Coordinate System 
	*(Equatorial or Horizontal) is currently inside the sky map window.  This is
	*stored in the member variable 'bool SkyMap::isPoleVisible, and is set by the 
	*function SkyMap::setMapGeometry(), which is called by SkyMap::paintEvent().
	*If a Pole is visible, then it will return TRUE immediately.  The idea is that
	*when a pole is on-screen it is computationally expensive to determine whether 
	*a particular position is on-screen or not: for many valid Dec/Alt values, *all* 
	*values of RA/Az will indeed be onscreen, but for other valid Dec/Alt values, 
	*only *most* RA/Az values are onscreen.  It is cheaper to simply accept all 
	*"horizontal" RA/Az values, since we have already determined that they are 
	*on-screen in the "vertical" Dec/Alt coordinate.
	*
	*Finally, if no Pole is onscreen, it checks the difference between the Focus 
	*position's RA/Az coordinate and that of the point p.  If the absolute value of 
	*this difference is larger than XMax, the function returns FALSE.  Otherwise,
	*it returns TRUE.
	
	*@param p pointer to the skypoint to be checked.
	*@param fov the vertical center-to-edge angle of the window, in degrees
	*@param XMax the horizontal center-to-edge angle of the window, in degrees
	*@return true if the point p was found to be inside the Sky map window.
	*@see SkyMap::setMapGeometry()
	*@see SkyMap::fov()
	*/
	bool checkVisibility( SkyPoint *p, float fov, double XMax );

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

/**Determine the on-screen position angle of a SkyObject.  This is the sum
	*of the object's sky position angle (w.r.t. North), and the position angle
	*of "North" at the position of the object (w.r.t. the screen Y-axis).  
	*The latter is determined by constructing a test point with the same RA but 
	*a slightly increased Dec as the object, and calculating the angle w.r.t. the 
	*Y-axis of the line connecing the object to its test point. 
	*/
	double findPA( SkyObject *o, int x, int y, double scale=1.0 );

/**@short Sets the shape of the default mouse cursor to a cross.  
	*/
	void setDefaultMouseCursor();

/**@short Sets the shape of the mouse cursor to a magnifying glass.  
	*/
	void setZoomMouseCursor();

/**Check if the current point on screen is a valid point on the sky. This is needed
	*to avoid a crash of the program if the user clicks on a point outside the sky (the
	*corners of the sky map at the lowest zoom level are the invalid points).  
	*@param dx the screen pixel X-coordinate, relative to the screen center
	*@param dy the screen pixel Y-coordinate, relative to the screen center
	*/
	bool unusablePoint (double dx, double dy);

	bool mouseButtonDown, midMouseButtonDown;
	bool mouseMoveCursor;  // true if mouseMoveEvent; needed by setMouseMoveCursor
	bool slewing, clockSlewing;
	bool computeSkymap;  //if false only old pixmap will repainted with bitBlt(), this saves a lot of cpu usage
	bool angularDistanceMode;
	int idSolInfo, idMessHST, idMoonInfo, idMoonImages, idMessInfo, idNGCHST;
	int scrollCount;
	double Range;
	double RefractCorr1[184], RefractCorr2[184];
	double y0;

	//data for checkVisibility
	bool isPoleVisible;
	int guidemax;
	float guideFOV;
	double XRange, Ymax;
	double guideXRange;

	QString sURL;
	
	KStars *ksw;
	KStarsData *data;
	KSPopupMenu *pmenu;
	QPixmap *sky, *sky2;
	InfoBoxes  *IBoxes;
	SkyPoint  Focus, OldFocus, ClickedPoint, FocusPoint, MousePoint, Destination, PreviousClickedPoint;
	SkyObject *ClickedObject, *FocusObject, *TransientObject;
	StarPixmap *starpix;	// the pixmap of the stars

	QPointArray *pts;	// needed in paintEvent() so it should not every event call reallocated (save time)
	SkyPoint *sp;			// see line above

	QPoint beginRulerPoint, endRulerPoint;  // used in angle mode
	QRect ZoomRect; //The manual-focus circle.

	//data for transient object labels
	QTimer TransientTimer, HoverTimer;
	QColor TransientColor;
	unsigned int TransientTimeout;
};

#endif
