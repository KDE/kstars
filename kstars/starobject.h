/***************************************************************************
                          starobject.h  -  K Desktop Planetarium
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




#ifndef STAROBJECT_H
#define STAROBJECT_H

#include "skyobject.h"

class SkyObjectName;

/**
  *@author Thomas Kabelmann
  */

// StarObject is a subclass of SkyObject and has implemented the color of the star stored in QChar
class StarObject : public SkyObject {
	public:
		StarObject();

		StarObject(StarObject & o);

	  	StarObject( int t, dms r, dms d, double m, QString n="unnamed",
  					 		QString n2="", QString lname="", QString SpType="" );

  		StarObject( int t, double r, double d, double m, QString n="unnamed",
  					 		QString n2="", QString lname="", QString SpType="" );

		~StarObject();

		QChar color() { return SpType.at(0); }
		QString sptype() { return SpType; }
		
//		void setSkyObjectName( SkyObjectName *n ) { soName = n; }
//		SkyObjectName *skyObjectName() { return soName; }
		
	private:
		QString SpType;
		
		SkyObjectName *soName;
};

#endif
