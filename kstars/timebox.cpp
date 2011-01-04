/***************************************************************************
                          timebox.cpp  -  description
                             -------------------
    begin                : Sun Jan 20 2002
    copyright            : (C) 2002 by Pablo de Vicente
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

#include "timebox.h"
#include "libkdeedu/extdate/extdatetime.h"
#include <tqdatetime.h>  //needed for QTime
#include <tqregexp.h>
#include <kglobal.h>
#include <klocale.h>
#include <stdlib.h>
#include <kdebug.h>

timeBox::timeBox(TQWidget *parent, const char *name, bool tt) : TQLineEdit(parent,name) 
{ 

	if (tt) { 
		setMaxLength(9);
		setMaximumWidth(90);
	}
	else {
		setMaxLength(12);
		setMaximumWidth(120);
	}

	timet = tt;
}

void timeBox::showTime (TQTime t)
{
	setEntry( t.toString("hh:mm:ss") );
}

void timeBox::showDate (ExtDate t)
{
	setEntry( t.toString() );

}

TQTime timeBox::createTime ( bool *ok )
{
//	TQString entry;
	int h = 0, m = 0, is = 0;
	double s = 0.0;
	TQTime qt;
	bool valueFound = false, badEntry = false , checkValue = false;

//Initialize bool for result
	if ( ok != NULL ) *ok = false;

//	TQString errMsg = i18n( "Unable to parse %1 entry. Specify a %1 value as a simple integer, a floating-point number, or a triplet of values using colons or spaces as separators." );

	TQString entry = text().stripWhiteSpace();

	//Try simplest cases: integer or double representation

	h = entry.toInt( &checkValue );
	if ( checkValue ) {
		qt = TQTime( h, 0, 0 );
		valueFound = true;
		if ( ok != NULL ) *ok = true;
		return qt;
	} else {
		double x = entry.toDouble( &checkValue );
		if ( checkValue ) {
			int seconds = int(x * 3600);
			TQTime qt(0,0,0);
			qt.addSecs(seconds);
			valueFound = true;
			if ( ok != NULL ) *ok = true;
			return qt;
		}
	}

	//no success yet...try assuming multiple fields

	if ( !valueFound ) { 
		TQStringList fields;
		
		//check for colon-delimiters or space-delimiters
		if ( entry.tqcontains(':') ) 
			fields = TQStringList::split( ':', entry );
		else fields = TQStringList::split( " ", entry ); 

		// If two fields we will add a third one, and then parse with 
		// the 3-field code block. If field[1] is a double, convert 
		// it to integer mins, and convert the remainder to secs
		 
		if ( fields.count() == 2 ) {
			double mx = fields[1].toDouble( &checkValue );
			if ( checkValue ) {
				fields[1] = TQString("%1").arg( int(mx) );
				fields.append( TQString("%1").arg( int( 60.0*(mx - int(mx)) ) ) );
			} else {
				fields.append( TQString( "0" ) );
			}
		}
		
		// Three fields space-delimited ( h/d m s ); 
		// ignore all after 3rd field

		if ( fields.count() >= 3 ) {
			fields[0].tqreplace( TQRegExp("h"), "" );
			fields[1].tqreplace( TQRegExp("m"), "" );
			fields[2].tqreplace( TQRegExp("s"), "" );
		}
		//See if first two fields parse as integers.
		//
		h = fields[0].toInt( &checkValue );
		if ( !checkValue ) badEntry = true;
		m = fields[1].toInt( &checkValue );
		if ( !checkValue ) badEntry = true;
		s = fields[2].toDouble( &checkValue );
		if ( !checkValue ) badEntry = true;

		if ( !badEntry ) {
			valueFound = true;
			is = (int)s;

			if ( ok != NULL ) *ok = true;

			TQTime qt(h,m,is);
			return qt;

		} else {
			if ( ok != NULL ) *ok = false;
		}
	}

//	 if ( !valueFound )
//		KMessageBox::sorry( 0, errMsg.arg( "Angle" ), i18n( "Could Not Set Value" ) );


	return qt;
}


ExtDate timeBox::createDate (bool */*ok*/)
{
	
	TQString entry = text().stripWhiteSpace();

	// if entry is an empty string or invalid date use current date

	ExtDate date = ExtDate().fromString(entry);

	if ( !date.isValid() ) {
		kdDebug() << k_funcinfo << "Invalid date" << endl;
		showDate(ExtDate::tqcurrentDate());
		entry = text().stripWhiteSpace();
		return ExtDate::tqcurrentDate();
	} else {
		return date;
	}
}

timeBox::~timeBox(){
}
