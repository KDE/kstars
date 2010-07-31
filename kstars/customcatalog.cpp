/***************************************************************************
                          customcatalog.cpp  -  description
                             -------------------
    begin                : Fri May 3 2005
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

#include <tqstring.h>

#include "customcatalog.h"

CustomCatalog::CustomCatalog() : m_Name( i18n("Custom") ), m_Prefix( "CC" ), 
		m_Color( "#FF0000" ), m_Epoch( 2000.0 ), m_ObjList() {
}

CustomCatalog::CustomCatalog( TQString nm, TQString px, TQString co, float ep, 
		TQPtrList<SkyObject> ol ) : m_Name(nm), m_Prefix(px), m_Color(co), 
		m_Epoch(ep), m_ObjList( ol ) {

}

CustomCatalog::~CustomCatalog() {
}

