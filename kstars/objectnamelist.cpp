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
#include "skyobjectname.h"
#include "skyobject.h"
#include "starobject.h"

#include <qstring.h>
#include <kdebug.h>

ObjectNameList::ObjectNameList() {
	amount = 0;
	language = latin;
	mode = allLists;

	// delete just objects of local list
	for (int i= 0; i< 27; i++) {
		list[local][i].setAutoDelete(true);
		unsorted[i] = false;
	}

	constellations.setAutoDelete(true);
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
	amount++;
	// create name string and init with longname if forced by parameter else default name
	QString name = ( useLongName ) ? object->longname() : object->name();

	//if star's name is it's genetive name, make sure we don't use the Greek charcter here
	if ( object->type() == 0 && name == ((StarObject*)object)->gname() )
		name = ((StarObject*)object)->gname( false );
	
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
	currentIndex = getIndex( name );
	list[local] [currentIndex].append(soName);

	// type == -1 -> constellation
	if (object->type() == -1) {
		// get latin name (default name)
		iName = name;
		// create new SkyObject with localized name
		soName = new SkyObjectName(iName, object);
		// to delete these objects store them in separate list
		constellations.append(soName);
	}

	// append in latin list
	currentIndex = getIndex(name);
	list[latin][currentIndex].append(soName);
	// set list unsorted
	unsorted[currentIndex] = true;
}

SkyObjectName* ObjectNameList::first( const QString &name ) {
	sort();
	SkyObjectName *soName = 0;
	// set mode: string is empty set mode to all lists
	name.isEmpty() ? setMode( allLists ) : setMode( oneList );

	// start with first list in array
	if ( mode == allLists ) {
		currentIndex = 0;
	} else {
		// start with list which contains the first letter
		currentIndex = getIndex( name );
	}

	soName = list[language][currentIndex].first();

	//It's possible that there is no object that belongs to currentIndex
	//If not, and mode==allLists, try the next index
	while ( !soName && mode==allLists && currentIndex < 26 ) {
		currentIndex++;  // loop through the array
		soName = list[language][currentIndex].first();
  }

	return soName;
}

SkyObjectName* ObjectNameList::next() {
	SkyObjectName *soName = 0;
	// get next SkyObjectName object
	soName = list[ language ] [ currentIndex ].next();

	// if all lists must checked and SkyObjectName is NULL
	// check next available list in array and set to first element in list
	// if currentIndex == 26 -> last index is reached
	if ( mode==allLists && soName==0 && currentIndex<26 ) {
		do {
			currentIndex++;  // loop through the array
			soName = list[ language ] [ currentIndex ].first();
		} while ( currentIndex<26 && soName==0 );  // break if currentIndex == 27 or soName is found
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
				*Avoid invalid index due to non ASCII letters like "� etc. Add your own letters to put them in
				*the right list (due to %96 index can't never get smaller than 0).
				*/
			if (index > 26) {
				switch (index) {
					case 41 : index = 5; break;	// �= e
					case 54 : index = 15; break;	// �= o
					default : index = 0;						// all other letters
				}
				kdDebug() << k_funcinfo << "Object: " << name << " starts with non ASCII letter. Put it in list #" << index << endl;
			}
		}
	}

	return index;
}

void ObjectNameList::sort() {
	for (int i=0; i<27; ++i) {
		if (unsorted[i] == true) {
			unsorted[i] = false;
			list[latin][i].sort();
			list[local][i].sort();
		}
	}
}

void ObjectNameList::remove ( const QString &name ) {
	setMode(oneList);
	int index = getIndex(name);
	SortedList <SkyObjectName> *l = &(list[language][index]);

	SkyObjectName *son = find( name );
	if ( son ) l->remove( son );
}

SkyObjectName* ObjectNameList::find(const QString &name) {
	sort();
	if (name.isNull()) return 0;
	// find works only in one list and not in all lists
	setMode(oneList);

	// items are stored translated (JH: Why?  this whole class is confusing...)
	QString translatedName = i18n(name.utf8());

	int index = getIndex(name);

	// first item
	int lower = 0;
	SortedList <SkyObjectName> *l = &(list[language][index]);
	// last item
	int upper = l->count() - 1;
	// break if list is empty
	if (upper == -1) return 0;

	int next;

	// it's the "binary search" algorithm
	SkyObjectName *o;
	while (upper >= lower) {
		next = (lower + upper) / 2;
		o = l->at(next);
		if (translatedName == o->text()) { return o; }
		if (translatedName < o->text())
			upper = next - 1;
		else
			lower = next + 1;
	}
	return 0;
}
