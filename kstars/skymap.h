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
#include <qpoint.h>
#include <qpixmap.h>

#include "skyobject.h"
#include "starobject.h"
#include "ksplanet.h"
#include "dms.h"
#include "skypoint.h"
#include "starpixmap.h"

#define NZOOM 30

class QPopupMenu;
class QLabel;
class KStars;

/**Class for the sky display, derived from QWidget.  This widget is just a canvas
	*on which the sky is painted.  It's the main widget for KStars.  The widget also
	*handles user interaction (both mouse and keyboard).
	*
	*@short Canvas widget for displaying the sky bitmap; also handles user input.
	*@author Jason Harris
	*@version 0.9
	*/

class SkyMap : public QWidget  {
   Q_OBJECT
public:
/**
	*Constructor.  Read stored settings from KConfig object (focus position,
	*zoom level, sky color, etc.).  Run initPopupMenus().
	*/
	SkyMap(QWidget *parent=0, const char *name=0);
/**
	*Destructor (empty)
	*/
	~SkyMap();

/**@returns a pointer to the central focus point of the sky map
	*/
	SkyPoint* focus() { return &Focus; }

/**@short sets the central focus point of the sky map
	*@param f a pointer to the SkyPoint the map should be centered on
	*/
	void setFocus( SkyPoint *f ) { Focus.set( f->ra(), f->dec() ); }

/**@short sets the central focus point of the sky map, using alt/az coordinates
	*@param alt the new altitude
	*@param az the new azimuth
	*/
	void setFocusAltAz(double alt, double az);

/**@returns a pointer to the destination point of the sky map
	*/
	SkyPoint* destination() { return &Destination; }

/**@short sets the destination point of the sky map
	*@param f a pointer to the SkyPoint the map should slew to
	*/
	void setDestination( SkyPoint *f );

/**@short sets the destination point of the sky map, using alt/az coordinates
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
	*the private member FoundObject.  This function returns the FoundObject pointer.
	*@returns a pointer to the object centered on by the user.
	*/
	SkyObject* foundObject( void ) const { return FoundObject; }

/**Sets the FoundObject pointer to the argument.
	*@param o the SkyObject pointer to be assigned to FoundObject
	*/
	void setFoundObject( SkyObject *o ) { FoundObject = o; }

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

/**pixelScale is referenced one time outside of skymap.  This is the access method
	*for this external reference.
	*@returns current value of pixelScale
	*/
	int getPixelScale( void );

/**@returns estimate of the display's angular field of view
	*/
	float fov( void );

/**
	*Determine if the skypoint p might be visible in the current display window
	*/
	bool checkVisibility( SkyPoint *p, float fov, bool useAltAz, bool isPoleVisible );

/**call keyPressEvent, as if the key given as an argument had been pressed. */
	void invokeKey( int key );

/**Apply the color scheme described by the file given as an argument. */
	bool setColors( QString filename );

/**@returns a pointer to the current sky pixmap. */
	QPixmap skyPixmap() const { return *sky; }

/**@returns whether the map is in slewing mode */
	bool isSlewing() const { return slewing; }

public slots:
	virtual void setGeometry( int x, int y, int w, int h );
	virtual void setGeometry( const QRect &r );

/**Recalculates the positions of objects in the sky, and then repaints the sky map.
	*If the positions don't need to be recalculated, use update() instead of Update().
	*This saves a lot of CPU time.  Note that update() is called automatically when the widget
	*window is brought to the front from behind another window.
	*/	
	void Update();

/**Identical to Update(), except calls repaint() instead of update() in order
	*to have the paintEvent executed immediately (whereas update() simply adds
	*the call to the event queue, so if many calls to Update() are made, some
	*will be dropped.  UpdateNow() avoids this, forcing each paintEvent to execute.
	*/
	void UpdateNow();

/**Estimate the effect of atmospheric refraction.  Refraction only affects the
	*altitude of objects beyond the atmosphere (so it shouldn't be used for the horizon).
	*There are two cases:  the true altitude is known, and the apparent altitude is needed;
	*or the apparent altitude is known and the true altitude is needed.
	*@param alt The input altitude
	*@param findApparent if true, then alt is the true altitude, and we'll find the apparent alt.  Otherwise, vice versa.
	*@returns the corrected altitude.
	*/
	dms refract( dms alt, bool findApparent );

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

/**Popup menu function: Add a custom Image or Information URL. */
	void addLink( void );

/**Checks whether the timestep exceeds a threshhold value.  If so, sets
	*ClockSlewing=true and sets the SimClock to ManualMode. */
	void slotClockSlewing();

signals:
	void destinationChanged();
	
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
/**Initialize the popup menus */
	void initPopupMenu( void );

/**Given the coordinates of the SkyPoint argument, determine the
	*pixel coordinates in the SkyMap.  If Horiz==true, use the SkyPoint's
	*Alt/AZ coordinates; otherwise, use RA/Dec.
	*@returns QPoint containing pixel x, y coordinates of SkyPoint.
	*@param o SkyPoint to calculate x, y for.
	*@param Horiz if true, use Alt/Az coordinates.
	*@param doRefraction if true, correct for atmospheric refraction
	*/
	QPoint getXY( SkyPoint *o, bool Horiz, bool doRefraction=true );

/**Determine RA, Dec coordinates of the pixel at (dx, dy), which are the
	*pixel coordinates measured from the center of the SkyMap bitmap.
	*@param dx horizontal pixel offset from center of SkyMap.
	*@param dy vertical pixel offset from center of SkyMap.
	*@param Horiz if true, the SkyMap is displayed using the Horizontal coordinate system
	*@param LSTh Local sidereal time.
	*@param lat current geographic latitude
	*/	
	SkyPoint dXdYToRaDec( double dx, double dy, bool Horiz, dms LST, dms lat, bool doRefraction=true );

/**Large switch-controlled statement to draw objects on the SkyMap
	*according to their type and catalog.  This is going to be changed
	*So that each SkyObject has its own draw() function, which will be
	*called from SkyMap's paintEvent().	
	*color is only needed by Stars.
	*/
	void drawSymbol( QPainter &p, int type, int x, int y, int size, double e=1.0, int pa=0, QChar color=0 );

/**Determine the on-screen position angle of the SkyObject.  This is the sum
	*of the object's sky position angle (w.r.t. North), and the position angle
	*of "North" at the position of the object.  The latter is determined by
	*constructing a test point with the same RA but a slightly increased Dec
	*as the Object, and calculating the angle of a line connecing the object to
	*its test point. */
	int findPA( SkyObject *o, int x, int y );

/**Draw a planet.  This is an image if the planet image is loaded, the zoomlevel
	*is high enough (but not so high that the image fills the screen), and the
	*user has selected that planet images be shown.  If one of these conditions
	*is false, then a simple circle is drawn instead.  */
	void drawPlanet(QPainter &psky, KSPlanetBase *p, QColor c, int sizemin,
			double mult, int zoommin, int resize_mult = 1);

	void drawBoxes( QPixmap *pm );
	void drawBoxes( void );

/**Sets the shape of the default mouse cursor to a cross.  */
	void setDefaultMouseCursor();

/**Check if the current point on screen is a valid point on the sky. This is needed
  *to avoid a crash of the program if the user clicks on a point outside the sky (the
	*corners of the sky map at the lowest zoom level are the invalid points).  */
	bool unusablePoint (double dx, double dy);

/**Set the text of the Rise time and Set time labels in the popup menu  */
	void setRiseSetLabels( void );

	KStars *ksw;
	QString sURL;
	QPopupMenu *pmenu, *pmStar, *pmSolarSys, *pmMoon, *pmMess, *pmNGC;
	QLabel *pmStarTitle, *pmSolTitle, *pmMoonTitle, *pmMessTitle, *pmMessTitle2, *pmNGCTitle, *pmNGCTitle2;
	QLabel *pmMessType, *pmNGCType, *pmTitle, *pmTitle2, *pmType;
	QLabel *pmRiseTime, *pmSetTime, *pmTransitTime;
	bool mouseButtonDown, midMouseButtonDown;
	bool mouseMoveCursor;		// true if mouseMoveEvent; needed by setMouseMoveCursor
	
	QPixmap *sky;

	SkyPoint  Focus, OldFocus, ClickedPoint, MousePoint, Destination;
	SkyObject *ClickedObject, *FoundObject;

	StarPixmap *starpix;	// the pixmap of the stars
	int idSolInfo, idMessHST, idMoonInfo, idMoonImages, idMessInfo, idNGCHST;
	bool slewing, clockSlewing;
	int pixelScale[NZOOM];
	double Range[NZOOM];
	double RefractCorr1[184], RefractCorr2[184];
	double y0;
	int scrollCount;
	bool computeSkymap;	// if false only old pixmap will repainted with bitBlt(), this saves a lot of cpu usage
//DEBUG
	bool dumpHorizon;
//END_DEBUG

	QPointArray *pts;	// needed in paintEvent() so it should not every event call reallocated (save time)
	SkyPoint *sp;			// see line above
};

#endif
