/***************************************************************************
                          infobox.h  -  description
                             -------------------
    begin                : Thu May 30 2002
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

#ifndef INFOBOX_H
#define INFOBOX_H

#include <qobject.h>
#include <qpainter.h>
#include <qpoint.h>
#include <qrect.h>
#include <qsize.h>
#include <qstring.h>

/**InfoBox encapsulates a lightweight "window" to be drawn directly on a pixmap.
	*The box contains up to three strings of information, drawn one per line.
	*The box is automatically resized to fit the longest line.  It can be moved and 
	*"shaded"; in its shaded state, only the first string is visible.
	*@short An interactive  box containing three lines of text drawn directly on a qpixmap.
	*@author Jason Harris
	*@version 0.9
	*/

class InfoBox : public QObject {
	Q_OBJECT
public:
	/**default constructor.  Creates an infobox with empty text string
		*and default geometry
		*/
	InfoBox();
	/**general constructor.  Specify the text string, x,y position, and shaded state.
		*@param x The initial x-position
		*@param y The initial y-position
		*@param shade The initial Shade state
		*@param t1 The first line of text
		*@param t2 The second line of text
		*@param t3 The third line of text
		*/
	InfoBox( int x, int y, bool shade, QString t1="", QString t2="", QString t3="" );
	
	/**general constructor.  Specify the text string, x,y position, and shaded state.
		*Differs from the above function only in the data types of its arguments.
		*@param p The initial position
		*@param shade The initial Shade state
		*@param t1 The first line of text
		*@param t2 The second line of text
		*@param t3 The third line of text
		*/
	InfoBox( QPoint p, bool shade, QString t1="", QString t2="", QString t3="" );

	/**Destructor (empty)*/
	~InfoBox();

	/**Draw the InfoBox on the specified QPainter
		*@param p The QPainter on which to draw the box
		*@param BGColor The background color, if it is to be drawn
		*@param fillBG If true, then fill in the box background.  Otherwise, leave it transparent.
		*/
	void draw( QPainter &p, QColor BGColor, bool fillBG );

	/**Toggle between the shaded and normal states.  Also update the box size and 
		*emit the "shaded" signal.
		*/
	bool toggleShade();

	/**Reset the x,y position.  Check to see if box should be "anchored" 
		*to a screen edge.
		*@param x The new x-position
		*@param y The new y-position
		*/
	void move( int x, int y );
	
	/**Reset the x,y position.  Check to see if box should be "anchored" 
		*to a screen edge.  Differs from the above function only in the types
		*of its arguments.
		*@param p The new position
		*/
	void move( QPoint p );

	/**Reset the width and height
		*@param w The new width
		*@param h The new height
		*/
	void resize( int w, int h ) { Size.setWidth( w ); Size.setHeight( h ); }
	
	/**Reset the width and height using a QSize argument
		*@param s The new size
		*/
	void resize( QSize s ) { Size.setWidth( s.width() ); Size.setHeight( s.height() ); }

	/**Set the size of the box to fit the current displayed text */
	void updateSize();

	/**Make sure the InfoBox is inside (or outside) the QRect r.  Move it if necessary.
		*@returns true if the function was able to obey the constraint; otherwise returns false.
		*@param r The QRect that the infobox must be made to be inside (or outside).
		*@param inside If true, then the infobox must be inside r; otherwise, it must be outside r.
		*/
	bool constrain( QRect r, bool inside=true );

	/**Reset the first text string
		*@param newt The new text string
		*/
	void setText1( QString newt ) { Text1 = newt; }
	
	/**Reset the second text string
		*@param newt The new text string
		*/
	void setText2( QString newt ) { Text2 = newt; }
	
	/**Reset the third text string
		*@param newt The new text string
		*/
	void setText3( QString newt ) { Text3 = newt; }

	//for now, padx() and pady() simply return a constant
	/**@returns the padding between the text and the box edge in the x-direction.*/
	int padx() const { return 6; }
	/**@returns the padding between the text and the box edge in the y-direction.*/
	int pady() const { return 6; }

	/**set the isHidden flag according to the bool argument */
	void setVisible( bool t ) { Visible = t; }

	//Accessors for private data
	
	/**@returns The x-position of the infobox*/
	int x() const { return Pos.x(); }
	/**@returns The y-position of the infobox*/
	int y() const { return Pos.y(); }
	/**@returns The position of the infobox*/
	QPoint pos() const { return Pos; }
	/**@returns The width of the infobox*/
	int width() const { return Size.width(); }
	/**@returns The height of the infobox*/
	int height() const { return Size.height(); }
	/**@returns The size of the infobox*/
	QSize size() const { return Size; }
	/**@returns the position and size of the infobox*/
	QRect rect() const;
	/**@returns true if infobox is currently visible (i.e., not hidden)*/
	bool isVisible() const { return Visible; }
	/**@returns The first string of the infobox*/
	QString text1() const { return Text1; }
	/**@returns The second string of the infobox*/
	QString text2() const { return Text2; }
	/**@returns The third string of the infobox*/
	QString text3() const { return Text3; }
	/**@returns true if infobox is currently anchored to right edge of the QPixmap*/
	bool anchorRight() const { return AnchorRight; }
	/**@returns true if infobox is currently anchored to bottom edge of the QPixmap*/
	bool anchorBottom() const { return AnchorBottom; }
	/**Set AnchorRight according to bool argument*/
	void setAnchorRight( const bool ar ) { AnchorRight = ar; }
	/**Set AnchorBottom according to bool argument*/
	void setAnchorBottom( const bool ab ) { AnchorBottom = ab; }

signals:
	/**Emit signal that Infobox was moved*/
	void moved( QPoint p );
	/**Emit signal that infobox shade-state was changed*/
	void shaded( bool s );

private:
	bool Shaded, Visible, AnchorRight, AnchorBottom;
	int FullTextWidth, FullTextHeight;
	int ShadedTextWidth, ShadedTextHeight;
	QPoint Pos;
	QSize Size;
	QString Text1, Text2, Text3;
};

#endif
