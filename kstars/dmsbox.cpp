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
#include "dms.h"

#include <kdebug.h>
#include <qregexp.h>

dmsBox::dmsBox(QWidget *parent, const char *name, bool dg) : QLineEdit(parent,name) {

	
//	QHBox * dBox = new QHBox(parent,name);
	
//	dmsName = new QLineEdit( dBox,"dmsName");
	
//	dBox->setSpacing(1);
//	setStretchFactor(dBox,0);
//	dBox->setMargin(6);

//	dmsName->setMaxLength(14);
//	dmsName->setMaximumWidth(160);
	setMaxLength(14);
	setMaximumWidth(160);

//	dBox->setMaximumWidth(180);

	deg = dg;
}

void dmsBox::showInDegrees (const dms *d) { showInDegrees( dms( *d ) ); }
void dmsBox::showInDegrees (dms d)
{
	double seconds = d.arcsec() + d.marcsec()/1000.;

	setDMS ( QString("%1 %2 %3").arg(d.degree(),2).arg(d.arcmin(),2).arg(seconds,6,'f',2) );
}

void dmsBox::showInHours (const dms *d) { showInHours( dms( *d ) ); }
void dmsBox::showInHours (dms d)
{
	double seconds = d.second() + d.msecond()/1000.;

	setDMS ( QString("%1 %2 %3").arg(d.hour(),2).arg(d.minute(),2).arg(seconds,6,'f',3) );

}

void dmsBox::show(const dms *d) { show( dms( *d ) ); }
void dmsBox::show(dms d)
{
	if (deg)
		showInDegrees(d);
	else
		showInHours(d);
}

dms dmsBox::createDms ( bool deg, bool *ok )
{
//	QString entry;
	int d = 0, m = 0;
	double s = 0.0;
	dms dmsAng;
	bool valueFound = false, badEntry = false , checkValue = false;

//Initialize bool for result
	if ( ok != NULL ) *ok = false;

//	QString errMsg = i18n( "Unable to parse %1 entry.  Specify a %1 value as a simple integer, a floating-point number, or a triplet of values using colons or spaces as separators." );

	QString entry = text().stripWhiteSpace();

	//Try simplest cases: integer or double representation

	d = entry.toInt( &checkValue );
	if ( checkValue ) {
		if (deg) dmsAng.setD( d, 0, 0 );
		else dmsAng.setH( d, 0, 0 );
		valueFound = true;
		if ( ok != NULL ) *ok = true;
		return dmsAng;
	} else {
		double x = entry.toDouble( &checkValue );
		if ( checkValue ) {
			if ( deg ) dmsAng.setD( x );
			else dmsAng.setH( x );
			valueFound = true;
			if ( ok != NULL ) *ok = true;
			return dmsAng;
		}
	}

	//no success yet...try assuming multiple fields

	if ( !valueFound ) { 
		QStringList fields;
		
		//check for colon-delimiters or space-delimiters
		if ( entry.contains(':') ) 
			fields = QStringList::split( ':', entry );
		else fields = QStringList::split( " ", entry ); 

		// If two fields we will add a third one, and then parse with 
		// the 3-field code block. If field[1] is a double, convert 
		// it to integer arcmin, and convert the remainder to arcsec
		 
		if ( fields.count() == 2 ) {
			double mx = fields[1].toDouble( &checkValue );
			if ( checkValue ) {
				fields[1] = QString("%1").arg( int(mx) );
				fields.append( QString("%1").arg( int( 60.0*(mx - int(mx)) ) ) );
			} else {
				fields.append( QString( "0" ) );
			}
		}
		
		// Three fields space-delimited ( h/d m s ); 
		// ignore all after 3rd field

		if ( fields.count() >= 3 ) {
			fields[0].replace( QRegExp("h"), "" );
			fields[0].replace( QRegExp("d"), "" );
			fields[1].replace( QRegExp("m"), "" );
			fields[2].replace( QRegExp("s"), "" );
		}
		//See if first two fields parse as integers.
		//
		d = fields[0].toInt( &checkValue );
		if ( !checkValue ) badEntry = true;
		m = fields[1].toInt( &checkValue );
		if ( !checkValue ) badEntry = true;
		s = fields[2].toDouble( &checkValue );
		if ( !checkValue ) badEntry = true;

		if ( !badEntry ) {
			valueFound = true;
			double D = (double)abs(d) + (double)m/60. 
					+ (double)s/3600.;
			if ( d <0 ) {D = -1.0*D;}

			if ( ok != NULL ) *ok = true;

			if (deg) {  	
				return	dms( D );
			} else {
				dms h;
				h.setH (D);
				return	h;
			}
		} else {
			if ( ok != NULL ) *ok = false;
		}
	}

//	 if ( !valueFound )
//		KMessageBox::sorry( 0, errMsg.arg( "Angle" ), i18n( "Could not Set Value" ) );


	return dmsAng;
}

dmsBox::~dmsBox(){
}
