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
#include <qpainter.h>
#include <qpixmap.h>
#include <qpoint.h>

/**
	*Used in LocationDialog for displaying a bitmap of the Earth.
	*In addition, cities in the database are drawn as grey or white dots.
	*Also, the widget processes mouse clicks, to trim the list of
	*cities to those near the mouse click.
	*@short Widget used in the LocationDialog for displaying the world map.
  *@author Jason Harris
  *@version 0.9
  */

class MapCanvas : public QWidget  {
   Q_OBJECT
public: 
/**
	*Default constructor.  Initialize the widget: create pixmaps, load the
	*world map bitmap, set pointers to the main window and the
	*LocationDialog parent.
	*/
	MapCanvas(QWidget *parent=0, const char *name=0);
/**
	*Destructor (empty)
	*/
	~MapCanvas();
	
public slots:
	virtual void setGeometry( int x, int y, int w, int h );
  virtual void setGeometry( const QRect &r );

protected:
	virtual void paintEvent( QPaintEvent *e );
	virtual void mousePressEvent( QMouseEvent *e );

private:
	QPixmap *Canvas, *bgImage;		
	QString BGColor;
	QPoint origin;
};

#endif
