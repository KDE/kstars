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
#include <qpoint.h>
#include <qrect.h>
#include <qsize.h>
#include <qstring.h>

/**@class InfoBox 
	*InfoBoxencapsulates a lightweight floating "window" to be drawn directly 
	*on a pixmap.  The window contains three lines of text, and it can 
	*be "shaded" to show only the top line.  The window resizes itself 
	*automatically to contain the text within it.
	*@author Jason Harris
	*@version 1.0
	*/

class QPainter;

class InfoBox : public QObject {
	Q_OBJECT
public:
	/**default constructor.  Creates an infobox with empty text string
		*and default geometry
		*/
	InfoBox();
	
	/**General constructor.  Specify The text string, x,y position and size.
		*@param x the X-coordinate for the box
		*@param y the Y-coordinate for the box
		*@param shade if TRUE, apply text shading as well
		*@param t1 The first line of text
		*@param t2 The second line of text
		*@param t3 The third line of text
		*/
	InfoBox( int x, int y, bool shade, QString t1="", QString t2="", QString t3="" );
	
	/**General constructor.  Specify The text string, x,y position and size.
		*This behaves just like the above function.  It differs only in the data types
		*of its arguments.
		*@param p The (x,y) position of the box
		*@param shade if TRUE, apply text shading as well
		*@param t1 The first line of text
		*@param t2 The second line of text
		*@param t3 The third line of text
		*/
	InfoBox( QPoint p, bool shade, QString t1="", QString t2="", QString t3="" );

	/**Destructor (empty)*/
	~InfoBox();

	/**Draw the InfoBox.  First calls updateSize() and constrain() to make sure 
		*the box is onscreen and the correct size.
		*@param p reference to the QPainter on which to draw the box
		*@param BGColor the background color to be used
		*@param BGMode the background mode (0=none; 1=semi-transparent; 2=opaque)
		*@see InfoBox::updateSize()
		*@see InfoBox::constrain()
		*/
	void draw( QPainter &p, QColor BGColor, unsigned int BGMode );

	/**Toggle the Shaded state of the box.
		*/
	bool toggleShade();

	/**Reset the x,y position.  Check the edge anchors.
		*@param x the new X-position
		*@param y the new Y-position
		*/
	void move( int x, int y );
	
	/**Reset the x,y position.  Check the edge anchors.
		*This function behaves just like the above function.  It differs 
		*only in the data type of its arguments.
		*@param p the new (X,Y) position
		*/
	void move( QPoint p );

	/**Reset the width and height.
		*@param w the new width
		*@param h the new height
		*/
	void resize( int w, int h ) { Size.setWidth( w ); Size.setHeight( h ); }
	
	/**Reset the width and height.  This function behaves just like the above 
		*function.  It differs only in the data type of its arguments.
		*@param s the new size
		*/
	void resize( QSize s ) { Size.setWidth( s.width() ); Size.setHeight( s.height() ); }

	/**Set the size of the box to fit the current displayed text */
	void updateSize();

	/**Make sure the InfoBox is inside (or outside) the QRect r.
		*@return true if the function was able to obey the constraint.
		*@param r the Rect which the box must lie completely inside/outside of.
		*@param inside if TRUE (the default), the box must lie inside the rect r.  
		*Otherwise, the box must lie *outside* rect r.
		*/
	bool constrain( QRect r, bool inside=true );

	/**Reset the first text string
		*@param newt the new text.
		*/
	void setText1( QString newt ) { Text1 = newt; }

	/**Reset the second text string
		*@param newt the new text.
		*/
	void setText2( QString newt ) { Text2 = newt; }

	/**Reset the third text string
		*@param newt the new text.
		*/
	void setText3( QString newt ) { Text3 = newt; }

	//temporarily, padx() and pady() simply return a constant
	int padx() const { return 6; }
	int pady() const { return 6; }

	/**set the Visible flag according to the bool argument */
	void setVisible( bool t ) { Visible = t; }

	/**@return the X-position of the box*/
	int x() const { return Pos.x(); }

	/**@return the Y-position of the box*/
	int y() const { return Pos.y(); }

	/**@return the (X,Y) position of the box*/
	QPoint pos() const { return Pos; }

	/**@return the width of the box*/
	int width() const { return Size.width(); }

	/**@return the height of the box*/
	int height() const { return Size.height(); }

	/**@return the size of the box*/
	QSize size() const { return Size; }

	/**@return whether the box is visible */
	bool isVisible() const { return Visible; }

	/**@return the first line of text*/
	QString text1() const { return Text1; }

	/**@return the second line of text*/
	QString text2() const { return Text2; }

	/**@return the third line of text*/
	QString text3() const { return Text3; }

	/**@return the geometry of the box*/
	QRect rect() const;

	/**@return TRUE if the box is anchored to the right window edge*/
	bool anchorRight() const { return ( AnchorFlag & AnchorRight ); }

	/**@return TRUE if the box is anchored to the bottom window edge*/
	bool anchorBottom() const { return ( AnchorFlag & AnchorBottom ); }

	/**Set the box to be anchored to the right window edge*/
	void setAnchorRight( const bool ar );

	/**Set the box to be anchored to the bottom window edge*/
	void setAnchorBottom( const bool ab );

	/**@return the box's anchor flag bitmask.*/
	int  anchorFlag() const { return AnchorFlag; }

	/**Set the box's anchor flag bitmask*/
	void setAnchorFlag( const int af ) { AnchorFlag = af; }

	enum AnchorType { 
		AnchorNone   = 0x0000,
		AnchorRight  = 0x0001,
		AnchorBottom = 0x0002,
		AnchorBoth   = AnchorRight | AnchorBottom
	};

signals:
	/**Signal emitted when the box is moved
		*@param p the new (X,Y) position
		*@see InfoBox::move()
		*/
	void moved( QPoint p );
	
	/**Signal emitted when the box's shaded-state is toggled
		*@param s the new shaded state
		*@see InfoBox::toggleShade()
		*/
	void shaded( bool s );

private:
	bool Shaded, Visible;
	//TextWidth, TextHeight are the text dimensions when box is unshaded;
	//TextWidth1, TextHeight1 are the text dimensions when the box is shaded.
	int FullTextWidth, FullTextHeight;
	int ShadedTextWidth, ShadedTextHeight;
	int AnchorFlag;
	QPoint Pos;
	QSize Size;
	QString Text1, Text2, Text3;
};

#endif
