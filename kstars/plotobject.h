/***************************************************************************
                          plotobject.h - A list of points to be plotted
                             -------------------
    begin                : Sun 18 May 2003
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

#ifndef PLOTOBJECT_H
#define PLOTOBJECT_H

#include <qpoint.h>
#include <qptrlist.h>

class QString;
class QPainter;

/**class DRect
	*@short equivalent of QRect with double x,y coordinates
	*@author Jason Harris
	*@version 1.0
	*/
class DRect {
public:
	DRect() { DRect( 0.0, 0.0, 1.0, 1.0 ); }
	DRect( double x, double y, double w, double h ) { X = x; Y = y; W = w; H = h; }
	~DRect() {}

	double x() const { return X; }
	double y() const { return Y; }
	double w() const { return W; }
	double h() const { return H; }
	double x2() const { return X + W; }
	double y2() const { return Y + H; }

	void setX( double x ) { X = x; }
	void setY( double y ) { Y = y; }
	void setw( double w ) { W = w; }
	void seth( double h ) { H = h; }

private:
	double X,Y,W,H;
};

/**class DPoint
	*@short equivalent of QPoint with double x,y coordinates
	*@author Jason Harris
	*@version 1.0
	*/
class DPoint {
public:
	DPoint() { DPoint( 0.0, 0.0 ); }
	DPoint( double x, double y ) { setX( x ); setY( y ); }
	~DPoint() {}

	double x() const { return X; }
	double y() const { return Y; }

	QPoint qpoint( QRect pb, DRect db ) {
		int px = pb.left() + int( pb.width()*( x() -  db.x() )/db.w() );
		int py = pb.top() + int( pb.height()*( db.y2() - y() )/db.h() );
		return QPoint( px, py );
	}

	void setX( double x ) { X = x; }
	void setY( double y ) { Y = y; }

private:
	double X, Y;
};

/**@class PlotObject
	*@short Encapsulates an object to be plotted in a PlotWidget.
	*@author Jason Harris
	*@version 1.0
	*Each PlotObject consists of a list of QPoints, an object type, a color, a size,
	*and a QString name.  An additional integer (param) specifies something further
	*about the object's appearance, depending on its type.  There is a draw function
	*for plotting the object on a PlotWidget's QPainter.
	*/
class PlotObject{
public:
/**@enum PTYPE
	*The Type classification of the PlotObject
	*/
	enum PTYPE { POINTS=0, CURVE=1, LABEL=2, POLYGON=3, UNKNOWN_TYPE };

/**@enum PPARAM
	*Parameter specifying the kind of points
	*/
	enum PPARAM { DOT=0, CIRCLE=1, SQUARE=2, LETTER=3, UNKNOWN_POINT };

/**@enum CPARAM
	*Parameter specifying the kind of line.  These are numerically equal to
	*the Qt::PenStyle enum values.
	*/
	enum CPARAM { NO_LINE=0, SOLID=1, DASHED=2, DOTTED=3, DASHDOTTED=4, DASHDOTDOTTED=5, UNKNOWN_CURVE };

/**Default constructor.  Create a POINTS-type object with an empty list of points.
	*/
	PlotObject();

/**Constructor.  Create a PlotObject according to the arguments.
	*/
	PlotObject( const QString &name, const QString &color, PTYPE otype, unsigned int size=2, unsigned int param=0 );

/**Destructor (empty)
	*/
	~PlotObject();

/**@return the PlotObject's Name
	*/
	QString name() const { return Name; }

/**@short set the PlotObject's Name
	*@param n the new name
	*/
	void setName( const QString &n ) { Name = n; }

/**@return the PlotObject's Color
	*/
	QString color() const { return Color; }

/**@short set the PlotObject's Color
	*@param n the new color
	*/
	void setColor( const QString &c ) { Color = c; }

/**@return the PlotObject's Type
	*/
	PTYPE type() const { return Type; }

/**@short set the PlotObject's Type
	*@param t the new type
	*/
	void setType( PTYPE t ) { Type = t; }

/**@return the PlotObject's Size
	*/
	unsigned int size() const { return Size; }

/**@short set the PlotObject's Size
	*@param s the new size
	*/
	void setSize( unsigned int s ) { Size = s; }

/**@return the PlotObject's type-specific Parameter
	*Parameter is an unsigned int because it can either be a PPARAM or a CPARAM enum.
	*/
	unsigned int param() const { return Parameter; }

/**@short set the PlotObject's type-specific Parameter
	*@param p the new parameter
	*Parameter is an unsigned int because it can either be a PPARAM or a CPARAM enum.
	*/
	void setParam( unsigned int p ) { Parameter = p; }

/**@return a pointer to the DPoint at position i
	*@param i the index of the desired point.
	*/
	DPoint* point( unsigned int i ) { return pList.at(i); }

	QPtrList<DPoint>* points() { return &pList; }

/**@short Add a point to the object's list.
	*@param p the DPoint to add.
	*/
	void addPoint( const DPoint &p ) { pList.append( new DPoint( p.x(), p.y() ) ); }

/**@short Add a point to the object's list.  This is an overloaded function,
	*provided for convenience.  It behaves essentially like the above function.
	*@param p pointer to the DPoint to add.
	*/
	void addPoint( DPoint *p ) { pList.append( p ); }

/**@short remove the QPoint at position index from the list of points
	*@param index the index of the point to be removed.
	*/
	void removePoint( unsigned int index );

/**@return the number of QPoints currently in the list
	*/
	unsigned int count() const { return pList.count(); }

/**@short clear the Object's points list
	*/
	void clearPoints() { pList.clear(); }

private:
	QPtrList<DPoint> pList;
	PTYPE Type;
	unsigned int Size, Parameter;
	QString Color, Name;
};

#endif
