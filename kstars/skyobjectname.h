/***************************************************************************
                          skyobjectname.h  -  description
                             -------------------
    begin                : Wed Aug 22 2001
    copyright            : (C) 2001 by Thomas Kabelmann
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

#ifndef SKYOBJECTNAME_H
#define SKYOBJECTNAME_H

#include <qstring.h>
#include <klistbox.h>

#include "skyobject.h"

/**
  *@author Thomas Kabelmann
  */

class SkyObjectName {
	
	public:
		SkyObjectName (const char *str = 0, SkyObject *obj = 0);
		~SkyObjectName();

		QString text() { return Text; }
		SkyObject *skyObject() { return skyobject; }
		
	private:
	
		SkyObject *skyobject;
		QString Text;
};


class SkyObjectNameListItem : public QListBoxText  {
	
	public:
		SkyObjectNameListItem (QListBox *parent, SkyObjectName *name, bool useLatinConstellNames = true);
		~SkyObjectNameListItem();

		SkyObjectName * objName() { return object; }
		
	private:
		SkyObjectName *object;
};

#endif
