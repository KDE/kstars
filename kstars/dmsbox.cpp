/***************************************************************************
                          dmsbox.cpp  -  description
                             -------------------
    begin                : wed Dec 19 2001
    copyright            : (C) 2001-2002 by Pablo de Vicente
    email                : vicente@oan.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <stdlib.h>

#include "dmsbox.h"

#include <kdebug.h>
#include <klocale.h>
#include <qregexp.h>
#include <qstring.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

dmsBox::dmsBox(QWidget *parent, const char *name, bool dg) 
	: KLineEdit(parent,name), EmptyFlag(true) {
	setMaxLength(14);
	setMaximumWidth(160);
	setDegType( dg );
	
	
	connect( this, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotTextChanged( const QString & ) ) );
}

void dmsBox::setEmptyText() {
	QPalette p = palette();
	QColor txc = p.color( QPalette::Active, QColorGroup::Text );
	QColor bgc = paletteBackgroundColor();
	int r( ( txc.red()   + bgc.red()   )/2 );
	int g( ( txc.green() + bgc.green() )/2 );
	int b( ( txc.blue()  + bgc.blue()  )/2 );
	
	p.setColor( QPalette::Active,   QColorGroup::Text, QColor( r, g, b ) );
	p.setColor( QPalette::Inactive, QColorGroup::Text, QColor( r, g, b ) );
	setPalette( p );
	
	if ( degType() ) 
		setText( "dd mm ss.s" );
	else 
		setText( "hh mm ss.s" );
		
	EmptyFlag = true;
}

void dmsBox::focusInEvent( QFocusEvent *e ) {
	KLineEdit::focusInEvent( e );
	
	if ( EmptyFlag ) {
		clear();
		unsetPalette();
		EmptyFlag = false;
	}
}

void dmsBox::focusOutEvent( QFocusEvent *e ) {
	KLineEdit::focusOutEvent( e );
	
	if ( text().isEmpty() ) {
		setEmptyText();
	}
}

void dmsBox::slotTextChanged( const QString &t ) {
	if ( ! hasFocus() ) {
		if ( EmptyFlag && ! t.isEmpty() ) {
			unsetPalette();
			EmptyFlag = false;
		}
		
		if ( ! EmptyFlag && t.isEmpty() ) {
			setEmptyText();
		}
	}
}

void dmsBox::setDegType( bool t ) {
	deg = t;

	if ( deg ) {
		QToolTip::add( this, i18n( "Angle value in degrees. You may enter a simple integer \nor a floating-point value, or space- or colon-delimited values \nspecifying degrees, arcminutes and arcseconds." ) );
		QWhatsThis::add( this, i18n( "Enter an angle value in degrees.  The angle can be expressed as a simple integer (\"45\") or floating-point (\"45.333\") value, or as space- or colon-delimited values specifying degrees, arcminutes and arcseconds (\"45:20\", \"45:20:00\", \"45:20\", \"45 20.0\", etc.)." ) ); 
	} else {
		QToolTip::add( this, i18n( "Angle value in hours. You may enter a simple integer \nor floating-point value, or space- or colon-delimited values \nspecifying hours, minutes and seconds." ) );
		QWhatsThis::add( this, i18n( "Enter an angle value in hours.  The angle can be expressed as a simple integer (\"12\") or floating-point (\"12.333\") value, or as space- or colon-delimited values specifying hours, minutes and seconds (\"12:20\", \"12:20:00\", \"12:20\", \"12 20.0\", etc.)." ) );
	}

	clear();
	unsetPalette();
	EmptyFlag = false;
	setEmptyText();
}

void dmsBox::showInDegrees (const dms *d) { showInDegrees( dms( *d ) ); }
void dmsBox::showInDegrees (dms d)
{
	double seconds = d.arcsec() + d.marcsec()/1000.;
	setDMS( QString().sprintf( "%02d %02d %05.2f", d.degree(), d.arcmin(), seconds ) );
}

void dmsBox::showInHours (const dms *d) { showInHours( dms( *d ) ); }
void dmsBox::showInHours (dms d)
{
	double seconds = d.second() + d.msecond()/1000.;
	setDMS( QString().sprintf( "%02d %02d %05.2f", d.hour(), d.minute(), seconds ) );
}

void dmsBox::show(const dms *d, bool deg) { show( dms( *d ),deg ); }
void dmsBox::show(dms d, bool deg)
{
	if (deg)
		showInDegrees(d);
	else
		showInHours(d);
}

dms dmsBox::createDms ( bool deg, bool *ok )
{
	dms dmsAngle(0.0);
	bool check;
	check = dmsAngle.setFromString( text(), deg );
	if (ok) *ok = check; //ok might be a null pointer!

	return dmsAngle;
}

dmsBox::~dmsBox(){
}

#include "dmsbox.moc"
