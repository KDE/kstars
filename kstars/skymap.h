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
#include <qpixmap.h>
#include <qpoint.h>
#include <qpixmap.h>

#include "skyobject.h"
#include "starobject.h"
#include "ksplanet.h"
#include "dms.h"
#include "skypoint.h"
#include "starpixmap.h"

class QPopupMenu;
class QLabel;
class KStars;

/**Class for the sky display, derived from QWidget.  This widget is just a canvas
	*on which the sky is painted.  It's the main widget for KStars.  The widget also
	* handles user interaction (both mouse and keyboard).
	*
	*@short Canvas widget for displaying the sky bitmap; also handles user input.
  *@author Jason Harris
	*@version 0.4
  */

class SkyMap : public QWidget  {
   Q_OBJECT
public:
/**
	*Constructor.  Read stored settings from KConfig object (focus position,
	*zoom level, sky color.  Run initPopupMenus().
	*/
	SkyMap(QWidget *parent=0, const char *name=0);
/**
	*Destructor (empty)
	*/
	~SkyMap();

	QPixmap *sky;
	SkyPoint  focus, oldfocus, clickedPoint, MousePoint;
	SkyObject *clickedObject, *foundObject;
	dms LSTh, HourAngle;
	int pixelScale[13], ZoomLevel;
	int idSolInfo, idMessHST, idMoonInfo, idMoonImages, idMessInfo, idNGCHST;
	bool slewing;
	double Range[13];
	StarPixmap *starpix;	// the pixmap of the star

	int runningDownloads() { return downloads; }	// needed for quit the application correctly if a download is running
/*
 *Display object name and coordinates in the KStars infoPanel
 */
	void showFocusCoords( void );

public slots:
	virtual void setGeometry( int x, int y, int w, int h );
	virtual void setGeometry( const QRect &r );

// new slots for using the internal image-viewer
	void setUpDownloads();
	void setDownDownloads();

/**
	*If the skymap only needs to be repainted but not new computed use update(). This is always needed if another
	*widget like a dialog or another application is painting over the widget. New computing of skymap needs a lot of
	*time and the stars will be shown on the old position, so update() function saves a lot of cpu usage. So use Update()
	*to compute explicite the constellations.
	*/	
	void Update();

/**
	*Popup menu function: centers display at clicked position.
	*/
	void slotCenter( void );
/**
	*Popup menu function: Display 1st-Genration DSS image with ImageViewer.
	*/
	void slotDSS( void );
/**
	*Popup menu function: Display 2nd-Genration DSS image with ImageViewer.
	*/
	void slotDSS2( void );
/**
	*Popup menu function: Show webpage with konqueror (only available for some objects).
	*/
	void slotInfo( int id );
/**
	*Popup menu function: Display image with ImageViewer (only available for some objects).
	*/
	void slotImage( int id );
/**
	* Add a custom Image or Information URL.
	*/
	void addLink( void );
	
protected:
/**
	*Draw the Sky, and all objects in it.
	*/	
	virtual void paintEvent( QPaintEvent *e );
/**
	*Detect keystrokes: arrow keys, and +/- keys.
	*/
	virtual void keyPressEvent( QKeyEvent *e );
/**
	*When keyRelease is triggered, just set the "slewing" flag to false,
	*and update the display (many objects aren't drawn when slewing==true).
	*/
	virtual void keyReleaseEvent( QKeyEvent *e );
/**
	*Determine RA, Dec coordinates of clicked location.  Find the SkyObject
	*which is nearest to the clicked location.
	*
	*If left-clicked: Set set mouseButtonDown==true, slewing==true; display nearest object name in status bar.
	*If right-clicked: display popup menu appropriate for nearest object.
	*/	
	virtual void mousePressEvent( QMouseEvent *e );
/**
	*set mouseButtonDown==false, slewing==false
	*/	
	virtual void mouseReleaseEvent( QMouseEvent *e );
/**
	*Center SkyMap at double-clicked location
	*/
	virtual void mouseDoubleClickEvent( QMouseEvent *e );
/**
	*If mouseButtonDown==false: display RA, Dec of mouse pointer in status bar.
	*else:  redefine focus such that RA, Dec under mouse cursor remains constant.
	*/	
	virtual void mouseMoveEvent( QMouseEvent *e );

/**
	*If the skymap will be resized, the sky must be new computed. So this function calls explicite new computing of
	*the skymap.
	*/
	virtual void resizeEvent( QResizeEvent * );
	
/*
	*Set the shape of mouse cursor to a cross with 4 arrows.
	*/
private slots:
	void setMouseMoveCursor();
	
private:
/**
	*Initialize the popup menus
	*/
	void initPopupMenu( void );
/**
	*Given the coordinates of the SkyPoint argument, determine the
	*pixel coordinates in the SkyMap.  If Horiz==true, use the SkyPoint's
	*Alt/AZ coordinates; otherwise, use RA/Dec.
	*@returns QPoint containing pixel x, y coordinates of SkyPoint.
	*@param o SkyPoint to calculate x, y for.
	*@param Horiz if true, use Alt/Az coordinates.
	*@param LSTh pointer to Local Sidereal Time, needed to compute Alt/Az coords
	*@param lat pointer to observer's latitude, needed to compute Alt/Az coords
	*/
	QPoint getXY( SkyPoint *o, bool Horiz, dms LSTh, dms lat );
/**
	*Determine RA, Dec coordinates of the pixel at (dx, dy), which are the
	*pixel coordinates measured from the center of the SkyMap bitmap.
	*@param dx horizontal pixel offset from center of SkyMap.
	*@param dy vertical pixel offset from center of SkyMap.
	*@param Horiz if true, the SkyMap is displayed using the Horizontal coordinate system
	*@param LSTh Local sidereal time.
	*@param lat current geographic latitude
	*/	
	SkyPoint dXdYToRaDec( double dx, double dy, bool Horiz, dms LST, dms lat );
/**
	*Large switch-controlled statement to draw objects on the SkyMap
	*according to their type and catalog.  This is going to be changed
	*So that each SkyObject has its own draw() function, which will be
	*called from SkyMap's paintEvent().	
	*color is only needed by Stars.
	*/
	void drawSymbol( QPainter &p, int type, int x, int y, int size, QChar color=0 );
/**
	*Determine if the skypoint p might be visible in the current display window
	*/
	bool checkVisibility( SkyPoint *p, float fov, bool useAltAz, bool isPoleVisible );
/**
	*Sets the shape of the default mouse cursor to a cross.
	*/
	void setDefaultMouseCursor();
/**
	*Check the correctness of the current point on screen. This is needed to avoid a crash
	*of the program if a doubleclick or mousemove in empty space is happend. Empty space
	*is the space out of sky area.
	*/
	bool unusablePoint (double dx, double dy);
/**
	*Set the text of the Rise time and Set time labels in the popup menu
	*/
	void setRiseSetLabels( void );

	KStars *ksw;
	QString sURL;
	QPopupMenu *pmenu, *pmStar, *pmSolarSys, *pmMoon, *pmMess, *pmNGC;
	QLabel *pmStarTitle, *pmSolTitle, *pmMoonTitle, *pmMessTitle, *pmMessTitle2, *pmNGCTitle, *pmNGCTitle2;
	QLabel *pmMessType, *pmNGCType, *pmTitle, *pmTitle2, *pmType;
	QLabel *pmRiseTime, *pmSetTime;
	bool mouseButtonDown;
	bool mouseMoveCursor;		// true if mouseMoveEvent; needed by setMouseMoveCursor
	
	int downloads;		// the current downloads of images
	bool computeSkymap;	// if false only old pixmap will repainted with bitBlt(), this saves a lot of cpu usage
};

#endif
