/***************************************************************************
                          mapcanvas.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Apr 10 2001
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




#ifndef MAPCANVAS_H
#define MAPCANVAS_H

#include <qwidget.h>
#include <qpoint.h>

/**@class MapCanvas
	*Used in LocationDialog for displaying a map of the Earth.
	*In addition, cities in the database are drawn as grey or white dots.
	*Also, the widget processes mouse clicks, to trim the list of
	*cities to those near the mouse click.
	*@short Widget used in the LocationDialog for displaying the world map.
	*@author Jason Harris
	*@version 1.0
	*/

class QPixmap;

class MapCanvas : public QWidget  {
	Q_OBJECT
public: 
	/**Default constructor.  Initialize the widget: create pixmaps, load the
		*world map bitmap, set pointers to the main window and the
		*LocationDialog parent.
		*/
	MapCanvas(QWidget *parent=0, const char *name=0);
	/**Destructor (empty)
		*/
	~MapCanvas();
	
public slots:
	/**Set the geometry of the map widget (overloaded from QWidget).
		*Resizes the size of the map pixmap to match the widget, and resets
		*the Origin QPoint so it remains at the center of the widget.
		*@note this is called automatically by resize events.
		*@p x the x-position of the widget
		*@p y the y-position of the widget
		*@p w the width of the widget
		*@p h the height of the widget
		*/
	virtual void setGeometry( int x, int y, int w, int h );
	
	/**Set the geometry of the map widget (overloaded from QWidget).
		*Resizes the size of the map pixmap to match the widget, and resets
		*the Origin QPoint so it remains at the center of the widget.
		*This function behaves just like the above function.  It differs
		*only in the data type of its argument.
		*@note this is called automatically by resize events.
		*@p r QRect describing geometry
		*/
	virtual void setGeometry( const QRect &r );

protected:
	/**Draw the map.  Draw grey dots on the locations of all cities, 
		*and highlight the cities which match the current filters 
		*as white dits.  Also draw a red crosshairs on the 
		*currently-selected city.
		*@see LocationDialog
		*/
	virtual void paintEvent( QPaintEvent *e );
	
	/**Trim the list of cities so that only those within 2 degrees
		*of the mouse click are shown in the list.
		*@see LocationDialog
		*/
	virtual void mousePressEvent( QMouseEvent *e );

private:
	QPixmap *Canvas, *bgImage;
	QString BGColor;
	QPoint origin;
};

#endif
