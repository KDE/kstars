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


CultureList::CultureList()
{
    KSFileReader fileReader;
    if ( ! fileReader.open( "cnames.dat" ) )
        return;

    while ( fileReader.hasMoreLines() ) {
        QString line = fileReader.readLine();
        if ( line.size() < 1 ) continue;

        QChar mode = line.at( 0 );
        if ( mode == 'C' )
            m_CultureList << line.mid( 2 ).trimmed();
    }

    m_CultureList.sort();
    m_CurrentCulture = m_CultureList.at( Options::skyCulture() );
}

void CultureList::setCurrent ( QString newName ) {
    m_CurrentCulture = newName;
}

QString CultureList::getName( int index ) const
{
    return m_CultureList.value( index );
}
