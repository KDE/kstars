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

StarObject::StarObject() : SkyObject(), SpType(""), soName( 0 )
{
}

StarObject::StarObject( StarObject &o )
	: SkyObject (o)
{
	SpType = o.SpType;
	soName = o.soName;
}

StarObject::StarObject( dms r, dms d, double m, QString n, QString n2, QString st )
	: SkyObject (0, r, d, m, n, n2, ""), SpType(st), soName( 0 )
{
}

StarObject::StarObject( double r, double d, double m, QString n, QString n2, QString st )
	: SkyObject (0, r, d, m, n, n2, ""), SpType(st), soName( 0 )
{
}

StarObject::~StarObject()
{
// destructor is empty
}

QString StarObject::greekLetter( void ) {
	QString letter = "";
	if ( name2.find(" ") > 0 )
		letter = name2.mid( 0, name2.find(" ") );

	return letter;
}
