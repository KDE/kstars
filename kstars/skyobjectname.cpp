/***************************************************************************
                          skyobjectname.cpp  -  description
                             -------------------
    begin                : Wed Aug 22 2001
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

#include "skyobjectname.h"
#include "skyobject.h"

SkyObjectName::SkyObjectName( const QString &str, SkyObject *obj )
	: skyobject ( obj ), Text ( str )
{
}

SkyObjectNameListItem::SkyObjectNameListItem ( QListBox *parent, SkyObjectName *obj )
	: QListBoxText ( parent ), object ( obj )
{
	setText( obj->text() );
}


