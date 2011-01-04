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

#include <tqstring.h>
#include <klistbox.h>
#include <klocale.h>

/**@class SkyObjectName
	*Convenience class which tqcontains a SkyObject's name and a pointer to the SkyObject
	*itself.  This class is used to construct the List of named objects that may be
	*located with the FindDialog.
	*@short convenience class for indexing SkyObjects by name.
	*@author Thomas Kabelmann
	*@version 1.0
	*/

class SkyObject;

class SkyObjectName {
	
	public:
	/**Constructor*/
		SkyObjectName (const TQString &str = TQString::null, SkyObject *obj = 0);

	/**Destructor (empty)*/
		~SkyObjectName() {}

	/**@return the name of the SkyObject*/
		TQString text() { return Text; }

	/**@return translated version of the SkyObject's name*/
		TQString translatedText() { return i18n( Text.local8Bit().data()); }

	/**@return pointer to the SkyObject*/
		SkyObject *skyObject() { return skyobject; }

	/**Comparison operator, needed for sorting.
		*/
		bool operator < (SkyObjectName &o) { return Text < o.Text; }

	/**Equivalence operator, needed for sorting.
		*/
		bool operator == (SkyObjectName &o) { return Text == o.Text; }

	private:
	
		SkyObject *skyobject;
		TQString Text;
};


/**Class for filling list of named objects in the Find Object dialog (FindDialog).
	*The class is derived from TQListBoxText, and adds a SkyObjectName* member variable,
	*and a method to retrieve this variable (a pointer).  This makes it very easy
	*to add these items to the FindDialog's TQListBox, and to sort and filter them.
	*@short Derivative of TQListBoxItem specifically for SkyObjects
	*@author Thomas Kabelmann
	*@version 0.9
	*/

class SkyObjectNameListItem : public TQListBoxText  {
	
	public:
	/**Constructor */
		SkyObjectNameListItem (TQListBox *parent, SkyObjectName *name );

	/**Destructor (empty)*/
		~SkyObjectNameListItem() {}

	/**@returns pointer to SkyObjectName associated with this SkyObjectNameListItem */
		SkyObjectName * objName() { return object; }
		
	private:
		SkyObjectName *object;
};

#endif
