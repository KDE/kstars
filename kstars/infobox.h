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
	*This preliminary version contains a text string, and geometry (x,y,w,h).
  *@author Jason Harris
  */

class InfoBox : public QObject {
	Q_OBJECT
public:
	/**default constructor.  Creates an infobox with empty text string
		*and default geometry
		*/
	InfoBox();
	/**general constructor.  Specify The text string, x,y position and size.
		*/
	InfoBox( int x, int y, bool shade, QString t1="", QString t2="", QString t3="" );
	InfoBox( QPoint p, bool shade, QString t1="", QString t2="", QString t3="" );

	/**Destructor (empty)*/
	~InfoBox();

	/**Draw the InfoBox on the specified QPainter*/
	void draw( QPainter &p, QColor BGColor, unsigned int BGMode );

	bool toggleShade();

	/**Reset the x,y position.  Check the anchors*/
	void move( int x, int y );
	void move( QPoint p );

	/**Reset the width and height*/
	void resize( int w, int h ) { Size.setWidth( w ); Size.setHeight( h ); }
	/**Reset the width and height using a QSize argument*/
	void resize( QSize s ) { Size.setWidth( s.width() ); Size.setHeight( s.height() ); }

	/**Set the size of the box to fit the current displayed text */
	void updateSize();

	/**Make sure the InfoBox is inside (or outside) the QRect r.
		*@returns true if the function was able to obey the constraint; otherwise returns false.
		*/
	bool constrain( QRect r, bool inside=true );

	/**Reset the text string*/
	void setText1( QString newt ) { Text1 = newt; }
	void setText2( QString newt ) { Text2 = newt; }
	void setText3( QString newt ) { Text3 = newt; }

	//temporarily, padx() and pady() simply return a constant
	int padx() const { return 6; }
	int pady() const { return 6; }

	/**set the isHidden flag according to the bool argument */
	void setVisible( bool t ) { Visible = t; }

	/**Accessors for private data*/
	int x() const { return Pos.x(); }
	int y() const { return Pos.y(); }
	QPoint pos() const { return Pos; }
	int width() const { return Size.width(); }
	int height() const { return Size.height(); }
	QSize size() const { return Size; }
	bool isVisible() const { return Visible; }
	QString text1() const { return Text1; }
	QString text2() const { return Text2; }
	QString text3() const { return Text3; }
	QRect rect() const;
	bool anchorRight() const { return ( AnchorFlag & AnchorRight ); }
	bool anchorBottom() const { return ( AnchorFlag & AnchorBottom ); }
	void setAnchorRight( const bool ar );
	void setAnchorBottom( const bool ab );
	int  anchorFlag() const { return AnchorFlag; }
	void setAnchorFlag( const int af ) { AnchorFlag = af; }

	enum AnchorType { 
		AnchorNone   = 0x0000,
		AnchorRight  = 0x0001,
		AnchorBottom = 0x0002,
		AnchorBoth   = AnchorRight | AnchorBottom
	};

signals:
	void moved( QPoint p );
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
