/***************************************************************************
                          skyobject.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001 by Jason Harris
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

#include "skyobject.h"
#include "dms.h"

SkyObject::SkyObject(){
	type = 0;
	Position.set( 0.0, 0.0 );
  mag = 0.0;
  name = "unnamed";
  name2 = "";
  longname = "";
}

SkyObject::SkyObject( SkyObject &o ) {
	type = o.type;
	Position = *o.pos();
  mag = o.mag;
  name = o.name;
	name2 = o.name2;
	longname = o.longname;
	ImageList = o.ImageList;
	ImageTitle = o.ImageTitle;
	InfoList = o.InfoList;
	InfoTitle = o.InfoTitle;
}

SkyObject::SkyObject( int t, dms r, dms d, double m, QString n,
                      QString n2, QString lname ) {
  type = t;
	Position.setRA0( r );
	Position.setDec0( d );
	Position.setRA( r );
	Position.setDec( d );
  mag = m;
  name = n;
  name2 = n2;
  longname = lname;
}

SkyObject::SkyObject( int t, double r, double d, double m, QString n,
                      QString n2, QString lname ) {
  type = t;
	Position.setRA0( r );
	Position.setDec0( d );
	Position.setRA( r );
	Position.setDec( d );
  mag = m;
  name = n;
  name2 = n2;
  longname = lname;
}

