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
#include <klocale.h>
#include <qregexp.h>
#include <qwhatsthis.h>

dmsBox::dmsBox(QWidget *parent, const char *name, bool dg) : KLineEdit(parent,name) {
	setMaxLength(14);
	setMaximumWidth(160);
	deg = dg;

	if ( deg )
		QWhatsThis::add( this, i18n( "Enter an angle value in degrees.  The angle can be expressed as a simple integer (\"45\") or floating-point (\"45.333\") value, or as space- or colon-delimited values specifying degrees, arcminutes and arcseconds (\"45:20\", \"45:20:00\", \"45:20\", \"45 20.0\", etc.)." ) );
	else 
		QWhatsThis::add( this, i18n( "Enter an angle value in hours.  The angle can be expressed as a simple integer (\"12\") or floating-point (\"12.333\") value, or as space- or colon-delimited values specifying hours, minutes and seconds (\"12:20\", \"12:20:00\", \"12:20\", \"12 20.0\", etc.)." ) );
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
