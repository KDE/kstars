/***************************************************************************
                          infobox.cpp  -  description
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

#include <kdebug.h>
#include <qpainter.h>

#include "infobox.h"

InfoBox::InfoBox(){
	setText1( "" );
	setText2( "" );
	setText3( "" );
	move( 0, 0 );
//Initialize text dimension variables to 0
	FullTextWidth  = 0;
	FullTextHeight = 0;
	ShadedTextWidth  = 0;
	ShadedTextHeight = 0;

	AnchorFlag = AnchorNone;
	Visible = true;
	Shaded = false;
}

InfoBox::InfoBox( int x, int y, bool shade, QString t1, QString t2, QString t3 ) {
	setText1( t1 );
	setText2( t2 );
	setText3( t3 );
	move( x, y );
	Shaded = shade;
//Initialize text dimension variables to 0
	FullTextWidth  = 0;
	FullTextHeight = 0;
	ShadedTextWidth  = 0;
	ShadedTextHeight = 0;

	AnchorFlag = AnchorNone;
	Visible = true;
}

InfoBox::InfoBox( QPoint pt, bool shade, QString t1, QString t2, QString t3 ) {
	setText1( t1 );
	setText2( t2 );
	setText3( t3 );
	move( pt );
	Shaded = shade;
//Initialize text dimension variables to 0
	FullTextWidth  = 0;
	FullTextHeight = 0;
	ShadedTextWidth  = 0;
	ShadedTextHeight = 0;

	AnchorFlag = AnchorNone;
	Visible = true;
}

InfoBox::~InfoBox(){
}

void InfoBox::setAnchorRight( const bool ar ) {
	if ( ar ) setAnchorFlag( anchorFlag() | AnchorRight  );
	else      setAnchorFlag( anchorFlag() & ~AnchorRight );
}

void InfoBox::setAnchorBottom( const bool ab ) {
	if ( ab ) setAnchorFlag( anchorFlag() | AnchorBottom  );
	else      setAnchorFlag( anchorFlag() & ~AnchorBottom );
}

bool InfoBox::toggleShade() {
	Shaded = !Shaded;
	updateSize();

	emit shaded( Shaded );
	return Shaded;
}

void InfoBox::updateSize() {
	int dh = ShadedTextHeight/2;
//	kdDebug() << "Full=" << FullTextWidth + 2*padx() << "," << FullTextHeight - dh + 2*pady() << endl;
	if ( Shaded ) resize( ShadedTextWidth + 2*padx(), ShadedTextHeight - dh + 2*pady() );
	else resize( FullTextWidth + 2*padx(), FullTextHeight - dh + 2*pady() );
//	kdDebug() << "Size=" << Size.width() << "," << Size.height() << endl;
}

bool InfoBox::constrain( QRect r, bool inside ) {
	if ( inside ) {
		//Place InfoBox within QRect r:
		if ( x() < r.x() ) move( r.x(), y() );
		if ( y() < r.y() ) move( x(), r.y() );
		if ( x() + width() > r.right() ) move( r.right() - width(), y() );
		if ( y() + height() > r.bottom() ) move( x(), r.bottom() - height() );
		//The InfoBox is now within the bounds of QRect r, unless it is bigger than r.
		//In that case, we cannot obey the constraint, but the current position is as
		//close as we can get.  Return false in this case.
		if ( width() > r.width() || height() > r.height() ) return false;
		else return true;
	} else {
//FIXME...
		//Place InfoBox outside QRect r.  First, determine if InfoBox is within r:
//		if ( rect().intersects( r ) ) {
			//Move the InfoBox in all four directions until it no longer intersects r.
			//Determine which displacement is shortest
			//
		return false;
	}
}

void InfoBox::draw( QPainter &p, QColor BGColor, unsigned int BGMode ) {
	QRect r;
	int w,h;

	r = p.boundingRect( x(), y(), p.window().width(), p.window().height(), Qt::AlignCenter, text1() );
	ShadedTextWidth  = r.width();
	ShadedTextHeight = r.height();

	w = ShadedTextWidth;
	h = ShadedTextHeight;

	if ( !text2().isEmpty() ) {
		r = p.boundingRect( x(), y(), p.window().width(), p.window().height(), Qt::AlignCenter, text2() );
		if ( r.width() > w ) w = r.width();
		h += r.height();
	}

	if ( !text3().isEmpty() ) {
		r = p.boundingRect( x(), y(), p.window().width(), p.window().height(), Qt::AlignCenter, text3() );
		if ( r.width() > w ) w = r.width();
		h += r.height();
	}

	FullTextWidth = w;
	FullTextHeight = h;

	updateSize();
	constrain( QRect( 0, 0, p.window().width(), p.window().height() ) );

//Draw the box boundary and the text
	if ( BGMode==1 ) p.fillRect( x(), y(), width(), height(), QBrush( BGColor, Dense4Pattern ) );
	if ( BGMode==2 ) p.fillRect( x(), y(), width(), height(), QBrush( BGColor ) );

	p.drawText( x() + padx(), y() + ShadedTextHeight/2 + pady(), text1() );

	if ( !Shaded ) {
		if ( !text2().isEmpty() ) p.drawText( x() + padx(), y() + 3*ShadedTextHeight/2 + pady(), text2() );
		if ( !text3().isEmpty() ) p.drawText( x() + padx(), y() + 5*ShadedTextHeight/2 + pady(), text3() );
	}
}

QRect InfoBox::rect() const {
	return QRect( pos(), size() );
}

void InfoBox::move( int x, int y ) {
	Pos.setX( x ); Pos.setY( y ); emit moved( QPoint(x,y) );
}

void InfoBox::move( QPoint p ) {
	move( p.x(), p.y() ); emit moved( QPoint( p.x(), p.y() ) );
}

#include "infobox.moc"
