/***************************************************************************
                          objectnamelist.h  -  description
                             -------------------
    begin                : Mon Feb 18 2002
    copyright            : (C) 2002 by Thomas Kabelmann
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

#ifndef OBJECTNAMELIST_H
#define OBJECTNAMELIST_H


/**@class ObjectNameList
	*This class provides an interface like a QPtrList, but sorts objects internally
	*in 27 lists. The objects are sorted alphabetically. List 0 contains all objects
	*beginning not with a letter. List 1 - 26 contains objects beginning with a letter.
	*The number of the list is similar to positon of letter in alphabet. (A = 1 .. Z = 26 )
	*@author Thomas Kabelmann
	*@version 1.0
	*/


#include <qglobal.h>
#include <qptrlist.h>
#include <qstring.h>

class SkyObject;
class SkyObjectName;

/**Reimplemented from QPtrList for sorting objects in the list. */
template <class T> class SortedList : public QPtrList <T> {
 protected:
  int compareItems(QPtrCollection::Item item1, QPtrCollection::Item item2) {
    if ( *((T*)item1) == *((T*)item2) ) return 0;
    return ( *((T*)item1) < *((T*)item2) ) ? -1 : 1;
  }
};

class ObjectNameList {

	public:
	/** Constructor */
		ObjectNameList();

	/** Destructor */
		~ObjectNameList();

	/**Appends a skyobject to the list.
		*@param object pointer to the SkyObject to be appended to the list.
		*@param useLongName if TRUE, use the longname of the object, 
		*rather than the primary name
		*/
		void append(SkyObject *object, bool useLongName=false);

	/**Select the list which contains objects whose names begin with the 
		*same letter as the argument.  The selected list ID is recorded in 
		*an internal variable, so it is persistent.
		*@note This function is case insensitive.
		*@note if no string is given, then the first list is selected, and 
		*also a flag is set so that all lists will be traversed by the next() 
		*function. 
		*@return pointer to the first object in the selected list.
		*@param name the name to use in selecting a list.
		*/
		SkyObjectName* first(const QString &name = QString::null);

	/**Returns next object in the currently selected list.
		*@return pointer to the next object in the current list, or NULL if 
		*the end of the list was reached.
		*@note if the "all lists" flag is set, then it will traverse all lists
		*before returning NULL.
		*/
		SkyObjectName* next();

	/**@return pointer to the object, if it is in one of the lists; 
		*otherwise return the NULL pointer.
		*@note This function is case sensitive.
		*@param name name of object to find. 
		*@return pointer to object with the given name
		*/
		SkyObjectName* find(const QString &name = QString::null);

	/**@short remove the named object from the list.
		*@param name the name of the object to be removed.
		*@note If the object is not found, then nothing happens.
		*/
		void remove( const QString &name = QString::null );

	/**Define the language which should be used for constellation names
		*/
		enum Language { local = 0, latin = 1 };

	/**Change constellation-name language option.
		*@param lang the language to use (0=local; 1=latin)
		*/
		void setLanguage( Language lang );

	/**Change constellation-name language option.
		*This function behaves just like the above function; 
		*it differs only in the data type of its argument.
		*@param lang the language to use (0=local; 1=latin)
		*/
		void setLanguage( bool lang );

		/**@return the total count of the number of named objects.
			*/
		uint count() const { return amount; }

	private:

	/**Sorts the lists with objects for faster access.
		*It's needed for find(). first() and find() call this function.
		*/
		void sort();


	/**@return the list index which contains the object whose name matches the argument. 
		*@note this does not return the position within a list of names; it returns the ID 
		*of the list itself.
		*@param name the name of the object whose index is to be found
		*/
		int getIndex( const QString &name = QString::null );

	/**Two modes are available:
		*allLists = loop through the whole list if next() is called
		*oneList = loop through one list if next is called
		*If oneList is set, just check all objects which start with same letter to save cpu time.
		*/
		enum Mode { allLists, oneList } mode;

	/**Set mode
		*@see first()
		*/
		void setMode( Mode m );

	/**Hold 2 pointer lists in memory. Number of lists can easily increased for adding more languages.
		*First index is the language and second index is reserved for alphabetically sorted list.
		* 0 = all objects beginning with a number
		* 1 = A ... 26 = Z
		*/
		SortedList <SkyObjectName> list[2][27];

		/**Hold status if list is unsorted or not.*/
		bool unsorted[27];

	/**
		*Constellations has latin names and alloc extra SkyObjectNames which will stored in 2 list.
		*But second list will not delete objects while clearing it, because most of objects in this list
		*are in first list too. We just have to delete objects which are not in first list. These objects
		*will stored in this list.
		*/
		QPtrList <SkyObjectName> constellations;

	/**
		*Which list was accessed last time by first() or next()
		*/
		int currentIndex;
		
		Language language;

		unsigned int amount;
};

#endif
