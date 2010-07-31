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

#include <tqstring.h>
#include <klocale.h>

/**@class FOV A simple class encapulating a Field-of-View symbol
	*@author Jason Harris
	*@version 1.0
	*/

class QPainter;

class FOV {
	public:
		FOV();
		FOV( TQString name );  //in this case, read params from fov.dat
		FOV( TQString name, float size, int shape=0, TQString color="#FFFFFF" );
		~FOV() {}

//			enum SHAPE { FOV_SQUARE=0, FOV_CIRCLE=1, FOV_CROSSHAIRS=2, FOV_BULLSEYE=3, FOV_UNKNOWN };

		TQString name() const { return Name; }
		void setName( const TQString &n ) { Name = n; }
		int shape() const { return Shape; }
		void setShape( int s ) { Shape = s; }
		float size() const { return Size; }
		void setSize( float s ) { Size = s; }
		TQString color() const { return Color; }
		void setColor( const TQString &c ) { Color = c; }

	/**@short draw the FOV symbol on a QPainter
		*@param p reference to the target TQPainter.  The painter should already be started.
		*@param size the size of the target symbol, in pixels.
		*/
		void draw( TQPainter &p, float size );

	private:
		TQString Name, Color;
		float Size;
		int Shape;
};

#endif
