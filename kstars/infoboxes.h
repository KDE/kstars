/***************************************************************************
                          infoboxes.h  -  description
                             -------------------
    begin                : Wed Jun 5 2002
    copyright            : (C) 2002 by Jason Harris
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

#ifndef INFOBOXES_H
#define INFOBOXES_H

#include <qobject.h>
#include <qcolor.h>
#include <qevent.h>
#include <qpoint.h>
#include <kdebug.h>

#include "infobox.h"

/**@class InfoBoxes
	*Infoboxes manages the three infobox objects which are drawn on the Skymap.
	*Each Infobox is a member variable in Infoboxes.  Infoboxes handles user 
	*interactions with the boxes, and makes sure they do not overlap each other or 
	*move outside the bounds of the SkyMap.
	*@short Infoboxes encapsulates and manages the three Infobox objects
	*@author Jason Harris
	*@version 1.0
	*/

class QPainter;
class GeoLocation;
class SkyPoint;
class dms;
class InfoBox;
class KStarsDateTime;

class InfoBoxes : public QObject {
Q_OBJECT
public:
/**Constructor.  Create three infoboxes and place them in the skymap.
	*@param w The width of the region in which the boxes can be drawn 
	*(typically the width of the SkyMap)
	*@param h The height of the region in which the boxes can be drawn 
	*(typically the height of the SkyMap)
	*@param tx the x-position of the Time infobox
	*@param ty the y-position of the Time infobox
	*@param tshade if TRUE, apply text shading to the Time infobox
	*@param gx the x-position of the Geographic infobox
	*@param gy the y-position of the Geographic infobox
	*@param gshade if TRUE, apply text shading to the Geographic infobox
	*@param fx the x-position of the Focus-object infobox
	*@param fy the y-position of the Focus-object infobox
	*@param fshade if TRUE, apply text shading to the Focus-object infobox
	*@param colorText The foreground color for infoboxes
	*@param colorGrab The foreground color for infoboxes, while they are 
	*"grabbed" by the user
	*@param colorBG The background color for infoboxes
	*
	*@todo Use Qt::white as default color instead of QColor("white"),
	*      for default values of colorText, colorGrab and colorBG,
	*      since that's considerably faster.
	*/
	InfoBoxes( int w, int h,
			int tx=0, int ty=0, bool tshade=false,
			int gx=0, int gy=600, bool gshade=false,
			int fx=600, int fy=0, bool fshade=false,
			QColor colorText=QColor("white"),
			QColor colorGrab=QColor("red"),
			QColor colorBG=QColor("black") );

/**Constructor.  Create three infoboxes and place them in the skymap.
	*Differs from the above function only in the types of its arguments.
	*@param w The width of the region in which the boxes can be drawn 
	*(typically the width of the SkyMap)
	*@param h The height of the region in which the boxes can be drawn 
	*(typically the height of the SkyMap)
	*@param tp the position of the Time infobox
	*@param tshade if TRUE, apply text shading to the Time infobox
	*@param gp the position of the Geographic infobox
	*@param gshade if TRUE, apply text shading to the Geographic infobox
	*@param fp the position of the Focus-object infobox
	*@param fshade if TRUE, apply text shading to the Focus-object infobox
	*@param colorText The foreground color for infoboxes
	*@param colorGrab The foreground color for infoboxes, while they are 
	*"grabbed" by the user
	*@param colorBG The background color for infoboxes
	*/
	InfoBoxes( int w, int h,
			QPoint tp, bool tshade,
			QPoint gp, bool gshade,
			QPoint fp, bool fshade,
			QColor colorText=QColor("white"),
			QColor colorGrab=QColor("red"),
			QColor colorBG=QColor("black") );

/**Destructor (empty)*/
	~InfoBoxes();

/**@return pointer to the Time infobox*/
	InfoBox *timeBox()  { return TimeBox; }
/**@return pointer to the Geographic infobox*/
	InfoBox *geoBox()   { return GeoBox; }
/**@return pointer to the Focus-object infobox*/
	InfoBox *focusBox() { return FocusBox; }

/**Resets the width and height parameters.  These usually reflect the size
	*of the Skymap widget (Skymap::resizeEvent() calls this function).
	*Will also reposition the infoboxes to fit the new size.  Infoboxes
	*that were along an edge will remain along the edge.
	*@param w The new width
	*@param h The new height
	*/
	void resize( int w, int h );

/**@return the width of the region containing the infoboxes (usually the
	*width of the Skymap)
	*/
	int width() const { return Width; }

/**@return the height of the region containing the infoboxes (usually the
	*height of the Skymap)
	*/
	int height() const { return Height; }
	
/**Draw the boxes on a Qpainter object (representing the SkyMap).
	*@param p The QPainter on which to draw the boxes.
	*@param FGColor The foreground color (Pen color) to use when drawing boxes.
	*@param grabColor The foreground color to use if the box is "grabbed" by the user.
	*@param BGColor The background color (brush color) to use
	*@param BGMode: 0=no BG fill; 1=transparent BG fill; 2=Opaque BG fill.
	*/
	void drawBoxes( QPainter &p, QColor FGColor=QColor("white"),
			QColor grabColor=QColor("red"), QColor BGColor=QColor("black"),
			unsigned int BGMode=0 );
	
/**Determine whether a mouse click occurred inside one of the infoboxes.
	*Also, set the internal variable GrabBox to indicate which box was grabbed.
	*Finally, set the internal variable GrabPos to record the relative position of the
	*mouse cursor inside the box (we hold this position constant while dragging).
	*@param e The mouse event to check (it's a mousePressEvent)
	*@return true if the mouse press occurred inside one of the infoboxes.
	*/
	bool grabBox( QMouseEvent *e );
	
/**Set the internal variable GrabBox to 0, indicating that no box is currently 
	*grabbed.  Also determine if any box should be anchored to an edge.  (This
	*is called by SkyMap::mouseReleaseEvent() )
	*@return true if a box was grabbed in the first place; otherwise, return false.
	*/
	bool unGrabBox();
	
/**Move the Grabbed box around by keeping the relative position of the mouse cursor
	*to the box position equal to GrabPos. (this is called by SkyMap::mouseMoveEvent() ).
	*Once the box has been moved, we call fixCollisions() to make sure the boxes don't
	*overlap or exceed the SkyMap boundaries.
	*@param e The mouse event which contains the new mouse cursor position
	*@return false if no box is grabbed; otherwise, moves the grabbed box and returns true.
	*/
	bool dragBox( QMouseEvent *e );
	
/**Toggle the shade-state of the infobox in which the user double-clicked.
	*After shading the box, call fixCollisions() on the other two boxes.
	*(This is called by SkyMap::mouseDoubleClickEvent() )
	*@param e the mouse event containing the position of the double-click.
	*@return false if the double-click was not inside any box; otherwise shade the
	*target box and return true.
	*/
	bool shadeBox( QMouseEvent *e );
	
/**Make sure the target Infobox lies within the SkyMap boundaries, and that it does
	*not overlap with the other two Infoboxes.  If an overlap is detected, the target
	*box does a test-displacement each direction until there is no overlap (or the 
	*SkyMap boundary is reached).  The target box is then moved in the direction that 
	*required the smallest displacement to remove the overlap.
	*@param target the infobox which should be tested for collisions.
	*@return false if the box collisions could not be resolved; otherwise, returns true.
	*/
	bool fixCollisions( InfoBox *target );
	
/**@return true if the collection of infoboxes is visible (i.e., not hidden).
	*/
	bool isVisible() { return Visible; }

public slots:
/**Set whether the Infoboxes should be drawn, according to the bool argument.
	*This is the visibility setting for all three boxes.  Each individual box 
	*also has its own Visible parameter.  A box is only drawn if both 
	*Infoboxes::Visible /and/ Infobox::Visible are true.
	*@param t If true, the Infoboxes will be drawn.
	*/
	void setVisible( bool t ) { Visible = t; }
	
/**Call the TimeBox's setVisible() function.
	*@param t The bool parameter to send
	*/
	void showTimeBox( bool t ) { TimeBox->setVisible( t ); }

/**Call the GeoBox's setVisible() function.
	*@param t The bool parameter to send
	*/
	void showGeoBox( bool t ) { GeoBox->setVisible( t ); }

/**Call the FocusBox's setVisible() function.
	*@param t The bool parameter to send
	*/
	void showFocusBox( bool t ) { FocusBox->setVisible( t ); }

/**Update the TimeBox strings according to the arguments.
	*The arguments are date/time objects; this function converts them to  
	*strings and displays them in the TimeBox.
	*@param ut The Universal Time date/time object
	*@param lt The Local Time date/time object
	*@param lst The Sidereal Time object
	*@return true if values have changed
	*/
	bool timeChanged( const KStarsDateTime &ut, const KStarsDateTime &lt, dms *lst );

/**Update the GeoBox strings according to the argument.
	*@param geo The Geographic Location (we get the name, longitude and latitude from this)
	*@return true if values have changed
	*/
	bool geoChanged(const GeoLocation *geo);

/**Update the FocusBox coordinates strings according to the argument.
	*@param p the SkyPoint object from which we get the coordinates.
	*@return true if values have changed
	*/
	bool focusCoordChanged(const SkyPoint *p);

/**Update the FocusBox name string according to the argument.
	*@param n The object name
	*@return true if values have changed
	*/
	bool focusObjChanged(const QString &n);

/**Check if boxes are anchored with bottom or right border.
	@param resetToDefault reset all borders of boxes to false before checking borders.
	*/
	void checkBorders(bool resetToDefault=true);

private:
	int Width, Height;
	int GrabbedBox;
	bool Visible;
	const QColor boxColor, grabColor, bgColor;
	QPoint GrabPos;
	InfoBox *GeoBox, *FocusBox, *TimeBox;
};

#endif
