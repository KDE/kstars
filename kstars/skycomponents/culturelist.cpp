/***************************************************************************
               culturelist.cpp  -  K Desktop Planetarium
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
 
#include "culturelist.h"
#include "ksfilereader.h"
#include "Options.h"


CultureList::CultureList() : QStringList()
{
    QChar mode;
    QString line, cultureName;
    QStringList names;
    KSFileReader fileReader;

    // Initialyse the list of cultures
    if ( ! fileReader.open( "cnames.dat" ) ) return;

    while ( fileReader.hasMoreLines() ) {
        line = fileReader.readLine();
        if ( line.size() < 1 ) continue;

        mode = line.at( 0 );

        if ( mode == 'C' ) {
            append( line.mid( 2 ).trimmed() );
        }
    }

    // Initialise current culture
    for (int i = 0; i < size(); ++i) {
        names.append( at(i) );
    }

    names.sort();
    m_CurrentCulture = names.at( (int)Options::skyCulture() );
}

CultureList::~CultureList()
{}

QString CultureList::current() {
    return m_CurrentCulture;
}

void CultureList::setCurrent ( QString newName ) {
    m_CurrentCulture = newName;
}

QStringList CultureList::getNames() {
    QStringList names;

    for (int i = 0; i < size(); ++i) {
        names.append( at(i) );
    }

    names.sort();

    return names;
}

QString CultureList::getName( int index ) {
    QStringList names;
    QString name;
    int i;

    for (i = 0; i < size(); ++i) {
        names.append( at(i) );
    }

    names.sort();
    name = names.at( index );

    return name;
}

