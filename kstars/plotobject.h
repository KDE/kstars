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
/**Default constructor.  Create a POINTS object with an empty list of points.
	*/
	PlotObject();

/**Constructor.  Create a PlotObject according to the arguments.
	*/
	PlotObject( const QString &name, const QString &color, int otype, int size=2, int param=0 );

/**Destructor (empty)
	*/
	~PlotObject();

/**@enum TYPE
	*The Type classification of the PlotObject
	*/
	enum TYPE { POINTS=0, CURVE=1, LABEL=2, UNKNOWN_TYPE };

/**@enum PPARAM
	*Parameter specifying the kind of points
	*/
	enum PPARAM { DOT=0, CIRCLE=1, SQUARE=2, LETTER=3, UNKNOWN_POINT };

/**@enum CPARAM
	*Parameter specifying the kind of line.  These are numerically equal to
	*the Qt::PenStyle enum values.
	*/
	enum CPARAM { NO_LINE=0, SOLID=0, DASHED=1, DOTTED=2, DASHDOTTED=3, DASHDOTDOTTED=4, UNKNOWN_CURVE };

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
	unsigned int type() const { return Type; }

/**@short set the PlotObject's Type
	*@param t the new type
	*/
	void setType( unsigned int t ) { Type = t; }

/**@return the PlotObject's Size
	*/
	unsigned int size() const { return Size; }

/**@short set the PlotObject's Size
	*@param s the new size
	*/
	void setSize( unsigned int s ) { Size = s; }

/**@return the PlotObject's type-specific Parameter
	*/
	unsigned int param() const { return Parameter; }

/**@short set the PlotObject's type-specific Parameter
	*@param p the new parameter
	*/
	void setParam( unsigned int p ) { Parameter = p; }

/**@return a pointer to the QPoint at position i
	*@param i the index of the desired point.
	*/
	QPoint* point( unsigned int i ) { return pList.at(i); }

/**@short Add a point to the object's list.
	*@param p the point to add.
	*/
	void addPoint( const QPoint &p ) { pList.append( new QPoint( p.x(), p.y() ) ); }

/**@short Add a point to the object's list.  This is an overloaded function,
	*provided for convenience.  It behaves essentially like the above function.
	*@param p pointer to the point to add.
	*/
	void addPoint( QPoint *p ) { pList.append( p ); }

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

/**@short Draw the PlotObject on a QPainter.
	*@param p the target QPainter.
	*/
	void draw( QPainter &p );

private:
	QPtrList<QPoint> pList;
	unsigned int Size, Type, Parameter;
	QString Color, Name;
};

#endif
