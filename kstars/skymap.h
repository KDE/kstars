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

#include <qwidget.h>
#include <qpainter.h>
#include <qpaintdevice.h>
#include <qpoint.h>
#include <qpixmap.h>

#include "skyobject.h"
#include "starobject.h"
#include "ksplanetbase.h"
#include "ksplanet.h"
#include "dms.h"
#include "skypoint.h"
#include "starpixmap.h"
#include "kstarsdata.h"

class QLabel;
class KSPopupMenu;
class InfoBoxes;
class KStars;

/**This is the canvas on which the sky is painted.  It's the main widget for KStars.
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
	*@returns pointer to InfoBoxes object.
	*/
		InfoBoxes* infoBoxes() const { return IBoxes; }

/**Display object name and coordinates in the FocusBox
	*/
		void showFocusCoords();

/**@short Update the focus position according to current options.
	*/
	void updateFocus();

/**@returns a pointer to the central focus point of the sky map
	*/
	SkyPoint* focus() { return &Focus; }

/**@returns a pointer to the destination point of the sky map
	*/
	SkyPoint* destination()  { return &Destination; }

/**@short sets the central focus point of the sky map
	*@param f a pointer to the SkyPoint the map should be centered on
	*/
	void setFocus( SkyPoint *f );

/**@short sets the focus point of the skymap, using ra/dec coordinates
	*@param ra the new right ascension
	*@param dec the new declination
	*/
	void setFocus( const dms &ra, const dms &dec );

/**@short sets the focus point of the sky map, using ra/dec coordinates
	*Differs from the above function only in the type of its parameters
	*@param ra the new right ascension
	*@param dec the new declination
	*/
	void setFocus(double ra, double dec);

/**@short sets the focus point of the sky map, using alt/az coordinates
	*@param alt the new altitude
	*@param az the new azimuth
	*/
	void setFocusAltAz( const dms &alt, const dms & az);

/**@short sets the central focus point of the sky map, using alt/az coordinates
	*@param alt the new altitude
	*@param az the new azimuth
	*/
	void setFocusAltAz(double alt, double az);

/**@short sets the destination point of the sky map
	*@param f a pointer to the SkyPoint the map should slew to
	*/
	void setDestination( SkyPoint *f );

/**@short sets the destination point of the skymap, using ra/dec coordinates
	*@param ra the new right ascension
	*@param dec the new declination
	*/
	void setDestination( const dms &ra, const dms &dec );

/**@short sets the destination point of the sky map, using ra/dec coordinates
	*Differs from the above function only in the type of its parameters
	*@param ra the new right ascension
	*@param dec the new declination
	*/
	void setDestination(double ra, double dec);

/**@short sets the destination point of the sky map, using alt/az coordinates
	*@param alt the new altitude
	*@param az the new azimuth
	*/
	void setDestinationAltAz( const dms &alt, const dms & az);

/**@short sets the destination point of the sky map, using alt/az coordinates.
	*Differs from the above function only in the type of its parameters
	*@param alt the new altitude
	*@param az the new azimuth
	*/
	void setDestinationAltAz(double alt, double az);

/**To check how much the focus point has changed, we store the previous
	*focus point.  This function retrieves the SkyPOint of the previous focus.
	*@returns a pointer to the previous central focus point of the sky map
	*/
	SkyPoint* oldfocus() { return &OldFocus; }

/**@short sets the previous central focus point of the sky map
	*@param f a pointer to the SkyPoint the map was centered on
	*/
	void setOldFocus( SkyPoint *f ) { OldFocus.set( f->ra(), f->dec() ); }

/**When the user clicks on a point in the sky map, the sky coordinates of the mouse
	*cursor are stored in the private member ClickedPoint.  This function retrieves
	*a pointer to ClickedPoint.
	*@returns a pointer to ClickedPoint, the sky coordinates where the user clicked.
	*/
	SkyPoint* clickedPoint() { return &ClickedPoint; }

/**Sets the ClickedPoint to the skypoint given as an argument.
	*@param f pointer to the new ClickedPoint.
	*/
	void setClickedPoint( SkyPoint *f ) { ClickedPoint.set( f->ra(), f->dec() ); }

	SkyPoint* focusPoint() { return &FocusPoint; }
	void setFocusPoint( SkyPoint *f ) { if ( f ) FocusPoint.set( f->ra(), f->dec() ); }

	SkyPoint* previousClickedPoint() { return &PreviousClickedPoint; }

/**Sets the ClickedPoint to the skypoint given as an argument.
	*@param f pointer to the new ClickedPoint.
	*/
	void setPreviousClickedPoint( SkyPoint *f ) { PreviousClickedPoint.set( f->ra(), f->dec() ); }

/**When the user moves the mouse in the sky map, the sky coordinates of the mouse
	*cursor are continually passed to the private member MousePoint.  This function retrieves
	*a pointer to MousePoint.
	*@returns a pointer to MousePoint, the current sky coordinates of the mouse cursor.
	*/
	SkyPoint* mousePoint() { return &MousePoint; }

/**Sets the MousePoint to the skypoint given as an argument.
	*Note that in this function, the argument is a SkyPoint, not a pointer to a SkyPoint.
	*@param f the new MousePoint.
	*/
	void setMousePoint( SkyPoint f ) { MousePoint.set( f.ra(), f.dec() ); }

/**If the user clicks on the sky map near an object, a pointer to the object is stored in
	*the private member ClickedObject.  This function returns the ClickedObject pointer.
	*@returns a pointer to the object clicked on by the user.
	*/
	SkyObject* clickedObject( void ) const { return ClickedObject; }

/**Sets the ClickedObject pointer to the argument.
	*@param o the SkyObject pointer to be assigned to ClickedObject
	*/
	void setClickedObject( SkyObject *o ) { ClickedObject = o; }

/**If the user centers the sky map on an object (by double-clicking or using the
	*Find Object dialog), a pointer to the "focused" object is stored in
	*the private member FocusObject.  This function returns the FocusObject pointer.
	*@returns a pointer to the object centered on by the user.
	*/
	SkyObject* focusObject( void ) const { return FocusObject; }

/**Sets the FocusObject pointer to the argument.
	*@param o the SkyObject pointer to be assigned to FocusObject
	*/
	void setFocusObject( SkyObject *o ) { FocusObject = o; }

/**@returns the current setting of the starpix color mode (real colors, solid red, solid
	*white or solid black)
	*/
	int starColorMode( void ) const { return starpix->mode(); }

/**sets the starpix color mode (real colors, solid red, solid
	*white or solid black)
	*/
	void setStarColorMode( int mode ) { starpix->setColorMode( mode ); }

/**@returns the current setting of the starpix color intensity
	*/
	int starColorIntensity( void ) const { return starpix->intensity(); }

/**sets the starpix color intensity value
	*/
	void setStarColorIntensity( int value ) { starpix->setIntensity( value ); }

/**@returns estimate of the display's angular field of view
	*/
	float fov( void );

/**
	*Determine if the skypoint p might be visible in the current display window
	*/
	bool checkVisibility( SkyPoint *p, float fov, double XMax );

/**call keyPressEvent, as if the key given as an argument had been pressed. */
	void invokeKey( int key );

/**Apply the color scheme described by the file given as an argument. */
	bool setColors( QString filename );

/**@returns a pointer to the current sky pixmap. */
	QPixmap skyPixmap() const { return *sky; }

/**@returns whether the map is in slewing mode */
	bool isSlewing() const { return slewing; }

	bool isObjectLabeled( SkyObject *o );

/**@short Convenience function for shutting off tracking mode.  Just calls KStars::slotTrack().
	*/
	void stopTracking();

  //These were protected, but KStars needs them for slotPrint()...
	void drawMilkyWay( QPainter& psky, double scale = 1.0 );
	void drawCoordinateGrid( QPainter& psky, double scale = 1.0 );
	void drawEquator( QPainter& psky, double scale = 1.0 );
	void drawEcliptic( QPainter& psky, double scale = 1.0 );
	void drawConstellationLines( QPainter& psky, double scale = 1.0 );
	void drawConstellationBoundaries( QPainter& psky, double scale = 1.0 );
	void drawConstellationNames( QPainter& psky, QFont& stdFont, double scale = 1.0 );
	void drawStars( QPainter& psky, double scale = 1.0 );
	void drawDeepSkyObjects( QPainter& psky, double scale = 1.0 );
	void drawDeepSkyCatalog( QPainter& psky, QPtrList<DeepSkyObject>& catalog, QColor& color, bool drawObject, bool drawImage, double scale = 1.0 );
	void drawPlanetTrail( QPainter& psky, KSPlanetBase *ksp, double scale = 1.0 );
	void drawSolarSystem( QPainter& psky, bool drawPlanets, double scale = 1.0 );
	void drawHorizon( QPainter& psky, QFont& stdFont, double scale = 1.0 );
	void drawAttachedLabels( QPainter &psky, double scale = 1.0 );
	void drawNameLabel( QPainter &psky, SkyObject *obj, int x, int y, double scale );

	void setMapGeometry( void );
	void exportSkyImage( const QPaintDevice *pd );

public slots:
	virtual void setGeometry( int x, int y, int w, int h );
	virtual void setGeometry( const QRect &r );

/**Recalculates the positions of objects in the sky, and then repaints the sky map.
	*If the positions don't need to be recalculated, use update() instead of forceUpdate().
	*This saves a lot of CPU time.
	*@param now if true, paintEvent() is run immediately.  Otherwise, it is added to the event queue
	*/
	void forceUpdate( bool now=false );
	void forceUpdateNow() { forceUpdate( true ); }

/**Estimate the effect of atmospheric refraction.  Refraction only affects the
	*altitude of objects beyond the atmosphere (so it shouldn't be used for the horizon).
	*There are two cases:  the true altitude is known, and the apparent altitude is needed;
	*or the apparent altitude is known and the true altitude is needed.
	*@param alt The input altitude
	*@param findApparent if true, then alt is the true altitude, and we'll find the apparent alt.  Otherwise, vice versa.
	*@returns the corrected altitude.
	*/
	dms refract( const dms *alt, bool findApparent );

/**Step the Focus point toward the Destination point.  If the Focus is within 1 step of the
	*destination, snap directly to the destination.
	*@param step the size of one step (default is 1.0 degree)
	*/
	void slewFocus( void );

/**Popup menu function: centers display at clicked position. */
	void slotCenter( void );

/**Popup menu function: Display 1st-Generation DSS image with ImageViewer. */
	void slotDSS( void );

/**Popup menu function: Display 2nd-Generation DSS image with ImageViewer. */
	void slotDSS2( void );

/**Popup menu function: Show webpage with konqueror (only available for some objects). */
	void slotInfo( int id );

/**Popup menu function: Display image with ImageViewer (only available for some objects). */
	void slotImage( int id );

/**Popup menu function: Show Detailed Information Dialog. */
	void slotDetail( void );

	void slotAddObjectLabel( void );
	void slotRemoveObjectLabel( void );

	void slotAddPlanetTrail( void );
	void slotRemovePlanetTrail( void );

/**Popup menu function: Add a custom Image or Information URL. */
	void addLink( void );

/**Checks whether the timestep exceeds a threshold value.  If so, sets
	*ClockSlewing=true and sets the SimClock to ManualMode. */
	void slotClockSlewing();
	void slotAngularDistance(void);

signals:
	void destinationChanged();
    void linkAdded();

protected:
/**Draw the Sky, and all objects in it. */
	virtual void paintEvent( QPaintEvent *e );

/**Detect keystrokes: arrow keys, and +/- keys. */
	virtual void keyPressEvent( QKeyEvent *e );

/**When keyRelease is triggered, just set the "slewing" flag to false,
	*and update the display (to draw objects that are hidden when slewing==true). */
	virtual void keyReleaseEvent( QKeyEvent *e );

/**Determine RA, Dec coordinates of clicked location.  Find the SkyObject
	*which is nearest to the clicked location.
	*
	*If left-clicked: Set set mouseButtonDown==true, slewing==true; display nearest object name in status bar.
	*If right-clicked: display popup menu appropriate for nearest object.
	*/
	virtual void mousePressEvent( QMouseEvent *e );

/**set mouseButtonDown==false, slewing==false */
	virtual void mouseReleaseEvent( QMouseEvent *e );

/**Center SkyMap at double-clicked location  */
	virtual void mouseDoubleClickEvent( QMouseEvent *e );

/**If mouseButtonDown==false: display RA, Dec of mouse pointer in status bar.
	*else:  redefine focus such that RA, Dec under mouse cursor remains constant. */
	virtual void mouseMoveEvent( QMouseEvent *e );

/**Zoom in and out with the mouse wheel. */
	virtual void wheelEvent( QWheelEvent *e );

/**If the skymap will be resized, the sky must be new computed. So this function calls explicite new computing of
	*the skymap. */
	virtual void resizeEvent( QResizeEvent * );

private slots:
/**Set the shape of mouse cursor to a cross with 4 arrows. */
	void setMouseMoveCursor();

private:
/**Given the coordinates of the SkyPoint argument, determine the
	*pixel coordinates in the SkyMap.  If Horiz==true, use the SkyPoint's
	*Alt/AZ coordinates; otherwise, use RA/Dec.
	*@returns QPoint containing pixel x, y coordinates of SkyPoint.
	*@param o SkyPoint to calculate x, y for.
	*@param Horiz if true, use Alt/Az coordinates.
	*@param doRefraction if true, correct for atmospheric refraction
	*/
	QPoint getXY( SkyPoint *o, bool Horiz, bool doRefraction=true, double scale = 1.0 );

/**Determine RA, Dec coordinates of the pixel at (dx, dy), which are the
	*pixel coordinates measured from the center of the SkyMap bitmap.
	*@param dx horizontal pixel offset from center of SkyMap.
	*@param dy vertical pixel offset from center of SkyMap.
	*@param Horiz if true, the SkyMap is displayed using the Horizontal coordinate system
	*@param LSTh Local sidereal time.
	*@param lat current geographic laitude
	*/
	SkyPoint dXdYToRaDec( double dx, double dy, bool Horiz, dms *LST, const dms *lat, bool doRefraction=true );

/**Large switch-controlled statement to draw objects on the SkyMap
	*according to their type and catalog.  This is going to be changed
	*So that each SkyObject has its own draw() function, which will be
	*called from SkyMap's paintEvent().
	*color is only needed by Stars.
	*/
	void drawSymbol( QPainter &p, int type, int x, int y, int size, double e=1.0, int pa=0, QChar color=0, double scale = 1.0 );

/**Determine the on-screen position angle of a DeepSkyObject.  This is the sum
	*of the object's sky position angle (w.r.t. North), and the position angle
	*of "North" at the position of the object.  The latter is determined by
	*constructing a test point with the same RA but a slightly increased Dec
	*as the Object, and calculating the angle of a line connecing the object to
	*its test point. */
	int findPA( DeepSkyObject *o, int x, int y );

/**Draw a planet.  This is an image if the planet image is loaded, the zoomlevel
	*is high enough (but not so high that the image fills the screen), and the
	*user has selected that planet images be shown.  If one of these conditions
	*is false, then a simple circle is drawn instead.  */
	void drawPlanet(QPainter &psky, KSPlanetBase *p, QColor c,
			double zoommin, int resize_mult = 1, double scale = 1.0 );

/**Draw overlays on top of the sky map.  These include the infoboxes,
	*field-of-view indicator, telescope symbols, zoom box and any other
	*user-interaction graphics
	*/
	void drawOverlays( QPixmap *pm );

/**Draw the InfoBoxes on the QPainter passed as an argument (this should be
	*the skymap's painter).
	*/
	void drawBoxes( QPainter &p );

/**Draw the Field-of-View indicator.
	*@param psky The QPainter to draw on (this should be skymap's QPainter).
	*@param style The kind of FOV indicator to draw:
	*0: no symbol
	*1: 1-degree circle
	*2: crosshairs
	*3: bullseye (2, 1, 0.5 degrees)
	*4: rectangle (user-defined width, height)
	*/
	void drawTargetSymbol( QPainter &psky );

	void drawTelescopeSymbols(QPainter &psky);
	void drawZoomBox( QPainter &psky);

/**@short Sets the shape of the default mouse cursor to a cross.  */
	void setDefaultMouseCursor();

/**Check if the current point on screen is a valid point on the sky. This is needed
  *to avoid a crash of the program if the user clicks on a point outside the sky (the
	*corners of the sky map at the lowest zoom level are the invalid points).  */
	bool unusablePoint (double dx, double dy);

/**@short convenience function for getting the ZoomFactor from the options object
	*The Zoom Factor is the pixel scale of the display (i.e., the number of pixels which subtend one radian)
	*/
	double zoomFactor() const;

	bool mouseButtonDown, midMouseButtonDown;
	bool mouseMoveCursor;		// true if mouseMoveEvent; needed by setMouseMoveCursor
	bool slewing, clockSlewing;
	bool computeSkymap;	// if false only old pixmap will repainted with bitBlt(), this saves a lot of cpu usage
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

	KStars *ksw;
	KStarsData *data;
	QString sURL;
	KSPopupMenu *pmenu;
	QPixmap *sky;
	InfoBoxes   *IBoxes;
	SkyPoint  Focus, OldFocus, ClickedPoint, FocusPoint, MousePoint, Destination, PreviousClickedPoint;
	SkyObject *ClickedObject, *FocusObject;
	StarPixmap *starpix;	// the pixmap of the stars

	QPointArray *pts;	// needed in paintEvent() so it should not every event call reallocated (save time)
	SkyPoint *sp;			// see line above

	QRect ZoomRect; //The manual-focus circle.
//DEBUG
	bool dumpHorizon;
	bool measuringAngularDistance;
//END_DEBUG
};

#endif
