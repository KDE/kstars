/***************************************************************************
                          starobject.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Sep 18 2001
    copyright            : (C) 2001 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "starobject.h"

#include <qpainter.h>
#include <qstring.h>
#include <kdebug.h>

StarObject::StarObject() : SkyObject(), SpType(""), soName( 0 )
{
}

StarObject::StarObject( StarObject &o )
	: SkyObject (o)
{
	SpType = o.SpType;
	soName = o.soName;
}

StarObject::StarObject( dms r, dms d, double m, QString n, QString n2, QString sptype )
	: SkyObject (0, r, d, m, n, n2, ""), SpType(sptype), soName( 0 )
{
	QString lname = "";
	if ( n.length() && n != i18n("star") ) {
		if ( n2.length() )
			lname = n + "(" + n2 + ")";
		else
			lname = n;
	} else if ( n2.length() )
		lname = n2;

	setLongName( lname );
}

StarObject::StarObject( double r, double d, double m, QString n, QString n2, QString sptype )
	: SkyObject (0, r, d, m, n, n2, ""), SpType(sptype), soName( 0 )
{
	QString lname = "";
	if ( n.length() && n != i18n("star") ) {
		if ( n2.length() )
			lname = n + "(" + n2 + ")";
		else
			lname = n;
	} else if ( n2.length() )
		lname = n2;

	setLongName( lname );
}

StarObject::StarObject( int t, dms r, dms d, double m, QString n, QString n2, QString lname, QString cat,
						double a, double b, int pa, int pgc, int ugc, QString sptype )
: SkyObject(t, r, d, m, n, n2, lname, cat, a, b, pa, pgc, ugc), SpType(sptype)
{
}

QString StarObject::sptype( void ) {
	return SpType;
}

QString StarObject::greekLetter( void ) {
	QString letter = "";
	if ( name2().find(" ") > 0 )
		letter = name2().mid( 0, name2().find(" ") );

	return letter;
}

void StarObject::drawLabel( QPainter &psky, int x, int y, int zoom, bool drawName, bool drawMag, double scale ) {
	QString sName("");
	if ( drawName ) {
		if ( name() != "star" ) sName = name() + " ";
		else if ( ! longname().isEmpty() ) sName = longname() + " ";
	}
	if ( drawMag ) {
		sName += QString().sprintf("%.1f", mag() );
	}
	
	int offset = int( scale * (6 + int(0.5*(5.0-mag())) + int(0.5*( zoom - 6)) ));
	
	psky.drawText( x+offset, y+offset, sName );
}

