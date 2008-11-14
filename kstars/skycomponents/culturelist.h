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

/*
*@class CultureList
*A list of all cultures

*@author Jerome.sonrier
*@version 0.1
*/

class CultureList : public QStringList
{
public:
    /* @short Constructor
    */
    CultureList();

    /* @short Destructor
    */
    ~CultureList();

    /* @short Return the current sky culture
    */
    QString current();

    /* @short Set the current culture name
    */
    void setCurrent( QString newName );

    /* @short Return a sorted list of cultures
    */
    QStringList getNames();

    /* @short Return the name of the culture at index
    */
    QString getName( int index );


private:
    QString m_CurrentCulture;
};


#endif
