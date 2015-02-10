/***************************************************************************
               culturelist.h  -  K Desktop Planetarium
                             -------------------
    begin                : 04 Nov. 2008
    copyright            : (C) 2008 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CULTURELIST_H
#define CULTURELIST_H

#include <QStringList>

/** @class CultureList
 * A list of all cultures
 * FIXME: Why not use a QStringList?
 */
class CultureList
{
public:
    /** @short Create culture list and load its conternt from file */
    CultureList();

    /** @short Return the current sky culture */
    QString current() const { return m_CurrentCulture; }

    /** @short Set the current culture name */
    void setCurrent( QString newName );

    /** @short Return a sorted list of cultures */
    QStringList getNames() const { return m_CultureList; }

    /** @short Return the name of the culture at index.
     *  @return null string if is index is out of range */
    QString getName( int index ) const;

private:
    QString     m_CurrentCulture;
    // List of all available cultures. It's assumed that list is sorted.
    QStringList m_CultureList;
};


#endif
