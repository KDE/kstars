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
#include <tqregexp.h>
#include <tqstring.h>
#include <tqtooltip.h>
#include <tqwhatsthis.h>

dmsBox::dmsBox(TQWidget *parent, const char *name, bool dg) 
	: KLineEdit(parent,name), EmptyFlag(true) {
	setMaxLength(14);
	setMaximumWidth(160);
	setDegType( dg );
	
	
	connect( this, TQT_SIGNAL( textChanged( const TQString & ) ), this, TQT_SLOT( slotTextChanged( const TQString & ) ) );
}

void dmsBox::setEmptyText() {
	TQPalette p = palette();
	TQColor txc = p.color( TQPalette::Active, TQColorGroup::Text );
	TQColor bgc = paletteBackgroundColor();
	int r( ( txc.red()   + bgc.red()   )/2 );
	int g( ( txc.green() + bgc.green() )/2 );
	int b( ( txc.blue()  + bgc.blue()  )/2 );
	
	p.setColor( TQPalette::Active,   TQColorGroup::Text, TQColor( r, g, b ) );
	p.setColor( TQPalette::Inactive, TQColorGroup::Text, TQColor( r, g, b ) );
	setPalette( p );
	
	if ( degType() ) 
		setText( "dd mm ss.s" );
	else 
		setText( "hh mm ss.s" );
		
	EmptyFlag = true;
}

void dmsBox::focusInEvent( TQFocusEvent *e ) {
	KLineEdit::focusInEvent( e );
	
	if ( EmptyFlag ) {
		clear();
		unsetPalette();
		EmptyFlag = false;
	}
}

void dmsBox::focusOutEvent( TQFocusEvent *e ) {
	KLineEdit::focusOutEvent( e );
	
	if ( text().isEmpty() ) {
		setEmptyText();
	}
}

void dmsBox::slotTextChanged( const TQString &t ) {
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
		TQToolTip::add( this, i18n( "Angle value in degrees. You may enter a simple integer \nor a floating-point value, or space- or colon-delimited values \nspecifying degrees, arcminutes and arcseconds." ) );
		TQWhatsThis::add( this, i18n( "Enter an angle value in degrees.  The angle can be expressed as a simple integer (\"45\") or floating-point (\"45.333\") value, or as space- or colon-delimited values specifying degrees, arcminutes and arcseconds (\"45:20\", \"45:20:00\", \"45:20\", \"45 20.0\", etc.)." ) ); 
	} else {
		TQToolTip::add( this, i18n( "Angle value in hours. You may enter a simple integer \nor floating-point value, or space- or colon-delimited values \nspecifying hours, minutes and seconds." ) );
		TQWhatsThis::add( this, i18n( "Enter an angle value in hours.  The angle can be expressed as a simple integer (\"12\") or floating-point (\"12.333\") value, or as space- or colon-delimited values specifying hours, minutes and seconds (\"12:20\", \"12:20:00\", \"12:20\", \"12 20.0\", etc.)." ) );
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
	setDMS( TQString().sprintf( "%02d %02d %05.2f", d.degree(), d.arcmin(), seconds ) );
}

void dmsBox::showInHours (const dms *d) { showInHours( dms( *d ) ); }
void dmsBox::showInHours (dms d)
{
	double seconds = d.second() + d.msecond()/1000.;
	setDMS( TQString().sprintf( "%02d %02d %05.2f", d.hour(), d.minute(), seconds ) );
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
