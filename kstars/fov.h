/***************************************************************************
                          fov.h  -  description
                             -------------------
    begin                : Fri 05 Sept 2003
    copyright            : (C) 2003 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FOV_H
#define FOV_H

#include <qstring.h>
#include <klocale.h>

/**@class FOV A simple class encapulating a Field-of-View symbol
	*@author Jason Harris
	*@version 1.0
	*/

class QPainter;

class FOV {
	public:
		FOV();
		FOV( QString name );  //in this case, read params from fov.dat
		FOV( QString name, float size, int shape=0, QString color="#FFFFFF" );
		~FOV() {}

//			enum SHAPE { FOV_SQUARE=0, FOV_CIRCLE=1, FOV_CROSSHAIRS=2, FOV_BULLSEYE=3, FOV_UNKNOWN };

		QString name() const { return Name; }
		void setName( const QString &n ) { Name = n; }
		int shape() const { return Shape; }
		void setShape( int s ) { Shape = s; }
		float size() const { return Size; }
		void setSize( float s ) { Size = s; }
		QString color() const { return Color; }
		void setColor( const QString &c ) { Color = c; }

	/**@short draw the FOV symbol on a QPainter
		*@param p reference to the target QPainter.  The painter should already be started.
		*@param size the size of the target symbol, in pixels.
		*/
		void draw( QPainter &p, float size );

	private:
		QString Name, Color;
		float Size;
		int Shape;
};

#endif
