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
#include <QToolTip>
#include <QFocusEvent>

dmsBox::dmsBox(QWidget *parent, bool dg)
	: KLineEdit(parent), EmptyFlag(true) {
	setMaxLength(14);
	setMaximumWidth(160);
	setDegType( dg );
	psave = palette();

	//Somehow, the color is getting reset to grey already!
	psave.setColor( QPalette::Active, QPalette::Text, Qt::black );

	connect( this, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotTextChanged( const QString & ) ) );
}

void dmsBox::setEmptyText() {
	//Set the text color to the average between 
	//QColorGroup::Text and QColorGroup::Base
	QPalette p = psave;
	QColor txc = p.color( QPalette::Active, QPalette::Text );
	QColor bgc = p.color( QPalette::Active, QPalette::Base );
	int r( ( txc.red()   + bgc.red()   )/2 );
	int g( ( txc.green() + bgc.green() )/2 );
	int b( ( txc.blue()  + bgc.blue()  )/2 );

	p.setColor( QPalette::Active,   QPalette::Text, QColor( r, g, b ) );
	p.setColor( QPalette::Inactive, QPalette::Text, QColor( r, g, b ) );
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
		setPalette( psave );
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
			setPalette( psave );
			EmptyFlag = false;
		}

		if ( ! EmptyFlag && t.isEmpty() ) {
			setEmptyText();
		}
	}
}

void dmsBox::setDegType( bool t ) {
	deg = t;
	QString sDeg = ( t ? i18n( "degrees" ) : i18n( "hours" ) );
	QString sMin = ( t ? i18n( "arcminutes" ) : i18n( "minutes" ) );
	QString sSec = ( t ? i18n( "arcseconds" ) : i18n( "seconds" ) );

	QString sTip = i18n( "Angle value in %1.", sDeg );
	QString sWhatsThis;

	if ( isReadOnly() ) {
	  sWhatsThis = i18n( "This box displays an angle in %1. "
			     "The three numbers displayed are the angle's "
			     "%1, %2, and %3." , sDeg, sMin, sSec);
	} else {
	  sTip += i18n( "  You may enter a simple integer, or a floating-point value, "
			"or space- or colon-delimited values specifying "
			"%1, %2 and %3" , sDeg, sMin, sSec);

	  sWhatsThis = i18n( "Enter an angle value in %1.  The angle can be expressed "
			     "as a simple integer (\"12\"), or floating-point "
			     "(\"12.33\") value, or as space- or colon-delimited "
			     "values specifying %1, %2 and %3 (\"12:20\", \"12:20:00\", "
			     "\"12 20\", \"12 20 00.0\", etc.)." , sDeg, sMin, sSec);
	}

	setToolTip( sTip );
	setWhatsThis( sWhatsThis );

	clear();
	setPalette( psave );
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
