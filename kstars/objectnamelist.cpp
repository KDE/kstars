/***************************************************************************
                          objectnamelist.cpp  -  description
                             -------------------
    begin                : Mon Feb 18 2002
    copyright          : (C) 2002 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "objectnamelist.h"

#include <klocale.h>
#include <kdebug.h>

ObjectNameList::ObjectNameList()
{
	language = latin;
	mode = allLists;

// this is needed to avoid memory leaks, but enable this code will
// crash kstars at closing window
// TK: I reviewed the code but I don't know why it is crashing :(
/**********************************
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 27; j++) {
			list[i][j].setAutoDelete(true);
		}
	}
***********************************/

}

ObjectNameList::~ObjectNameList(){
}

void ObjectNameList::setLanguage( Language lang ) {
	language = lang;
}

void ObjectNameList::setLanguage( bool lang ) {
	language = ( Language ) lang;
}

void ObjectNameList::setMode( Mode m ) {
	mode = m;
}

void ObjectNameList::append( SkyObject *object, bool useLongName ) {
	// create name string and init with longname if forced by parameter else default name
	QString name = ( useLongName ) ? object->longname() : object->name();

	// create string with translated name
	QString iName;

	if ( object->type() == -1 ) {  // constellation
		iName = i18n( "Constellation name (optional)", name.local8Bit().data() );
	}
	else {  // all other types
		iName = i18n( name.local8Bit() );
	}

	// create SkyObjectName with translated name
	SkyObjectName *soName = new SkyObjectName( iName, object );
	// append in localized list
	currentIndex = getIndex( iName );
	list[ local ] [ currentIndex ].append( soName );

	// type == -1 -> constellation
	if ( object->type() == -1 ) {
		// get latin name (default name)
		iName = name;
		// create new SkyObject with localized name
		soName = new SkyObjectName( iName, object );
	}

	// append in latin list
	currentIndex = getIndex( iName );
	list[ latin ] [ currentIndex ].append(  soName );
}

SkyObjectName* ObjectNameList::first( const QString &name ) {
	SkyObjectName *soName = 0;
	// set mode: string is empty set mode to all lists
	name.isEmpty() ? setMode( allLists ) : setMode( oneList );

	// start with first list in array
	if ( mode == allLists ) {
		currentIndex = 0;
	}
	else {
		// start with list which contains the first letter
		currentIndex = getIndex( name );
	}

	soName = list[ language ] [ currentIndex ].first();

	return soName;
}

SkyObjectName* ObjectNameList::next() {
	SkyObjectName *soName = 0;
	// get next SkyObjectName object
	soName = list[ language ] [ currentIndex ].next();

	// if all lists must checked and SkyObjectName is NULL
	// check next available list in array and set to first element in list
	// if currentIndex == 26 -> last index is reached
	if ( mode == allLists && !soName && currentIndex < 26 ) {
		do {
			currentIndex++;  // loop through the array
			soName = list[ language ] [ currentIndex ].first();
		} while ( currentIndex < 26 && !soName );  // break if currentIndex == 27 or soName is found
	}

	return soName;
}

int ObjectNameList::getIndex( const QString &name ) {
	//	default index is 0 if object name starts with a number
	int index = 0;

	// if object name starts with a letter, so get index number between 1 and 26
	if ( !name.isEmpty() ) {
		QChar firstLetter = name[0];
		if ( firstLetter ) {
			if ( firstLetter.isLetter() ) {
				const unsigned char letter = (unsigned char) firstLetter.lower();
				index = letter % 96;  // a == 97 in ASCII code => 97 % 96 = 1
			}

			/**
				*Avoid invalid index due to non ASCII letters like "ö" etc. Add your own letters to put them in
				*the right list.
				*/
			if (index > 26) {
				switch (index) {
					case 54 : index = 15; break;  // ö == o
					default : index = 0;						// all other letters
				}
				kdDebug() << k_funcinfo << "Object: " << name << " starts with non ASCII letter. Put it in list #" << index << endl;
			}
		}
	}

	return index;
}
