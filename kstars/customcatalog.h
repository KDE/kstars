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

#include <qptrlist.h>
#include "skyobject.h"

class QString;
 
/**@class CustomCatalog
	*@short Object catalog added by the user.
	*/
class CustomCatalog {
public:
	CustomCatalog();
	CustomCatalog( QString nm, QString px, QString co, float ep, QPtrList<SkyObject> ol );
	~CustomCatalog();

	QString name() const { return m_Name; }
	QString prefix() const { return m_Prefix; }
	QString color() const { return m_Color; }
	float epoch() const { return m_Epoch; }
	QPtrList<SkyObject> objList() const { return m_ObjList; }

	void setName( const QString &name ) { m_Name = name; }
	void setPrefix( const QString &prefix ) { m_Prefix = prefix; }
	void setColor( const QString &color ) { m_Color = color; }
	void setEpoch( float epoch ) { m_Epoch = epoch; }
	void setObjectList( QPtrList<SkyObject> ol ) { m_ObjList = ol; }

private:
	QString m_Name, m_Prefix, m_Color;
	float m_Epoch;
	QPtrList<SkyObject> m_ObjList;
};

#endif //CUSTOM_CATALOG_H
