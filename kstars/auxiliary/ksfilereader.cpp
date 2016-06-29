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

#include "ksfilereader.h"

#include <QApplication>
#include <QObject>
#include <QFile>

#include <QDebug>
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"

KSFileReader::KSFileReader( qint64 maxLen ) :
        QTextStream(), m_maxLen(maxLen), m_curLine(0), m_targetLine(UINT_MAX)
{}

KSFileReader::KSFileReader( QFile& file, qint64 maxLen ) :
        QTextStream(), m_maxLen(maxLen), m_curLine(0), m_targetLine(UINT_MAX)
{
    QIODevice* device = (QIODevice*) & file;
    QTextStream::setDevice( device );
    QTextStream::setCodec("UTF-8");
    m_targetLine = UINT_MAX;
}

bool KSFileReader::open( const QString& fname )
{
    if (  !KSUtils::openDataFile( m_file, fname ) ) {
        qWarning() << QString("Couldn't open(%1)").arg(fname);
        return false;
    }
    QTextStream::setDevice( &m_file );
    QTextStream::setCodec("UTF-8");
    return true;
}

bool KSFileReader::openFullPath( const QString& fname )
{
    if ( !fname.isNull() ) {
        m_file.setFileName( fname );
        if ( !m_file.open( QIODevice::ReadOnly ))
          return false;
    }
    QTextStream::setDevice( &m_file );
    QTextStream::setCodec("UTF-8");
    return true;
}

void KSFileReader::setProgress( QString label,
                                unsigned int totalLines,
                                unsigned int numUpdates )
{
    m_label = label;
    m_totalLines = totalLines;
    if ( m_totalLines < 1 ) m_totalLines = 1;
    m_targetLine = m_totalLines / 100;
    m_targetIncrement = m_totalLines / numUpdates;

    connect( this, SIGNAL( progressText( const QString & ) ),
             KStarsData::Instance(), SIGNAL( progressText( const QString & ) ) );
}

void KSFileReader::showProgress()
{
    if ( m_curLine < m_targetLine ) return;
    if ( m_targetLine < m_targetIncrement )
        m_targetLine = m_targetIncrement;
    else
        m_targetLine += m_targetIncrement;

    int percent = int(.5 + (m_curLine * 100.0) / m_totalLines);
    //printf("%8d %8d %3d\n", m_curLine, m_totalLines, percent );
    if ( percent > 100 ) percent = 100;
    emit progressText( QString("%1 (%2%)").arg( m_label ).arg( percent ) );
//#ifdef ANDROID
// Can cause crashes on Android
    qApp->processEvents();
//#endif
}



