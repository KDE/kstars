/***************************************************************************
                          customcatalog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Fri 3 Jun 2005
    copyright            : (C) 2005 by Jason Harris
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

#ifndef CUSTOM_CATALOG_H
#define CUSTOM_CATALOG_H

#include <tqptrlist.h>
#include "skyobject.h"

class QString;
 
/**@class CustomCatalog
	*@short Object catalog added by the user.
	*/
class CustomCatalog {
public:
	CustomCatalog();
	CustomCatalog( TQString nm, TQString px, TQString co, float ep, TQPtrList<SkyObject> ol );
	~CustomCatalog();

	TQString name() const { return m_Name; }
	TQString prefix() const { return m_Prefix; }
	TQString color() const { return m_Color; }
	float epoch() const { return m_Epoch; }
	TQPtrList<SkyObject> objList() const { return m_ObjList; }

	void setName( const TQString &name ) { m_Name = name; }
	void setPrefix( const TQString &prefix ) { m_Prefix = prefix; }
	void setColor( const TQString &color ) { m_Color = color; }
	void setEpoch( float epoch ) { m_Epoch = epoch; }
	void setObjectList( TQPtrList<SkyObject> ol ) { m_ObjList = ol; }

private:
	TQString m_Name, m_Prefix, m_Color;
	float m_Epoch;
	TQPtrList<SkyObject> m_ObjList;
};

#endif //CUSTOM_CATALOG_H
