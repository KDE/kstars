/***************************************************************************
                          ksfilereader.cpp  -  description
                             -------------------
    begin                : 2007-07-16
    copyright            : (C) 2007 James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QApplication>
#include <QObject>
#include <QFile>

#include "kdebug.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"

#include "ksfilereader.h"

#ifndef MAXUINT 
    #define MAXUINT (~0) 
#endif 

KSFileReader::KSFileReader( qint64 maxLen ) : 
    QTextStream(), m_curLine(0), m_maxLen(maxLen), m_targetLine(MAXUINT)
{
}

KSFileReader::KSFileReader( QFile& file, qint64 maxLen ) : 
    QTextStream(), m_curLine(0), m_maxLen(maxLen), m_targetLine(MAXUINT)
{
    QIODevice* device = (QIODevice*) & file;
    QTextStream::setDevice( device );
	m_curLine = 0;
    m_targetLine = MAXUINT;
}

bool KSFileReader::open( const QString& fname )
{
    if (  !KSUtils::openDataFile( m_file, fname ) ) {
        kWarning() << QString("Couldn't open(%1)").arg(fname) << endl;
        return false;
    }
    QTextStream::setDevice( &m_file );
    return true;
}

void KSFileReader::setProgress( KStarsData* data, QString label, 
                                unsigned int totalLines, 
                                unsigned int numUpdates,
                                unsigned int firstNumUpdates)
{
    if ( firstNumUpdates == 0 ) firstNumUpdates = numUpdates;

    m_label = label;
    m_totalLines = totalLines;
    m_targetLine = m_totalLines / firstNumUpdates;
    m_targetIncrement = m_totalLines / numUpdates;

    connect( this, SIGNAL( progressText( const QString & ) ), 
	    data, SIGNAL( progressText( const QString & ) ) );

}

void KSFileReader::showProgress()
{
    if ( m_curLine < m_targetLine ) return;
    if ( m_targetLine < m_targetIncrement ) 
        m_targetLine = m_targetIncrement;
    else
        m_targetLine += m_targetIncrement;

    int percent = 1 + (m_curLine * 100) / m_totalLines;

    //kDebug() << m_label.arg( percent ) << endl;
    emit progressText( m_label.arg( percent ) );
    qApp->processEvents();
}


#include "ksfilereader.moc"
